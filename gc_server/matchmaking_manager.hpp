#pragma once

#include <vector>
#include <map>
#include <unordered_map>
#include <queue>
#include <memory>
#include <chrono>
#include <string>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <optional>
#include "steam/steam_api.h"
#include "cc_gcmessages.pb.h"
#include "cstrike15_gcmessages.pb.h"

// Forward declarations
class GCNetwork;
class IDatabase;  // Abstract database interface

// Match states
enum class MatchState {
    QUEUED,
    WAITING_FOR_CONFIRMATION,
    IN_PROGRESS,
    COMPLETED,
    ABANDONED
};

// Configuration structure
struct MatchmakingConfig {
    size_t playersPerTeam = 5;
    size_t maxSkillDifference = 3;
    std::chrono::seconds readyUpTime{30};
    std::chrono::seconds queueCheckInterval{5};
    std::chrono::minutes matchCleanupAge{5};
    uint32_t baseMMRSpread = 300;
    uint32_t mmrSpreadPerWaitTime = 100;  // Per 30 seconds
    std::vector<std::string> mapPool = {
        "de_dust2", "de_mirage", "de_inferno", "de_nuke",
        "de_overpass", "de_cache", "de_train", "de_vertigo", "de_ancient"
    };
};

// Player skill rating structure
struct PlayerSkillRating {
    uint32_t rank = 0;      // 0-18 for CS:GO ranks
    uint32_t wins = 0;
    uint32_t mmr = 1000;    // Matchmaking rating
    uint32_t level = 1;     // Player level
    
    // Add comparison operators for easier sorting
    bool operator<(const PlayerSkillRating& other) const {
        return mmr < other.mmr;
    }
};

// Queue entry for a single player
class QueueEntry {
public:
    uint64_t steamId;
    uint32_t accountId;
    SNetSocket_t socket;
    std::chrono::steady_clock::time_point queueTime;
    PlayerSkillRating skillRating;
    std::vector<std::string> preferredMaps;
    bool isPrime = false;
    std::atomic<bool> acceptedMatch{false};
    std::string region = "na";
    
    QueueEntry(uint64_t id, SNetSocket_t sock) : 
        steamId(id), 
        accountId(id & 0xFFFFFFFF),
        socket(sock),
        queueTime(std::chrono::steady_clock::now()) {}
        
    // Non-copyable but moveable
    QueueEntry(const QueueEntry&) = delete;
    QueueEntry& operator=(const QueueEntry&) = delete;
    QueueEntry(QueueEntry&&) = default;
    QueueEntry& operator=(QueueEntry&&) = default;
};

// Match structure with thread-safe operations
class Match {
public:
    uint64_t matchId;
    std::string matchToken;
    std::vector<std::shared_ptr<QueueEntry>> teamA;
    std::vector<std::shared_ptr<QueueEntry>> teamB;
    std::atomic<MatchState> state{MatchState::QUEUED};
    std::string mapName;
    std::string serverAddress;
    uint16_t serverPort;
    std::chrono::steady_clock::time_point createdTime;
    std::chrono::steady_clock::time_point readyUpDeadline;
    uint32_t avgMMR;
    
    Match() : 
        matchId(0),
        serverPort(0),
        createdTime(std::chrono::steady_clock::now()),
        avgMMR(1000) {}
        
    bool AllPlayersAccepted() const;
    size_t GetAcceptedCount() const;
    std::vector<uint64_t> GetAllPlayerIds() const;
    bool HasPlayer(uint64_t steamId) const;
};

// Thread-safe matchmaking manager
class MatchmakingManager {
private:
    // Configuration
    MatchmakingConfig m_config;
    
    // Thread safety
    mutable std::shared_mutex m_queueMutex;
    mutable std::shared_mutex m_matchMutex;
    
    // Player queues by skill bracket - using unordered_map for O(1) access
    std::unordered_map<uint32_t, std::vector<std::shared_ptr<QueueEntry>>> m_queuesBySkill;
    
    // Active matches - using unordered_map for O(1) access
    std::unordered_map<uint64_t, std::shared_ptr<Match>> m_activeMatches;
    
    // Player to match mapping - using unordered_map for O(1) access
    std::unordered_map<uint64_t, uint64_t> m_playerToMatch;
    
    // Match ID counter
    std::atomic<uint64_t> m_nextMatchId{1};
    
    // Database interface (injected dependency)
    std::shared_ptr<IDatabase> m_database;
    
