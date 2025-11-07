// Improved implementation with thread safety and modern C++ practices
#include "matchmaking_manager.hpp"
#include "networking.hpp"
#include "gameserver_manager.hpp"
#include "logger.hpp"
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <numeric>

// Initialize static global instance pointer
MatchmakingManager* MatchmakingManager::s_globalInstance = nullptr;

// Global instance accessors (for compatibility with existing code)
void MatchmakingManager::SetGlobalInstance(MatchmakingManager* instance) {
    s_globalInstance = instance;
}

MatchmakingManager* MatchmakingManager::GetInstance() {
    if (!s_globalInstance) {
        logger::error("MatchmakingManager::GetInstance() called but no global instance set!");
    }
    return s_globalInstance;
}

void MatchmakingManager::DestroyGlobalInstance() {
    s_globalInstance = nullptr;
}

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
    
    // Sort by MMR for better matching
    std::sort(allPlayers.begin(), allPlayers.end(),
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

// Additional helper methods for MatchmakingManager

void MatchmakingManager::UpdateMatchState(uint64_t matchId, MatchState newState) {
    std::unique_lock<std::shared_mutex> lock(m_matchMutex);
    
    auto matchIt = m_activeMatches.find(matchId);
    if (matchIt != m_activeMatches.end()) {
        matchIt->second->state.store(newState);
        logger::info("Match %llu state updated to %d", matchId, static_cast<int>(newState));
    }
}

std::optional<std::shared_ptr<Match>> MatchmakingManager::GetMatch(uint64_t matchId) const {
    std::shared_lock<std::shared_mutex> lock(m_matchMutex);
    
    auto it = m_activeMatches.find(matchId);
    if (it != m_activeMatches.end()) {
        return it->second;
    }
    return std::nullopt;
}

MatchmakingManager::QueueStatistics MatchmakingManager::GetQueueStatistics() const {
    std::shared_lock<std::shared_mutex> queueLock(m_queueMutex);
    std::shared_lock<std::shared_mutex> matchLock(m_matchMutex);
    
    QueueStatistics stats;
    stats.totalPlayers = 0;
    stats.activeMatches = m_activeMatches.size();
    
    auto now = std::chrono::steady_clock::now();
    uint64_t totalWaitSeconds = 0;
    size_t playerCount = 0;
    
    for (const auto& [bracket, queue] : m_queuesBySkill) {
        for (const auto& player : queue) {
            if (!player) continue;
            
            auto waitTime = std::chrono::duration_cast<std::chrono::seconds>(now - player->queueTime);
            totalWaitSeconds += waitTime.count();
            playerCount++;
            stats.totalPlayers++;
            
            // Count by rank
            stats.playersByRank[player->skillRating.rank]++;
        }
    }
    
    if (playerCount > 0) {
        stats.avgWaitTime = std::chrono::seconds(totalWaitSeconds / playerCount);
    } else {
        stats.avgWaitTime = std::chrono::seconds(0);
    }
    
    return stats;
}

void MatchmakingManager::UpdateConfig(const MatchmakingConfig& config) {
    m_config = config;
    logger::info("MatchmakingManager configuration updated");
}

std::shared_ptr<Match> MatchmakingManager::CreateMatch(const std::vector<std::shared_ptr<QueueEntry>>& players) {
    auto match = std::make_shared<Match>();
    match->matchId = m_nextMatchId.fetch_add(1);
    match->matchToken = GenerateMatchToken();
    match->state.store(MatchState::WAITING_FOR_CONFIRMATION);
    match->readyUpDeadline = std::chrono::steady_clock::now() + m_config.readyUpTime;
    
    // Select map
    auto selectedMap = SelectMapForMatch(players);
    match->mapName = selectedMap.value_or("de_dust2");
    
    // Calculate average MMR
    uint32_t totalMMR = 0;
    for (const auto& player : players) {
        if (player) totalMMR += player->skillRating.mmr;
    }
    match->avgMMR = totalMMR / players.size();
    
    // Distribute players to teams
    DistributePlayersToTeams(match, players);
    
    return match;
}

void MatchmakingManager::DistributePlayersToTeams(std::shared_ptr<Match> match, 
                                                   const std::vector<std::shared_ptr<QueueEntry>>& players) {
    if (players.size() != m_config.playersPerTeam * 2) {
        logger::error("Invalid player count for team distribution: %zu", players.size());
        return;
    }
    
    // Snake draft for balanced teams (based on MMR-sorted list)
    // Team A: indices 0, 3, 4, 7, 8
    // Team B: indices 1, 2, 5, 6, 9
    match->teamA.push_back(players[0]);
    match->teamB.push_back(players[1]);
    match->teamB.push_back(players[2]);
    match->teamA.push_back(players[3]);
    match->teamA.push_back(players[4]);
    match->teamB.push_back(players[5]);
    match->teamB.push_back(players[6]);
    match->teamA.push_back(players[7]);
    match->teamA.push_back(players[8]);
    match->teamB.push_back(players[9]);
}

std::optional<std::string> MatchmakingManager::SelectMapForMatch(const std::vector<std::shared_ptr<QueueEntry>>& players) const {
    // Count map preferences
    std::map<std::string, int> mapWeights;
    
    for (const auto& player : players) {
        if (!player) continue;
        for (const auto& map : player->preferredMaps) {
            mapWeights[map]++;
        }
    }
    
    // If no preferences, use full map pool
    if (mapWeights.empty()) {
        for (const auto& map : m_config.mapPool) {
            mapWeights[map] = 1;
        }
    }
    
    // Find maps with highest weight
    std::vector<std::string> topMaps;
    int maxWeight = 0;
    
    for (const auto& [map, weight] : mapWeights) {
        if (weight > maxWeight) {
            maxWeight = weight;
            topMaps.clear();
            topMaps.push_back(map);
        } else if (weight == maxWeight) {
            topMaps.push_back(map);
        }
    }
    
    // Random selection among top weighted maps
    if (!topMaps.empty()) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, topMaps.size() - 1);
        return topMaps[dis(gen)];
    }
    
    return std::nullopt;
}

