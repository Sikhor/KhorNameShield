#include "GameTracker.hpp"

#include <chrono>
#include <algorithm>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace
{
    std::unordered_map<std::string, std::deque<ScoreMsg>> ScoresByGame;
    std::mutex ScoresMutex;

    constexpr std::int64_t SCORE_WINDOW_SECONDS = 600; // 10 minutes
}

std::int64_t GetCurrentTimestamp()
{
    using namespace std::chrono;
    return duration_cast<seconds>(
        system_clock::now().time_since_epoch()
    ).count();
}

void CleanupOldScores(std::deque<ScoreMsg>& scores, std::int64_t maxAgeSeconds)
{
    const std::int64_t now = GetCurrentTimestamp();

    while (!scores.empty())
    {
        const ScoreMsg& front = scores.front();

        if ((now - front.timestamp) <= maxAgeSeconds)
            break;

        scores.pop_front();
    }
}

std::string SendScore(const std::string& body)
{
    try
    {
        json j = json::parse(body);

        ScoreMsg msg;
        msg.score = j.at("score").get<int>();
        msg.avatarId = j.at("avatarId").get<std::string>();
        msg.charName = j.at("charName").get<std::string>();
        msg.gameId = j.at("gameId").get<std::string>();
        msg.timestamp = GetCurrentTimestamp();

        if (msg.avatarId.empty() || msg.charName.empty() || msg.gameId.empty())
        {
            return R"({"ok":false,"error":"INVALID_SCORE_MSG"})";
        }

        {
            std::lock_guard<std::mutex> lock(ScoresMutex);

            auto& scores = ScoresByGame[msg.gameId];
            scores.push_back(msg);
            CleanupOldScores(scores, SCORE_WINDOW_SECONDS);
        }

        return GetTopScores(msg.gameId, 5);
    }
    catch (const json::exception&)
    {
        return R"({"ok":false,"error":"INVALID_JSON"})";
    }
    catch (const std::exception&)
    {
        return R"({"ok":false,"error":"SEND_SCORE_FAILED"})";
    }
}

std::string GetTopScores(const std::string& gameId, std::size_t maxEntries)
{
    json response;
    response["ok"] = true;
    response["gameId"] = gameId;
    response["leaderboard"] = json::array();

    std::lock_guard<std::mutex> lock(ScoresMutex);

    auto itGame = ScoresByGame.find(gameId);
    if (itGame == ScoresByGame.end())
    {
        return response.dump();
    }

    auto& scores = itGame->second;
    CleanupOldScores(scores, SCORE_WINDOW_SECONDS);

    std::unordered_map<std::string, LeaderboardEntry> totalsByCharName;

    for (const ScoreMsg& s : scores)
    {
        auto itPlayer = totalsByCharName.find(s.charName);

        if (itPlayer == totalsByCharName.end())
        {
            totalsByCharName[s.charName] = LeaderboardEntry{
                s.avatarId,
                s.charName,
                s.score,
                s.timestamp
            };
        }
        else
        {
            itPlayer->second.totalScore += s.score;

            if (s.timestamp > itPlayer->second.lastTimestamp)
            {
                itPlayer->second.lastTimestamp = s.timestamp;
                itPlayer->second.charName = s.charName;
            }
        }
    }

    std::vector<LeaderboardEntry> leaderboard;
    leaderboard.reserve(totalsByCharName.size());

    for (const auto& [avatarId, entry] : totalsByCharName)
    {
        leaderboard.push_back(entry);
    }

    std::sort(
        leaderboard.begin(),
        leaderboard.end(),
        [](const LeaderboardEntry& a, const LeaderboardEntry& b)
        {
            if (a.totalScore != b.totalScore)
                return a.totalScore > b.totalScore;

            return a.lastTimestamp > b.lastTimestamp;
        });

    if (leaderboard.size() > maxEntries)
    {
        leaderboard.resize(maxEntries);
    }

    for (const LeaderboardEntry& entry : leaderboard)
    {
        response["leaderboard"].push_back({
            {"avatarId", entry.avatarId},
            {"charName", entry.charName},
            {"totalScore", entry.totalScore},
            {"lastTimestamp", entry.lastTimestamp}
        });
    }

    response["activeScoreMessages"] = scores.size();

    return response.dump();
}

bool ClearScoresForGame(const std::string& gameId)
{
    std::lock_guard<std::mutex> lock(ScoresMutex);

    auto it = ScoresByGame.find(gameId);
    if (it == ScoresByGame.end())
    {
        return false; // nothing to delete
    }

    ScoresByGame.erase(it);
    return true;
}

std::string NewGame(const std::string& body)
{
    try
    {
        json j = json::parse(body);

        std::string gameId= j.at("gameId").get<std::string>();

        if (gameId.empty())
        {
            return R"({"ok":false,"error":"INVALID_GAMEID"})";
        }
        ClearScoresForGame(gameId);
        return R"({"ok":true})";

    }
    catch (const json::exception&)
    {
        return R"({"ok":false,"error":"INVALID_JSON"})";
    }
    catch (const std::exception&)
    {
        return R"({"ok":false,"error":"NEWGAME_FAILED"})";
    }
}
