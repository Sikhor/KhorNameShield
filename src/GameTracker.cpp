#include "GameTracker.hpp"

#include <chrono>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace
{
    std::unordered_map<std::string, std::deque<ScoreMsg>> ScoresByGame;
    std::mutex ScoresMutex;
    std::vector<Game> Games;
    std::int64_t NextGameStartTimestamp = 0;

    constexpr std::int64_t GAME_COOLDOWN_SECONDS = 60;   // 3 minutes
    constexpr std::int64_t SCORE_WINDOW_SECONDS   = 36000; // 10 hours
    constexpr std::size_t  DEFAULT_TOP_COUNT      = 5;
}

std::int64_t GetCurrentTimestamp()
{
    using namespace std::chrono;
    return duration_cast<seconds>(
        system_clock::now().time_since_epoch()
    ).count();
}

bool IsGameStartCooldownActive()
{
    return GetCurrentTimestamp() < NextGameStartTimestamp;
}

std::int64_t GetGameStartCooldownRemainingSeconds()
{
    const std::int64_t now = GetCurrentTimestamp();
    if (now >= NextGameStartTimestamp)
        return 0;

    return NextGameStartTimestamp - now;
}

void StartGameCooldown(std::int64_t cooldownSeconds)
{
    NextGameStartTimestamp = GetCurrentTimestamp() + cooldownSeconds;
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

GameStatus GameStatusFromString(const std::string& str)
{
    if (str == "Running") return GameStatus::Running;
    if (str == "Over")    return GameStatus::Over;
    if (str == "Pending") return GameStatus::Pending;

    return GameStatus::Pending;
}

std::string GameStatusToString(GameStatus status)
{
    switch (status)
    {
        case GameStatus::Running: return "Running";
        case GameStatus::Over:    return "Over";
        case GameStatus::Pending: return "Pending";
    }

    return "Pending";
}

bool LoadGamesFromFile(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open())
    {
        std::cerr << "Failed to open games file: " << filename << std::endl;
        return false;
    }

    try
    {
        json j;
        file >> j;

        if (!j.contains("games") || !j["games"].is_array())
        {
            std::cerr << "Invalid games.json format" << std::endl;
            return false;
        }

        std::lock_guard<std::mutex> lock(ScoresMutex);

        Games.clear();

        for (const auto& item : j["games"])
        {
            Game g;
            g.gameId = item.at("gameId").get<std::string>();
            g.description = item.at("description").get<std::string>();
            g.winScore = item.at("winScore").get<int>();
            g.status = GameStatusFromString(item.at("status").get<std::string>());

            Games.push_back(g);
        }

        std::cout << "Loaded " << Games.size() << " games" << std::endl;
        return true;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error parsing games.json: " << e.what() << std::endl;
        return false;
    }
}

const std::vector<Game>& GetAllGames()
{
    return Games;
}

const Game* GetGameById(const std::string& gameId)
{
    for (const Game& g : Games)
    {
        if (g.gameId == gameId)
            return &g;
    }
    return nullptr;
}

bool ClearScoresForGame(const std::string& gameId)
{
    auto it = ScoresByGame.find(gameId);
    if (it == ScoresByGame.end())
    {
        return false;
    }

    ScoresByGame.erase(it);
    return true;
}

Game* GetOrStartRunningGame()
{
    // already running?
    for (Game& g : Games)
    {
        if (g.status == GameStatus::Running)
            return &g;
    }

    // still cooling down? then do not start a new one yet
    if (IsGameStartCooldownActive())
        return nullptr;

    // cooldown is over and no game is running:
    // now clear old finished leaderboards and recycle games
    ClearScoresForOverGames();
    ResetOverGamesToPending();

    // collect pending games
    std::vector<Game*> pendingGames;
    for (Game& g : Games)
    {
        if (g.status == GameStatus::Pending)
        {
            pendingGames.push_back(&g);
        }
    }

    if (pendingGames.empty())
        return nullptr;

    // random pick
    static std::random_device rd;
    static std::mt19937 gen(rd());

    std::uniform_int_distribution<std::size_t> dist(0, pendingGames.size() - 1);
    Game* selectedGame = pendingGames[dist(gen)];

    selectedGame->status = GameStatus::Running;
    return selectedGame;
}

std::string GetTopScores(const std::string& gameId, std::size_t maxEntries)
{
    json response;
    response["ok"] = true;
    response["gameId"] = gameId;
    response["leaderboard"] = json::array();

    auto itGame = ScoresByGame.find(gameId);
    if (itGame == ScoresByGame.end())
    {
        response["activeScoreMessages"] = 0;
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
                itPlayer->second.avatarId = s.avatarId;
            }
        }
    }

    std::vector<LeaderboardEntry> leaderboard;
    leaderboard.reserve(totalsByCharName.size());

    for (const auto& [charName, entry] : totalsByCharName)
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

