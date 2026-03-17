#pragma once

#include <string>
#include <cstdint>
#include <deque>
#include <unordered_map>
#include <vector>
#include <mutex>

struct ScoreMsg
{
    int score = 0;
    std::string avatarId;
    std::string charName;
    std::string gameId;
    std::int64_t timestamp = 0;
};

struct LeaderboardEntry
{
    std::string avatarId;
    std::string charName;
    int totalScore = 0;
    std::int64_t lastTimestamp = 0;
};

std::int64_t GetCurrentTimestamp();
void CleanupOldScores(std::deque<ScoreMsg>& scores, std::int64_t maxAgeSeconds = 600);

std::string SendScore(const std::string& body);
std::string NewGame(const std::string& body);

std::string GetTopScores(const std::string& gameId, std::size_t maxEntries = 5);
bool ClearScoresForGame(const std::string& gameId);

