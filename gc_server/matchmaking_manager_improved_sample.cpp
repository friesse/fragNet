// Sample implementation showing key improvements for critical functions
#include "matchmaking_manager_improved.hpp"
#include "networking.hpp"
#include "gameserver_manager.hpp"
#include "logger.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <execution>  // C++17 parallel algorithms

// Constructor with dependency injection
MatchmakingManager::MatchmakingManager(std::shared_ptr<IDatabase> database, const MatchmakingConfig& config)
    : m_config(config), m_database(std::move(database)) {
    if (!m_database) {
        throw std::invalid_argument("Database interface cannot be null");
    }
    
    m_lastQueueCheck = std::chrono::steady_clock::now();
    m_lastCleanup = std::chrono::steady_clock::now();
    
    logger::info("MatchmakingManager initialized with config: %zu players per team", m_config.playersPerTeam);
}

// Thread-safe queue addition with validation
bool MatchmakingManager::AddPlayerToQueue(uint64_t steamId, SNetSocket_t socket, 
                                         const PlayerSkillRating& rating,
                                         const std::vector<std::string>& preferredMaps) {
    // Input validation
    if (steamId == 0) {
        logger::error("Invalid steamId: 0");
        return false;
    }
    
    // Validate MMR range
    if (rating.mmr > 5000 || rating.rank > 18) {
        logger::warning("Suspicious skill rating for player %llu: MMR=%u, Rank=%u", 
                       steamId, rating.mmr, rating.rank);
        // Could implement anti-cheat check here
    }
    
    // Remove player from queue if already queued (thread-safe)
    RemovePlayerFromQueue(steamId);
    
    try {
        auto entry = std::make_shared<QueueEntry>(steamId, socket);
        entry->skillRating = rating;
        entry->preferredMaps = preferredMaps.empty() ? m_config.mapPool : preferredMaps;
        
        // Validate map names against allowed pool
        entry->preferredMaps.erase(
            std::remove_if(entry->preferredMaps.begin(), entry->preferredMaps.end(),
                [this](const std::string& map) {
                    return std::find(m_config.mapPool.begin(), m_config.mapPool.end(), map) 
                           == m_config.mapPool.end();
                }),
            entry->preferredMaps.end()
        );
        
        uint32_t bracket = GetSkillBracket(rating.mmr);
        
        // Thread-safe queue modification
        {
            std::unique_lock<std::shared_mutex> lock(m_queueMutex);
            m_queuesBySkill[bracket].push_back(std::move(entry));
        }
        
        logger::info("Player %llu added to matchmaking queue (MMR: %u, Bracket: %u)", 
                     steamId, rating.mmr, bracket);
        
        // Try to create matches immediately
        ProcessMatchmakingQueue();
        
        return true;
        
    } catch (const std::exception& e) {
        logger::error("Failed to add player %llu to queue: %s", steamId, e.what());
        return false;
    }
}

// Thread-safe queue removal
bool MatchmakingManager::RemovePlayerFromQueue(uint64_t steamId) {
    std::unique_lock<std::shared_mutex> lock(m_queueMutex);
    
    for (auto& [bracket, queue] : m_queuesBySkill) {
        auto it = std::remove_if(queue.begin(), queue.end(),
            [steamId](const std::shared_ptr<QueueEntry>& entry) {
                return entry && entry->steamId == steamId;
            });
        
        if (it != queue.end()) {
            queue.erase(it, queue.end());
            logger::info("Player %llu removed from matchmaking queue", steamId);
            return true;
        }
    }
    return false;
}

