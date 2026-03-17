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
    std::int64_t timestamp = 0;
};

struct LeaderboardEntry
{
    std::string avatarId;
    std::string charName;
    int totalScore = 0;
    std::int64_t lastTimestamp = 0;
};

enum class GameStatus
{
    Pending,
    Running,
    Over
};

struct Game
{
    std::string gameId;
    std::string description;
    int winScore = 0;
    GameStatus status = GameStatus::Pending;
};

struct FinishedGameResult
{
    bool valid = false;
    std::string gameId;
    std::string description;
    std::string winnerCharName;
    std::int64_t endedAt = 0;
    std::vector<LeaderboardEntry> finalLeaderboard;
};

std::int64_t GetCurrentTimestamp();

bool IsGameStartCooldownActive();
std::int64_t GetGameStartCooldownRemainingSeconds();
void StartGameCooldown(std::int64_t cooldownSeconds);

void CleanupOldScores(std::deque<ScoreMsg>& scores, std::int64_t maxAgeSeconds = 600);

std::string SendScore(const std::string& body);
std::string GetTopScores(const std::string& gameId, std::size_t maxEntries = 5);
std::string BuildStatusResponse(
    const Game* game,
    bool scoreAccepted,
    int scoreAdded,
    bool gameOver,
    const std::string& winnerCharName
);

bool ClearScoresForGame(const std::string& gameId);
std::string NewGame(const std::string& body);

bool LoadGamesFromFile(const std::string& filename);
const std::vector<Game>& GetAllGames();
const Game* GetGameById(const std::string& gameId);

GameStatus GameStatusFromString(const std::string& str);
std::string GameStatusToString(GameStatus status);

Game* GetOrStartRunningGame();

Game* GetLatestOverGame();
void ResetOverGamesToPending();
void ClearScoresForOverGames();