std::string MatchmakingManager::GenerateMatchToken() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    for (int i = 0; i < 16; ++i) {
        ss << std::hex << dis(gen);
    }
    
    return ss.str();
}

void MatchmakingManager::CancelMatchInternal(uint64_t matchId, const std::string& reason) {
    std::unique_lock<std::shared_mutex> lock(m_matchMutex);
    
    auto matchIt = m_activeMatches.find(matchId);
    if (matchIt == m_activeMatches.end()) {
        return;
    }
    
    auto match = matchIt->second;
    match->state.store(MatchState::ABANDONED);
    
    std::vector<std::shared_ptr<QueueEntry>> playersToRequeue;
    
    // Collect players who accepted (except the one who declined)
    for (const auto& player : match->teamA) {
        m_playerToMatch.erase(player->steamId);
        if (player->acceptedMatch.load()) {
            playersToRequeue.push_back(player);
        }
    }
    for (const auto& player : match->teamB) {
        m_playerToMatch.erase(player->steamId);
        if (player->acceptedMatch.load()) {
            playersToRequeue.push_back(player);
        }
    }
    
    // Remove the match
    m_activeMatches.erase(matchIt);
    
    lock.unlock(); // Release lock before re-queueing
    
    // Free the server
    auto* serverManager = GameServerManager::GetInstance();
    auto servers = serverManager->GetAllServers();
    for (const auto& server : servers) {
        if (server.currentMatchId == matchId) {
            serverManager->ReleaseServer(server.serverSteamId);
            break;
        }
    }
    
    // Re-queue players who accepted
    for (const auto& player : playersToRequeue) {
        AddPlayerToQueue(player->steamId, player->socket, player->skillRating, player->preferredMaps);
    }
    
    logger::info("Match %llu cancelled: %s", matchId, reason.c_str());
}