// Optimized queue processing with better algorithms
void MatchmakingManager::ProcessMatchmakingQueue() {
    auto candidates = FindMatchCandidates();
    if (!candidates.has_value()) {
        return;
    }
    
    auto match = CreateMatch(candidates.value());
    if (!match) {
        return;
    }
    
    // Assign server (would integrate with GameServerManager)
    auto* serverManager = GameServerManager::GetInstance();
    auto* server = serverManager->FindAvailableServer();
    
    if (!server) {
        // Queue players with priority for next available server
        logger::warning("Match ready but no servers available");
        
        // Send priority queue notification to players
        for (const auto& player : candidates.value()) {
            // Send notification about priority queue status
        }
        return;
    }
    
    match->serverAddress = server->address;
    match->serverPort = server->port;
    
    // Thread-safe match storage
    {
        std::unique_lock<std::shared_mutex> matchLock(m_matchMutex);
        m_activeMatches[match->matchId] = match;
        
        // Map players to match
        for (const auto& player : candidates.value()) {
            m_playerToMatch[player->steamId] = match->matchId;
        }
    }
    
    // Remove players from queue
    {
        std::unique_lock<std::shared_mutex> queueLock(m_queueMutex);
        for (const auto& player : candidates.value()) {
            for (auto& [bracket, queue] : m_queuesBySkill) {
                queue.erase(
                    std::remove_if(queue.begin(), queue.end(),
                        [&player](const std::shared_ptr<QueueEntry>& entry) {
                            return entry && entry->steamId == player->steamId;
                        }),
                    queue.end()
                );
            }
        }
    }
    
    // Notify players (outside of locks to prevent deadlocks)
    NotifyMatchFound(*match);
    
    // Log match creation
    if (m_database) {
        m_database->LogMatch(*match);
    }
    
    logger::info("Match %llu created with %zu players on %s:%u",
                match->matchId, candidates.value().size(),
                server->address.c_str(), server->port);
    
    // Recursively try to create more matches
    ProcessMatchmakingQueue();
}

// Optimized candidate finding with early exit
std::optional<std::vector<std::shared_ptr<QueueEntry>>> MatchmakingManager::FindMatchCandidates() {
    std::shared_lock<std::shared_mutex> lock(m_queueMutex);
    
    // Collect eligible players
    std::vector<std::shared_ptr<QueueEntry>> allPlayers;
    size_t totalPlayers = 0;
    
    for (const auto& [bracket, queue] : m_queuesBySkill) {
        totalPlayers += queue.size();
        if (totalPlayers >= m_config.playersPerTeam * 2) {
            allPlayers.insert(allPlayers.end(), queue.begin(), queue.end());
        }
    }
    
    if (allPlayers.size() < m_config.playersPerTeam * 2) {
        return std::nullopt;
    }
    
    // Sort by MMR for better matching (using parallel sort for performance)
    std::sort(std::execution::par_unseq, allPlayers.begin(), allPlayers.end(),
        [](const std::shared_ptr<QueueEntry>& a, const std::shared_ptr<QueueEntry>& b) {
            return a && b && a->skillRating.mmr < b->skillRating.mmr;
        });
    
    // Use sliding window to find compatible match
    const size_t matchSize = m_config.playersPerTeam * 2;
    
    for (size_t i = 0; i <= allPlayers.size() - matchSize; ++i) {
        std::vector<std::shared_ptr<QueueEntry>> candidates(
            allPlayers.begin() + i, 
            allPlayers.begin() + i + matchSize
        );
        
        // Check if all players in window are compatible
        bool allCompatible = true;
        uint32_t minMMR = candidates.front()->skillRating.mmr;
        uint32_t maxMMR = candidates.back()->skillRating.mmr;
        
        // Quick MMR spread check
        if (maxMMR - minMMR > m_config.baseMMRSpread * 2) {
            continue;
        }
        
        // Detailed compatibility check
        for (size_t j = 0; j < candidates.size() && allCompatible; ++j) {
            for (size_t k = j + 1; k < candidates.size() && allCompatible; ++k) {
                if (!ArePlayersCompatible(*candidates[j], *candidates[k])) {
                    allCompatible = false;
                }
            }
        }
        
        if (allCompatible) {
            return candidates;
        }
    }
    
    return std::nullopt;
}

