#pragma once
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

enum class Severity { Low = 0, Medium = 1, High = 2 };

inline const char* SeverityStr(Severity s) {
    switch (s) {
    case Severity::Low: return "low";
    case Severity::Medium: return "medium";
    case Severity::High: return "high";
    }
    return "low";
}

inline Severity ParseSeverity(const std::string& s) {
    if (s == "high") return Severity::High;
    if (s == "medium") return Severity::Medium;
    return Severity::Low;
}

inline void BumpSeverity(Severity& cur, Severity s) {
    if ((int)s > (int)cur) cur = s;
}

/* ============================================================
   Normalization (bypass-resistant)
   - basic Unicode cleanup + basic homoglyph folding
   - leetspeak folding
   - collapse repeats
   - strip separators
   ============================================================ */

// ---- minimal UTF-8 decode/encode ----
inline bool Utf8Next(const std::string& s, size_t& i, uint32_t& cp) {
    if (i >= s.size()) return false;
    unsigned char c = (unsigned char)s[i++];

    if (c < 0x80) { cp = c; return true; }

    int extra = 0;
    if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; extra = 1; }
    else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; extra = 2; }
    else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; extra = 3; }
    else { cp = 0xFFFD; return true; }

    for (int k = 0; k < extra; ++k) {
        if (i >= s.size()) { cp = 0xFFFD; return true; }
        unsigned char cc = (unsigned char)s[i++];
        if ((cc & 0xC0) != 0x80) { cp = 0xFFFD; return true; }
        cp = (cp << 6) | (cc & 0x3F);
    }
    return true;
}