void MatchmakingManager::BuildMatchmakingHello(CMsgGCCStrike15_v2_MatchmakingGC2ClientHello& message, 
                                              uint64_t steamId) {
    message.set_account_id(steamId & 0xFFFFFFFF);
    
    // Get player rating
    auto ratingOpt = GetPlayerRating(steamId);
    PlayerSkillRating rating;
    if (ratingOpt.has_value()) {
        rating = ratingOpt.value();
    } else {
        // Use defaults
        rating.mmr = 1000;
        rating.rank = 6;
        rating.wins = 0;
        rating.level = 1;
    }
    
    // Set ranking info
    auto* ranking = message.mutable_ranking();
    ranking->set_account_id(steamId & 0xFFFFFFFF);
    ranking->set_rank_id(rating.rank);
    ranking->set_wins(rating.wins);
    
    // Set player level
    message.set_player_level(rating.level);
    message.set_player_cur_xp(0);
    
    // Check if player is in an active match
    std::shared_lock<std::shared_mutex> lock(m_matchMutex);
    auto matchIt = m_playerToMatch.find(steamId);
    if (matchIt != m_playerToMatch.end()) {
        auto match = m_activeMatches.find(matchIt->second);
        if (match != m_activeMatches.end() && match->second->state.load() == MatchState::IN_PROGRESS) {
            auto* ongoingMatch = message.mutable_ongoingmatch();
            BuildMatchReservation(*ongoingMatch, *match->second, steamId);
        }
    }
}

void MatchmakingManager::BuildMatchReservation(CMsgGCCStrike15_v2_MatchmakingGC2ClientReserve& message,
                                              const Match& match, uint64_t steamId) {
    message.set_serverid(match.matchId);
    // Note: direct_udp_ip requires inet_addr from <arpa/inet.h> (Linux) or <winsock2.h> (Windows)
    // message.set_direct_udp_ip(inet_addr(match.serverAddress.c_str()));
    message.set_direct_udp_port(match.serverPort);
    message.set_reservationid(match.matchId);
    
    // Note: following fields not available in current protobuf schema
    // MatchState state = match.state.load();
    // message.set_reservation_stage(state == MatchState::WAITING_FOR_CONFIRMATION ? 1 : 2);
    // message.add_map(match.mapName);
    // auto* token = message.mutable_encrypted_steamid();
    // token->assign(match.matchToken);
}

void MatchmakingManager::BuildMatchUpdate(CMsgGCCStrike15_v2_MatchmakingGC2ClientUpdate& message,
                                         const Match& match) {
    (void)match; // Unused - fields not available in protobuf schema
    // Note: following fields not available in current protobuf schema
    // message.set_matchtype(1);
    // message.set_waiting_players(match.GetAcceptedCount());
    // auto now = std::chrono::steady_clock::now();
    // auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - match.createdTime);
    // message.set_est_wait_time(elapsed.count());
}

void MatchmakingManager::NotifyMatchFound(const Match& match) {
    // Notify all players in the match
    for (const auto& player : match.teamA) {
        if (player) {
            CMsgGCCStrike15_v2_MatchmakingGC2ClientReserve reserveMsg;
            BuildMatchReservation(reserveMsg, match, player->steamId);
            // Send via networking layer (would need GCNetwork integration)
        }
    }
    
    for (const auto& player : match.teamB) {
        if (player) {
            CMsgGCCStrike15_v2_MatchmakingGC2ClientReserve reserveMsg;
            BuildMatchReservation(reserveMsg, match, player->steamId);
            // Send via networking layer
        }
    }
}

void MatchmakingManager::NotifyMatchReady(const Match& match) {
    logger::info("Notifying players that match %llu is ready", match.matchId);
    // In a real implementation, this would send connection details to players
}

void MatchmakingManager::Update() {
    auto now = std::chrono::steady_clock::now();
    
    // Process queue periodically
    if (now - m_lastQueueCheck >= m_config.queueCheckInterval) {
        ProcessMatchmakingQueue();
        CheckReadyUpTimeouts();
        m_lastQueueCheck = now;
        
        // Log queue status
        auto stats = GetQueueStatistics();
        if (stats.totalPlayers > 0) {
            logger::info("Matchmaking queue: %zu players, %zu active matches",
                        stats.totalPlayers, stats.activeMatches);
        }
    }
    
    // Cleanup old matches periodically
    if (now - m_lastCleanup >= std::chrono::minutes(1)) {
        CleanupAbandonedMatches();
        m_lastCleanup = now;
    }
}