// Database operations with prepared statements (example implementation)
std::optional<PlayerSkillRating> MatchmakingManager::GetPlayerRating(uint64_t steamId) const {
    if (!m_database) {
        logger::error("Database interface not available");
        return std::nullopt;
    }
    
    try {
        return m_database->GetPlayerRating(steamId);
    } catch (const std::exception& e) {
        logger::error("Failed to get player rating for %llu: %s", steamId, e.what());
        return std::nullopt;
    }
}

bool MatchmakingManager::UpdatePlayerRating(uint64_t steamId, const PlayerSkillRating& newRating) {
    if (!m_database) {
        logger::error("Database interface not available");
        return false;
    }
    
    try {
        return m_database->UpdatePlayerRating(steamId, newRating);
    } catch (const std::exception& e) {
        logger::error("Failed to update player rating for %llu: %s", steamId, e.what());
        return false;
    }
}

// Thread-safe match retrieval
std::optional<std::shared_ptr<Match>> MatchmakingManager::GetMatchByPlayer(uint64_t steamId) const {
    std::shared_lock<std::shared_mutex> lock(m_matchMutex);
    
    auto playerIt = m_playerToMatch.find(steamId);
    if (playerIt == m_playerToMatch.end()) {
        return std::nullopt;
    }
    
    auto matchIt = m_activeMatches.find(playerIt->second);
    if (matchIt == m_activeMatches.end()) {
        return std::nullopt;
    }
    
    return matchIt->second;
}

// Periodic cleanup with proper synchronization
void MatchmakingManager::CleanupAbandonedMatches() {
    auto now = std::chrono::steady_clock::now();
    std::vector<uint64_t> matchesToRemove;
    
    // Collect matches to remove (read lock)
    {
        std::shared_lock<std::shared_mutex> lock(m_matchMutex);
        
        for (const auto& [matchId, match] : m_activeMatches) {
            if (!match) continue;
            
            MatchState state = match->state.load();
            if ((state == MatchState::COMPLETED || state == MatchState::ABANDONED) &&
                (now - match->createdTime) > m_config.matchCleanupAge) {
                matchesToRemove.push_back(matchId);
            }
        }
    }
    
    // Remove matches (write lock)
    if (!matchesToRemove.empty()) {
        std::unique_lock<std::shared_mutex> lock(m_matchMutex);
        
        for (uint64_t matchId : matchesToRemove) {
            auto matchIt = m_activeMatches.find(matchId);
            if (matchIt != m_activeMatches.end()) {
                // Clean up player mappings
                auto match = matchIt->second;
                for (const auto& playerId : match->GetAllPlayerIds()) {
                    m_playerToMatch.erase(playerId);
                }
                
                m_activeMatches.erase(matchIt);
                logger::info("Cleaned up abandoned match %llu", matchId);
            }
        }
    }
}

// Match implementation
bool Match::AllPlayersAccepted() const {
    for (const auto& player : teamA) {
        if (player && !player->acceptedMatch.load()) return false;
    }
    for (const auto& player : teamB) {
        if (player && !player->acceptedMatch.load()) return false;
    }
    return true;
}

size_t Match::GetAcceptedCount() const {
    size_t count = 0;
    for (const auto& player : teamA) {
        if (player && player->acceptedMatch.load()) count++;
    }
    for (const auto& player : teamB) {
        if (player && player->acceptedMatch.load()) count++;
    }
    return count;
}

std::vector<uint64_t> Match::GetAllPlayerIds() const {
    std::vector<uint64_t> ids;
    ids.reserve(teamA.size() + teamB.size());
    
    for (const auto& player : teamA) {
        if (player) ids.push_back(player->steamId);
    }
    for (const auto& player : teamB) {
        if (player) ids.push_back(player->steamId);
    }
    
    return ids;
}

bool Match::HasPlayer(uint64_t steamId) const {
    for (const auto& player : teamA) {
        if (player && player->steamId == steamId) return true;
    }
    for (const auto& player : teamB) {
        if (player && player->steamId == steamId) return true;
    }
    return false;
}