std::string BuildStatusResponse(
    const Game* game,
    bool scoreAccepted,
    int scoreAdded,
    bool gameOver,
    const std::string& winnerCharName)
{
    json response;
    response["ok"] = true;
    response["scoreAccepted"] = scoreAccepted;
    response["scoreAdded"] = scoreAdded;
    response["gameOver"] = gameOver;
    response["winnerCharName"] = winnerCharName;
    response["cooldownActive"] = IsGameStartCooldownActive();
    response["nextGameStartsInSeconds"] = GetGameStartCooldownRemainingSeconds();

    // normal running game case
    if (game)
    {
        response["hasRunningGame"] = (game->status == GameStatus::Running);
        response["gameStatus"] = GameStatusToString(game->status);

        response["game"] = {
            {"gameId", game->gameId},
            {"description", game->description},
            {"winScore", game->winScore},
            {"status", GameStatusToString(game->status)}
        };

        json topScores = json::parse(GetTopScores(game->gameId, DEFAULT_TOP_COUNT));
        response["leaderboard"] = topScores["leaderboard"];
        response["activeScoreMessages"] = topScores["activeScoreMessages"];

        return response.dump();
    }

    // no running game: maybe we are in cooldown and should show last finished game
    Game* overGame = GetLatestOverGame();

    if (overGame)
    {
        response["hasRunningGame"] = false;
        response["gameStatus"] = "Waiting";

        response["game"] = {
            {"gameId", overGame->gameId},
            {"description", overGame->description},
            {"winScore", overGame->winScore},
            {"status", GameStatusToString(overGame->status)}
        };

        json topScores = json::parse(GetTopScores(overGame->gameId, DEFAULT_TOP_COUNT));
        response["leaderboard"] = topScores["leaderboard"];
        response["activeScoreMessages"] = topScores["activeScoreMessages"];

        return response.dump();
    }

    // no running game and no previous finished game
    response["hasRunningGame"] = false;
    response["gameStatus"] = "Waiting";
    response["game"] = nullptr;
    response["leaderboard"] = json::array();
    response["activeScoreMessages"] = 0;

    return response.dump();
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
        msg.timestamp = GetCurrentTimestamp();

        if (msg.avatarId.empty() || msg.charName.empty() || msg.score < 0)
        {
            return R"({"ok":false,"error":"INVALID_SCORE_MSG"})";
        }

        std::lock_guard<std::mutex> lock(ScoresMutex);

        Game* runningGame = GetOrStartRunningGame();

        if (!runningGame)
        {
            return BuildStatusResponse(nullptr, false, 0, false, "");
        }

        auto& scores = ScoresByGame[runningGame->gameId];
        CleanupOldScores(scores, SCORE_WINDOW_SECONDS);

        int currentTotalForPlayer = 0;
        for (const ScoreMsg& s : scores)
        {
            if (s.charName == msg.charName)
            {
                currentTotalForPlayer += s.score;
            }
        }

        // heartbeat only
        if (msg.score == 0)
        {
            return BuildStatusResponse(runningGame, false, 0, false, "");
        }

        // winning score is NOT stored, game is ended immediately
        if (currentTotalForPlayer + msg.score >= runningGame->winScore)
        {
            // at least we need to reach the winning score to win!!!
            scores.push_back(msg);

            runningGame->status = GameStatus::Over;
            StartGameCooldown(GAME_COOLDOWN_SECONDS);

            return BuildStatusResponse(runningGame, false, 0, true, msg.charName);
        }

        // normal score add
        scores.push_back(msg);
        CleanupOldScores(scores, SCORE_WINDOW_SECONDS);

        return BuildStatusResponse(runningGame, true, msg.score, false, "");
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


std::string NewGame(const std::string& body)
{
    try
    {
        json j = json::parse(body);

        std::string gameId = j.at("gameId").get<std::string>();

        if (gameId.empty())
        {
            return R"({"ok":false,"error":"INVALID_GAMEID"})";
        }

        std::lock_guard<std::mutex> lock(ScoresMutex);

        ClearScoresForGame(gameId);

        for (Game& g : Games)
        {
            if (g.gameId == gameId)
            {
                g.status = GameStatus::Pending;
                return R"({"ok":true})";
            }
        }

        return R"({"ok":false,"error":"GAME_NOT_FOUND"})";
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


Game* GetLatestOverGame()
{
    for (Game& g : Games)
    {
        if (g.status == GameStatus::Over)
            return &g;
    }

    return nullptr;
}

void ResetOverGamesToPending()
{
    for (Game& g : Games)
    {
        if (g.status == GameStatus::Over)
        {
            g.status = GameStatus::Pending;
        }
    }
}

void ClearScoresForOverGames()
{
    for (const Game& g : Games)
    {
        if (g.status == GameStatus::Over)
        {
            ClearScoresForGame(g.gameId);
        }
    }
}