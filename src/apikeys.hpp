#include <nlohmann/json.hpp>
#include <openssl/sha.h>

#include <chrono>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_set>
#include <vector>

using json = nlohmann::json;

struct ApiKeyConfig {
    std::unordered_set<std::string> allowedSha256Hex; // lowercase hex
    std::chrono::steady_clock::time_point lastLoad = std::chrono::steady_clock::now();
};

static std::mutex g_apiKeyMutex;
static ApiKeyConfig g_apiKeys;

static std::string ToLowerAscii(std::string s) {
    for (char& c : s) c = (char)std::tolower((unsigned char)c);
    return s;
}

static std::string Sha256HexLower(const std::string& input) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char*)input.data(), input.size(), hash);

    static const char* hex = "0123456789abcdef";
    std::string out;
    out.resize(SHA256_DIGEST_LENGTH * 2);

    for (size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        out[i * 2 + 0] = hex[(hash[i] >> 4) & 0xF];
        out[i * 2 + 1] = hex[(hash[i] >> 0) & 0xF];
    }
    return out;
}

static bool LoadApiKeysFile(const std::string& path, ApiKeyConfig& cfgOut) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    json j;
    try { f >> j; }
    catch (...) { return false; }

    ApiKeyConfig next;

    // Accept either plaintext keys (keys) OR hashed keys (keys_sha256_hex).
    if (j.contains("keys_sha256_hex") && j["keys_sha256_hex"].is_array()) {
        for (const auto& v : j["keys_sha256_hex"]) {
            if (!v.is_string()) continue;
            std::string h = ToLowerAscii(v.get<std::string>());
            if (h.size() == 64) next.allowedSha256Hex.insert(std::move(h));
        }
    } else if (j.contains("keys") && j["keys"].is_array()) {
        // fallback (less secure): hash plaintext keys at load time
        for (const auto& v : j["keys"]) {
            if (!v.is_string()) continue;
            std::string key = v.get<std::string>();
            next.allowedSha256Hex.insert(Sha256HexLower(key));
        }
    } else {
        return false;
    }

    next.lastLoad = std::chrono::steady_clock::now();
    cfgOut = std::move(next);
    return true;
}

static void MaybeReloadApiKeys(const std::string& path, int reloadSeconds) {
    static std::chrono::steady_clock::time_point lastTry = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    if (now - lastTry < std::chrono::seconds(reloadSeconds)) return;
    lastTry = now;

    ApiKeyConfig next;
    if (LoadApiKeysFile(path, next)) {
        std::lock_guard<std::mutex> lk(g_apiKeyMutex);
        g_apiKeys = std::move(next);
    }
}

static bool IsApiKeyAllowed(const std::string& apiKey) {
    if (apiKey.empty()) return false;
    const std::string h = Sha256HexLower(apiKey);

    std::lock_guard<std::mutex> lk(g_apiKeyMutex);
    return g_apiKeys.allowedSha256Hex.find(h) != g_apiKeys.allowedSha256Hex.end();
}