inline void Utf8Append(std::string& out, uint32_t cp) {
    if (cp < 0x80) out.push_back((char)cp);
    else if (cp < 0x800) {
        out.push_back((char)(0xC0 | (cp >> 6)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back((char)(0xE0 | (cp >> 12)));
        out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    } else {
        out.push_back((char)(0xF0 | (cp >> 18)));
        out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    }
}

inline bool IsCombiningMark(uint32_t cp) {
    return (cp >= 0x0300 && cp <= 0x036F) ||
           (cp >= 0x1AB0 && cp <= 0x1AFF) ||
           (cp >= 0x1DC0 && cp <= 0x1DFF) ||
           (cp >= 0x20D0 && cp <= 0x20FF) ||
           (cp >= 0xFE20 && cp <= 0xFE2F);
}

inline bool IsZeroWidth(uint32_t cp) {
    switch (cp) {
    case 0x200B: case 0x200C: case 0x200D:
    case 0x2060: case 0xFEFF:
        return true;
    default: return false;
    }
}

// conservative confusables folding: Cyrillic/Greek look-alikes -> Latin
inline uint32_t FoldHomoglyph(uint32_t cp) {
    static const std::unordered_map<uint32_t, uint32_t> m = {
        // Cyrillic
        {0x0410,'A'},{0x0412,'B'},{0x0415,'E'},{0x041A,'K'},{0x041C,'M'},{0x041D,'H'},{0x041E,'O'},
        {0x0420,'P'},{0x0421,'C'},{0x0422,'T'},{0x0425,'X'},{0x0406,'I'},{0x0408,'J'},
        {0x0430,'a'},{0x0435,'e'},{0x043E,'o'},{0x0440,'p'},{0x0441,'c'},{0x0445,'x'},{0x0456,'i'},
        {0x0458,'j'},{0x0443,'y'},{0x043A,'k'},{0x043C,'m'},{0x0442,'t'},{0x043D,'h'},{0x0432,'b'},
        {0x0455,'s'},
        // Greek (selected)
        {0x0391,'A'},{0x0392,'B'},{0x0395,'E'},{0x0397,'H'},{0x0399,'I'},{0x039A,'K'},{0x039C,'M'},
        {0x039D,'N'},{0x039F,'O'},{0x03A1,'P'},{0x03A4,'T'},{0x03A7,'X'},{0x03A5,'Y'},
        {0x03B1,'a'},{0x03B5,'e'},{0x03BF,'o'},{0x03C1,'p'},{0x03C4,'t'},{0x03C7,'x'},{0x03C5,'y'}
    };
    auto it = m.find(cp);
    return (it != m.end()) ? it->second : cp;
}

inline std::string UnicodeFoldLower(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    size_t i = 0;
    uint32_t cp = 0;
    while (Utf8Next(raw, i, cp)) {
        if (cp == 0 || IsZeroWidth(cp) || IsCombiningMark(cp)) continue;
        cp = FoldHomoglyph(cp);
        if (cp < 128) cp = (uint32_t)std::tolower((unsigned char)cp);
        Utf8Append(out, cp);
    }
    return out;
}

inline char FoldLeetChar(char c) {
    switch (c) {
    case '0': return 'o';
    case '1': return 'i';
    case '3': return 'e';
    case '4': return 'a';
    case '5': return 's';
    case '7': return 't';
    case '8': return 'b';
    case '9': return 'g';
    case '@': return 'a';
    case '$': return 's';
    case '!': return 'i';
    default: return c;
    }
}

inline std::string LeetFold(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char uc : s) {
        char c = (char)uc;
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        out.push_back(FoldLeetChar(c));
    }
    return out;
}

inline std::string CollapseRepeats(const std::string& s, int maxRun = 2) {
    if (s.empty()) return s;
    std::string out;
    out.reserve(s.size());
    char prev = 0;
    int run = 0;
    for (char c : s) {
        if (c == prev) {
            run++;
            if (run <= maxRun) out.push_back(c);
        } else {
            prev = c;
            run = 1;
            out.push_back(c);
        }
    }
    return out;
}

// for scan key: keep only [a-z0-9]
inline std::string StripSeparatorsScan(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (unsigned char uc : s) {
        if (std::isalnum(uc)) out.push_back((char)uc);
    }
    return out;
}

struct NormalizedName {
    std::string unicodeFolded;
    std::string leetFolded;
    std::string scanKey;
};

inline NormalizedName NormalizeNameForScan(const std::string& raw) {
    NormalizedName n;
    n.unicodeFolded = UnicodeFoldLower(raw);
    n.leetFolded = LeetFold(n.unicodeFolded);
    auto collapsed = CollapseRepeats(n.leetFolded, 2);
    n.scanKey = StripSeparatorsScan(collapsed);
    return n;
}

/* ============================================================
   Word lists
   ============================================================ */

struct WordList {
    std::string file;       // filename for debugging
    std::string category;   // "reserved", "hate_speech", ...
    Severity severity = Severity::Medium;
    std::string matchMode;  // "contains" | "exact" | "word"
    std::vector<std::string> tokens; // already normalized scan tokens
};

inline bool MatchWordMode(const std::string& scanKey, const std::string& token) {
    // boundary best-effort: only alphabetic boundaries
    size_t pos = scanKey.find(token);
    while (pos != std::string::npos) {
        bool leftOk = (pos == 0) || !std::isalpha((unsigned char)scanKey[pos - 1]);
        bool rightOk = (pos + token.size() >= scanKey.size()) ||
                       !std::isalpha((unsigned char)scanKey[pos + token.size()]);
        if (leftOk && rightOk) return true;
        pos = scanKey.find(token, pos + 1);
    }
    return false;
}

inline std::string NormalizeToken(const std::string& token) {
    auto n = NormalizeNameForScan(token);
    return n.scanKey;
}

struct ListsState {
    std::vector<WordList> lists;
    std::unordered_map<std::string, fs::file_time_type> mtimes; // path->mtime
    std::chrono::steady_clock::time_point lastCheck = std::chrono::steady_clock::now();
};

class WordLists {
public:
    explicit WordLists(std::string dir) : dir_(std::move(dir)) {}

    // Hot reload if any JSON changed
    void MaybeReload(int reloadSeconds) {
        auto now = std::chrono::steady_clock::now();
        if (now - state_.lastCheck < std::chrono::seconds(reloadSeconds)) return;
        state_.lastCheck = now;

        bool changed = false;
        std::unordered_map<std::string, fs::file_time_type> newMtimes;

        if (!fs::exists(dir_) || !fs::is_directory(dir_)) return;

        for (const auto& entry : fs::directory_iterator(dir_)) {
            if (!entry.is_regular_file()) continue;
            auto p = entry.path();
            if (p.extension() != ".json") continue;

            auto mtime = fs::last_write_time(p);
            newMtimes[p.string()] = mtime;

            auto it = state_.mtimes.find(p.string());
            if (it == state_.mtimes.end() || it->second != mtime)
                changed = true;
        }

        // deleted files?
        if (newMtimes.size() != state_.mtimes.size()) changed = true;

        if (!changed) return;

        // reload everything (simple + reliable)
        std::vector<WordList> loaded;
        for (const auto& kv : newMtimes) {
            LoadOneFile(kv.first, loaded);
        }

        std::lock_guard<std::mutex> lk(mu_);
        state_.lists = std::move(loaded);
        state_.mtimes = std::move(newMtimes);
    }

    struct Hit {
        bool ok = true;
        Severity severity = Severity::Low;
        std::vector<std::string> reasons;
    };

    Hit Check(const std::string& scanKey) const {
        Hit h;

        std::lock_guard<std::mutex> lk(mu_);
        for (const auto& list : state_.lists) {
            for (const auto& token : list.tokens) {
                bool match = false;
                if (list.matchMode == "exact") {
                    match = (scanKey == token);
                } else if (list.matchMode == "word") {
                    match = MatchWordMode(scanKey, token);
                } else { // contains
                    match = (scanKey.find(token) != std::string::npos);
                }

                if (match) {
                    h.ok = false;
                    BumpSeverity(h.severity, list.severity);
                    h.reasons.push_back("LIST_" + list.category);
                    break;
                }
            }
        }

        // optional: de-duplicate reasons
        std::sort(h.reasons.begin(), h.reasons.end());
        h.reasons.erase(std::unique(h.reasons.begin(), h.reasons.end()), h.reasons.end());
        return h;
    }

private:
    void LoadOneFile(const std::string& path, std::vector<WordList>& out) const {
        std::ifstream f(path);
        if (!f.is_open()) return;

        json j;
        try { f >> j; } catch (...) { return; }
        if (!j.is_object()) return;

        WordList wl;
        wl.file = fs::path(path).filename().string();
        wl.category = j.value("category", "unknown");
        wl.severity = ParseSeverity(j.value("severity", "medium"));
        wl.matchMode = j.value("matchMode", "contains");

        if (j.contains("words") && j["words"].is_array()) {
            for (const auto& w : j["words"]) {
                if (!w.is_string()) continue;
                auto token = NormalizeToken(w.get<std::string>());
                if (!token.empty()) wl.tokens.push_back(std::move(token));
            }
        }

        if (!wl.tokens.empty())
            out.push_back(std::move(wl));
    }

    std::string dir_;
    mutable std::mutex mu_;
    mutable ListsState state_;
};