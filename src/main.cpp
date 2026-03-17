#include <crow.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <mutex>
#include <unordered_set>
#include <openssl/sha.h>
#include "wordlists.hpp"     // from above
#include "apikeys.hpp"       // your api key validator (already)
#include "GameTracker.hpp"       // GameTracker for Scores

using json = nlohmann::json;
struct Policy {
    int minLen = 3;
    int maxLen = 16;
    bool allowSpaces = false;
    bool allowUnderscore = true;
    int reloadSeconds = 5;
};

/* ============================================================
   API KEY STORAGE
   ============================================================ */

static std::unordered_set<std::string> g_allowedKeys;
static std::mutex g_keyMutex;
static const std::string API_KEY_FILE = "config/apikeys.json";

std::string Sha256Hex(const std::string& input)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256(reinterpret_cast<const unsigned char*>(input.data()), input.size(), hash);

    static const char* hex = "0123456789abcdef";
    std::string out;
    out.resize(SHA256_DIGEST_LENGTH * 2);

    for (size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i)
    {
        out[i * 2]     = hex[(hash[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex[(hash[i]) & 0xF];
    }
    return out;
}

void LoadApiKeys()
{
    std::ifstream f(API_KEY_FILE);
    if (!f.is_open())
    {
        std::cerr << "WARNING: Could not open apikeys.json\n";
        return;
    }

    json j;
    f >> j;

    std::unordered_set<std::string> newKeys;

    if (j.contains("keys_sha256_hex"))
    {
        for (auto& k : j["keys_sha256_hex"])
        {
            if (k.is_string())
                newKeys.insert(k.get<std::string>());
        }
    }

    std::lock_guard<std::mutex> lock(g_keyMutex);
    g_allowedKeys = std::move(newKeys);

    std::cout << "Loaded " << g_allowedKeys.size() << " API keys\n";
}

bool IsApiKeyValid(const std::string& apiKey)
{
    if (apiKey.empty())
        return false;

    std::string hash = Sha256Hex(apiKey);

    std::lock_guard<std::mutex> lock(g_keyMutex);
    return g_allowedKeys.find(hash) != g_allowedKeys.end();
}


/* loading word files and check */
static bool FormatAllowedAscii(const std::string& raw, const Policy& p, std::string& why) {
    if ((int)raw.size() < p.minLen) { why = "TOO_SHORT"; return false; }
    if ((int)raw.size() > p.maxLen) { why = "TOO_LONG"; return false; }
    for (unsigned char c : raw) {
        if (std::isalnum(c)) continue;
        if (c == '_' && p.allowUnderscore) continue;
        if (c == ' ' && p.allowSpaces) continue;
        why = "INVALID_CHAR";
        return false;
    }
    return true;
}

static Policy LoadPolicy(const std::string& path) {
    Policy p;
    std::ifstream f(path);
    if (!f.is_open()) return p;
    json j; try { f >> j; } catch (...) { return p; }
    if (!j.is_object()) return p;
    p.minLen = j.value("minLen", p.minLen);
    p.maxLen = j.value("maxLen", p.maxLen);
    p.allowSpaces = j.value("allowSpaces", p.allowSpaces);
    p.allowUnderscore = j.value("allowUnderscore", p.allowUnderscore);
    p.reloadSeconds = j.value("reloadSeconds", p.reloadSeconds);
    return p;
}


/* ============================================================
   MAIN
   ============================================================ */

int main()
{
    std::cout << "KhorNameShieldService starting...\n";

    LoadApiKeys();

    Policy policy = LoadPolicy("config/policy.json");
    WordLists lists("config/lists");

    crow::SimpleApp app;

    CROW_ROUTE(app, "/health").methods("GET"_method)
    ([] {
        return crow::response(200, R"({"ok":true,"service":"KhorNameShieldService"})");
    });

    CROW_ROUTE(app, "/NameCheck").methods("POST"_method)
    ([&](const crow::request& req) {

        const std::string apiKey = req.get_header_value("X-Api-Key");

        if (!IsApiKeyValid(apiKey))
        {
            return crow::response(403, R"({"ok":false,"error":"API_KEY_INVALID"})");
        }
        // --- Hot reload lists (and optionally policy too) ---
        lists.MaybeReload(policy.reloadSeconds);
        // If you want policy hot reload too, re-read policy.json here occasionally.

        // --- Parse JSON body ---
        json in;
        try { in = json::parse(req.body); }
        catch (...) { return crow::response(400, R"({"ok":false,"error":"INVALID_JSON"})"); }

        std::string name = in.value("name", "");
        if (name.empty()) return crow::response(400, R"({"ok":false,"error":"NAME_REQUIRED"})");

        // --- Format rules ---
        std::string why;
        if (!FormatAllowedAscii(name, policy, why)) {
            json out = {
                {"ok", false},
                {"severity", "medium"},
                {"reasons", json::array({ "FORMAT_" + why })},
                {"suggestions", json::array({ "Use 3–16 characters with letters/numbers (and underscore if enabled)." })}
            };
            return crow::response(200, out.dump());
        }

        // --- Normalize + list check ---
        auto norm = NormalizeNameForScan(name);
        auto hit = lists.Check(norm.scanKey);

        json out;
        out["ok"] = hit.ok;
        out["severity"] = SeverityStr(hit.severity);
        out["reasons"] = hit.reasons;

        out["normalized"] = {
            {"unicodeFolded", norm.unicodeFolded},
            {"leetFolded", norm.leetFolded},
            {"scanKey", norm.scanKey}
        };

        if (!hit.ok) {
            out["suggestions"] = json::array({
                "Choose a different name.",
                "Avoid staff-like words, hate/extremism references, and offensive terms."
            });
        }

        return crow::response(200, out.dump());
    });


    CROW_ROUTE(app, "/GameTracker/SendScore").methods("POST"_method)
    ([&](const crow::request& req) {

        const std::string apiKey = req.get_header_value("X-Api-Key");

        if (!IsApiKeyValid(apiKey))
        {
            std::cout << "Error: Received invalid customer key " << apiKey << std::endl;
            return crow::response(403, R"({"ok":false,"error":"API_KEY_INVALID"})");
        }else{
            std::cout << "Received valid customer key " << std::endl;
            return crow::response(200, SendScore(req.body));
        }
    });

    CROW_ROUTE(app, "/GameTracker/NewGame").methods("POST"_method)
    ([&](const crow::request& req) {

        const std::string apiKey = req.get_header_value("X-Api-Key");

        if (!IsApiKeyValid(apiKey))
        {
            std::cout << "Error: Received invalid customer key " << apiKey << std::endl;
            return crow::response(403, R"({"ok":false,"error":"API_KEY_INVALID"})");
        }else{
            std::cout << "Received valid customer key " << std::endl;
            return crow::response(200, NewGame(req.body));
        }
    });

    int Port= 8188;
    std::cout << "Starting on port:" << Port << std::endl;
    app.bindaddr("127.0.0.1").port(Port).multithreaded().run();

    
    return 0;
}