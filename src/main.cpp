#include <crow.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <fstream>
#include <mutex>
#include <unordered_set>
#include <openssl/sha.h>

using json = nlohmann::json;

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

/* ============================================================
   MAIN
   ============================================================ */

int main()
{
    std::cout << "KhorNameShieldService starting...\n";

    LoadApiKeys();

    crow::SimpleApp app;

    CROW_ROUTE(app, "/health").methods("GET"_method)
    ([] {
        return crow::response(200, R"({"ok":true,"service":"KhorNameShieldService"})");
    });

    CROW_ROUTE(app, "/v1/namecheck").methods("POST"_method)
    ([](const crow::request& req) {

        const std::string apiKey = req.get_header_value("X-Api-Key");

        if (!IsApiKeyValid(apiKey))
        {
            return crow::response(403, R"({"ok":false,"error":"API_KEY_INVALID"})");
        }

        json body;
        try {
            body = json::parse(req.body);
        }
        catch (...)
        {
            return crow::response(400, R"({"ok":false,"error":"INVALID_JSON"})");
        }

        std::string name = body.value("name", "");
        if (name.empty())
        {
            return crow::response(400, R"({"ok":false,"error":"NAME_REQUIRED"})");
        }

        // TODO: plug in your name validation logic here
        json response = {
            {"ok", true},
            {"checkedName", name}
        };

        return crow::response(200, response.dump());
    });

    // IMPORTANT: bind only to localhost
    app.bindaddr("127.0.0.1")
       .port(8188)
       .multithreaded()
       .run();

    return 0;
}