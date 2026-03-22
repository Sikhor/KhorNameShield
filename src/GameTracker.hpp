#pragma once

#include <string>
#include <vector>
#include <deque>
#include <cstdint>

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

std::int64_t GetCurrentTimestamp();

bool IsGameStartCooldownActive();
std::int64_t GetGameStartCooldownRemainingSeconds();
void StartGameCooldown(std::int64_t cooldownSeconds);

void CleanupOldScores(std::deque<ScoreMsg>& scores, std::int64_t maxAgeSeconds);

GameStatus GameStatusFromString(const std::string& str);
std::string GameStatusToString(GameStatus status);

bool LoadGamesFromFile(const std::string& filename);

const std::vector<Game>& GetAllGames();
const Game* GetGameById(const std::string& gameId);

Game* GetRunningGame();
Game* GetOverGame();
Game* GetOrStartRunningGame();

void ClearAllScores();

std::string GetTopScores(std::size_t maxEntries);

std::string BuildStatusResponse(
    const Game* game,
    bool scoreAccepted,
    int scoreAdded,
    int charCurrentScore,
    bool gameOver,
    const std::string& winnerCharName);

std::string SendScore(ScoreMsg msg);
std::string PrepareAndSendScore(const std::string& body);
std::string GetGameState(const std::string& body);
std::string NewGame(const std::string& body);
std::string CheckScoreMsg(ScoreMsg msg);
