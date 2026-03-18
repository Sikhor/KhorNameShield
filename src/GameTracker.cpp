#include "GameTracker.hpp"

#include <chrono>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <random>
#include <mutex>
#include <unordered_map>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace
{
    std::deque<ScoreMsg> CurrentScores;
    std::mutex ScoresMutex;
    std::vector<Game> Games;
    std::int64_t NextGameStartTimestamp = 0;

    constexpr std::int64_t GAME_COOLDOWN_SECONDS = 60;     // 1 minute
    constexpr std::int64_t SCORE_WINDOW_SECONDS  = 36000;  // 10 hours
    constexpr std::size_t  DEFAULT_TOP_COUNT     = 5;
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
        CurrentScores.clear();
        NextGameStartTimestamp = 0;

        for (const auto& item : j["games"])
        {
            Game g;
            g.gameId = item.at("gameId").get<std::string>();
            g.description = item.at("description").get<std::string>();
            g.winScore = item.at("winScore").get<int>();

            // On startup we normalize all games to Pending
            g.status = GameStatus::Pending;

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

Game* GetRunningGame()
{
    for (Game& g : Games)
    {
        if (g.status == GameStatus::Running)
            return &g;
    }
    return nullptr;
}

Game* GetOverGame()
{
    for (Game& g : Games)
    {
        if (g.status == GameStatus::Over)
            return &g;
    }
    return nullptr;
}

void ClearAllScores()
{
    CurrentScores.clear();
}

Game* GetOrStartRunningGame()
{
    // 1. If a game is already running, use it
    if (Game* runningGame = GetRunningGame())
    {
        return runningGame;
    }

    // 2. If cooldown is active, do not start a new game yet
    if (IsGameStartCooldownActive())
    {
        return nullptr;
    }

    // 3. Cooldown over: if an old game is over, clear all scores and reset it
    if (Game* overGame = GetOverGame())
    {
        ClearAllScores();
        overGame->status = GameStatus::Pending;
    }

    // 4. Collect pending games
    std::vector<Game*> pendingGames;
    for (Game& g : Games)
    {
        if (g.status == GameStatus::Pending)
        {
            pendingGames.push_back(&g);
        }
    }

    if (pendingGames.empty())
    {
        return nullptr;
    }

    // 5. Pick a random pending game and start it
    static std::random_device rd;
    static std::mt19937 gen(rd());

    std::uniform_int_distribution<std::size_t> dist(0, pendingGames.size() - 1);
    Game* selectedGame = pendingGames[dist(gen)];
    selectedGame->status = GameStatus::Running;

    return selectedGame;
}

std::string GetTopScores(std::size_t maxEntries)
{
    json response;
    response["ok"] = true;
    response["leaderboard"] = json::array();

    CleanupOldScores(CurrentScores, SCORE_WINDOW_SECONDS);

    if (CurrentScores.empty())
    {
        response["activeScoreMessages"] = 0;
        return response.dump();
    }

    std::unordered_map<std::string, LeaderboardEntry> totalsByCharName;

    for (const ScoreMsg& s : CurrentScores)
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

    response["activeScoreMessages"] = CurrentScores.size();
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

    // Running or explicitly provided game
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

        json topScores = json::parse(GetTopScores(DEFAULT_TOP_COUNT));
        response["leaderboard"] = topScores["leaderboard"];
        response["activeScoreMessages"] = topScores["activeScoreMessages"];

        return response.dump();
    }

    // No running game: maybe waiting with last over game still visible
    if (Game* overGame = GetOverGame())
    {
        response["hasRunningGame"] = false;
        response["gameStatus"] = "Waiting";

        response["game"] = {
            {"gameId", overGame->gameId},
            {"description", overGame->description},
            {"winScore", overGame->winScore},
            {"status", GameStatusToString(overGame->status)}
        };

        json topScores = json::parse(GetTopScores(DEFAULT_TOP_COUNT));
        response["leaderboard"] = topScores["leaderboard"];
        response["activeScoreMessages"] = topScores["activeScoreMessages"];

        return response.dump();
    }

    // Nothing running and nothing over
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

        // no running game because cooldown/waiting is active
        if (!runningGame)
        {
            return BuildStatusResponse(nullptr, false, 0, false, "");
        }

        CleanupOldScores(CurrentScores, SCORE_WINDOW_SECONDS);

        // heartbeat only
        if (msg.score == 0)
        {
            return BuildStatusResponse(runningGame, false, 0, false, "");
        }

        int currentTotalForPlayer = 0;
        for (const ScoreMsg& s : CurrentScores)
        {
            if (s.charName == msg.charName)
            {
                currentTotalForPlayer += s.score;
            }
        }

        // winning score reached
        if (currentTotalForPlayer + msg.score >= runningGame->winScore)
        {
            CurrentScores.push_back(msg);

            runningGame->status = GameStatus::Over;
            StartGameCooldown(GAME_COOLDOWN_SECONDS);

            return BuildStatusResponse(runningGame, true, msg.score, true, msg.charName);
        }

        // normal score add
        CurrentScores.push_back(msg);
        CleanupOldScores(CurrentScores, SCORE_WINDOW_SECONDS);

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

        ClearAllScores();
        NextGameStartTimestamp = 0;

        bool found = false;

        for (Game& g : Games)
        {
            if (g.gameId == gameId)
            {
                g.status = GameStatus::Running;
                found = true;
            }
            else
            {
                g.status = GameStatus::Pending;
            }
        }

        if (!found)
        {
            return R"({"ok":false,"error":"GAME_NOT_FOUND"})";
        }

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