void MatchmakingManager::CheckReadyUpTimeouts() {
    auto now = std::chrono::steady_clock::now();
    std::vector<uint64_t> matchesToCancel;
    
    {
        std::shared_lock<std::shared_mutex> lock(m_matchMutex);
        
        for (const auto& [matchId, match] : m_activeMatches) {
            if (match->state.load() == MatchState::WAITING_FOR_CONFIRMATION &&
                now > match->readyUpDeadline) {
                matchesToCancel.push_back(matchId);
            }
        }
    }
    
    for (uint64_t matchId : matchesToCancel) {
        CancelMatchInternal(matchId, "Ready-up timeout");
    }
}

bool MatchmakingManager::IsPlayerInQueue(uint64_t steamId) const {
    std::shared_lock<std::shared_mutex> lock(m_queueMutex);
    
    for (const auto& [bracket, queue] : m_queues) {
        for (const auto& entry : queue) {
            if (entry && entry->steamId == steamId) {
                return true;
            }
        }
    }
    return false;
}

bool MatchmakingManager::AcceptMatch(uint64_t steamId) {
    std::unique_lock<std::shared_mutex> lock(m_matchMutex);
    
    // Find match containing this player
    for (auto& [matchId, match] : m_activeMatches) {
        if (match->state.load() != MatchState::WAITING_FOR_CONFIRMATION) {
            continue;
        }
        
        // Check if player is in this match
        bool found = false;
        for (auto& player : match->teamA) {
            if (player && player->steamId == steamId) {
                player->hasAccepted = true;
                found = true;
                break;
            }
        }
        if (!found) {
            for (auto& player : match->teamB) {
                if (player && player->steamId == steamId) {
                    player->hasAccepted = true;
                    found = true;
                    break;
                }
            }
        }
        
        if (found) {
            // Check if all players have accepted
            bool allAccepted = true;
            for (const auto& p : match->teamA) {
                if (p && !p->hasAccepted) {
                    allAccepted = false;
                    break;
                }
            }
            if (allAccepted) {
                for (const auto& p : match->teamB) {
                    if (p && !p->hasAccepted) {
                        allAccepted = false;
                        break;
                    }
                }
            }
            
            if (allAccepted) {
                match->state.store(MatchState::IN_PROGRESS);
                NotifyMatchReady(*match);
            }
            return true;
        }
    }
    return false;
}

bool MatchmakingManager::DeclineMatch(uint64_t steamId) {
    std::unique_lock<std::shared_mutex> lock(m_matchMutex);
    
    // Find and cancel match containing this player
    for (auto& [matchId, match] : m_activeMatches) {
        if (match->state.load() != MatchState::WAITING_FOR_CONFIRMATION) {
            continue;
        }
        
        // Check if player is in this match
        for (const auto& player : match->teamA) {
            if (player && player->steamId == steamId) {
                CancelMatchInternal(matchId, "Player declined");
                return true;
            }
        }
        for (const auto& player : match->teamB) {
            if (player && player->steamId == steamId) {
                CancelMatchInternal(matchId, "Player declined");
                return true;
            }
        }
    }
    return false;
}

uint32_t MatchmakingManager::GetSkillBracket(uint32_t mmr) const {
    // Simple bracket system: divide MMR by bracket size
    return mmr / m_config.baseMMRSpread;
}

bool MatchmakingManager::ArePlayersCompatible(const QueueEntry& p1, const QueueEntry& p2) const {
    // Check MMR difference
    uint32_t mmrDiff = std::abs(static_cast<int32_t>(p1.skillRating.mmr) - 
                                 static_cast<int32_t>(p2.skillRating.mmr));
    
    if (mmrDiff > m_config.baseMMRSpread * 2) {
        return false;
    }
    
    // Check map preferences if any
    if (!p1.preferredMaps.empty() && !p2.preferredMaps.empty()) {
        bool hasCommonMap = false;
        for (const auto& map1 : p1.preferredMaps) {
            for (const auto& map2 : p2.preferredMaps) {
                if (map1 == map2) {
                    hasCommonMap = true;
                    break;
                }
            }
            if (hasCommonMap) break;
        }
        if (!hasCommonMap) {
            return false;
        }
    }
    
    return true;
}