    // Last update times for periodic tasks
    std::chrono::steady_clock::time_point m_lastQueueCheck;
    std::chrono::steady_clock::time_point m_lastCleanup;
    
    // Private methods
    uint32_t GetSkillBracket(uint32_t mmr) const;
    bool ArePlayersCompatible(const QueueEntry& p1, const QueueEntry& p2) const;
    std::optional<std::string> SelectMapForMatch(const std::vector<std::shared_ptr<QueueEntry>>& players) const;
    void NotifyMatchFound(const Match& match);
    void NotifyMatchReady(const Match& match);
    void CancelMatchInternal(uint64_t matchId, const std::string& reason);
    std::string GenerateMatchToken() const;
    std::optional<std::vector<std::shared_ptr<QueueEntry>>> FindMatchCandidates();
    std::shared_ptr<Match> CreateMatch(const std::vector<std::shared_ptr<QueueEntry>>& players);
    void DistributePlayersToTeams(std::shared_ptr<Match> match, const std::vector<std::shared_ptr<QueueEntry>>& players);
    
    // Global instance management (for compatibility with existing code)
    static MatchmakingManager* s_globalInstance;
    
public:
    // Constructor with dependency injection
    explicit MatchmakingManager(std::shared_ptr<IDatabase> database, const MatchmakingConfig& config = {});
    
    // Disable copy and move for singleton-like behavior if needed
    MatchmakingManager(const MatchmakingManager&) = delete;
    MatchmakingManager& operator=(const MatchmakingManager&) = delete;
    MatchmakingManager(MatchmakingManager&&) = delete;
    MatchmakingManager& operator=(MatchmakingManager&&) = delete;
    
    // Destructor
    ~MatchmakingManager() = default;
    
    // Global instance accessors (for compatibility - prefer dependency injection)
    static void SetGlobalInstance(MatchmakingManager* instance);
    static MatchmakingManager* GetInstance();
    static void DestroyGlobalInstance();
    
    // Queue management (thread-safe)
    bool AddPlayerToQueue(uint64_t steamId, SNetSocket_t socket, const PlayerSkillRating& rating, 
                         const std::vector<std::string>& preferredMaps = {});
    bool RemovePlayerFromQueue(uint64_t steamId);
    bool IsPlayerInQueue(uint64_t steamId) const;
    size_t GetQueueSize() const;
    
    // Match management (thread-safe)
    void ProcessMatchmakingQueue();
    bool AcceptMatch(uint64_t steamId);
    bool DeclineMatch(uint64_t steamId);
    void UpdateMatchState(uint64_t matchId, MatchState newState);
    std::optional<std::shared_ptr<Match>> GetMatchByPlayer(uint64_t steamId) const;
    std::optional<std::shared_ptr<Match>> GetMatch(uint64_t matchId) const;
    
    // Player information (thread-safe with database)
    std::optional<PlayerSkillRating> GetPlayerRating(uint64_t steamId) const;
    bool UpdatePlayerRating(uint64_t steamId, const PlayerSkillRating& newRating);
    
    // Message builders (thread-safe)
    void BuildMatchmakingHello(CMsgGCCStrike15_v2_MatchmakingGC2ClientHello& message, uint64_t steamId);
    void BuildMatchReservation(CMsgGCCStrike15_v2_MatchmakingGC2ClientReserve& message, const Match& match, uint64_t steamId);
    void BuildMatchUpdate(CMsgGCCStrike15_v2_MatchmakingGC2ClientUpdate& message, const Match& match);
    
    // Periodic updates (thread-safe)
    void Update();
    void CleanupAbandonedMatches();
    void CheckReadyUpTimeouts();
    
    // Statistics (thread-safe)
    struct QueueStatistics {
        size_t totalPlayers;
        std::unordered_map<uint32_t, size_t> playersByRank;
        std::chrono::seconds avgWaitTime;
        size_t activeMatches;
    };
    QueueStatistics GetQueueStatistics() const;
    
    // Configuration
    void UpdateConfig(const MatchmakingConfig& config);
    const MatchmakingConfig& GetConfig() const { return m_config; }
};

// Database interface for dependency injection
class IDatabase {
public:
    virtual ~IDatabase() = default;
    
    virtual std::optional<PlayerSkillRating> GetPlayerRating(uint64_t steamId) const = 0;
    virtual bool UpdatePlayerRating(uint64_t steamId, const PlayerSkillRating& rating) = 0;
    virtual bool LogMatch(const Match& match) = 0;
};
