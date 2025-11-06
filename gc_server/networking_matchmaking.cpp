#include "networking_matchmaking.hpp"
#include "steam_network_message.hpp"
#include "logger.hpp"
#include <cmath>

void GCNetwork_Matchmaking::HandleMatchmakingClient2GCHello(SNetSocket_t p2psocket, void* message, 
                                                           uint32_t msgsize, uint64_t steamId, 
                                                           MYSQL* ranked_db) {
    NetworkMessage netMsg(message, msgsize);
    CMsgGCCStrike15_v2_MatchmakingClient2GCHello request;
    
    if (!netMsg.ParseTo(&request)) {
        logger::error("Failed to parse MatchmakingClient2GCHello");
        return;
    }
    
    logger::info("Processing MatchmakingClient2GCHello from player %llu", steamId);
    
    // Get matchmaking manager instance
    auto* mmManager = MatchmakingManager::GetInstance();
    
    // Build response
    CMsgGCCStrike15_v2_MatchmakingGC2ClientHello response;
    mmManager->BuildMatchmakingHello(response, steamId, ranked_db);
    
    // Send response
    NetworkMessage responseMsg = NetworkMessage::FromProto(response, 
                                                          k_EMsgGCCStrike15_v2_MatchmakingGC2ClientHello);
    responseMsg.WriteToSocket(p2psocket, true);
    
    logger::info("Sent MatchmakingGC2ClientHello to player %llu", steamId);
}

void GCNetwork_Matchmaking::HandleMatchmakingStart(SNetSocket_t p2psocket, void* message,
                                                  uint32_t msgsize, uint64_t steamId,
                                                  MYSQL* ranked_db) {
    NetworkMessage netMsg(message, msgsize);
    CMsgGCCStrike15_v2_MatchmakingStart request;
    
    if (!netMsg.ParseTo(&request)) {
        logger::error("Failed to parse MatchmakingStart");
        return;
    }
    
    logger::info("Player %llu requesting to start matchmaking", steamId);
    
    auto* mmManager = MatchmakingManager::GetInstance();
    
    // Check if player is already in queue
    if (mmManager->IsPlayerInQueue(steamId)) {
        logger::info("Player %llu already in queue", steamId);
        SendQueueStatus(p2psocket, steamId);
        return;
    }
    
    // Get player rating
    PlayerSkillRating rating = mmManager->GetPlayerRating(steamId, ranked_db);
    
    // Extract map preferences from request
    std::vector<std::string> preferredMaps;
    for (int i = 0; i < request.maps_size(); ++i) {
        preferredMaps.push_back(request.maps(i));
    }
    
    // Add player to queue
    if (mmManager->AddPlayerToQueue(steamId, p2psocket, rating, preferredMaps)) {
        logger::info("Player %llu added to matchmaking queue (MMR: %u)", steamId, rating.mmr);
        
        // Send queue status update
        SendQueueStatus(p2psocket, steamId);
        
        // Immediately try to create matches
        mmManager->ProcessMatchmakingQueue();
    } else {
        logger::error("Failed to add player %llu to queue", steamId);
    }
}

void GCNetwork_Matchmaking::HandleMatchmakingStop(SNetSocket_t p2psocket, void* message,
                                                 uint32_t msgsize, uint64_t steamId) {
    NetworkMessage netMsg(message, msgsize);
    CMsgGCCStrike15_v2_MatchmakingStop request;
    
    if (!netMsg.ParseTo(&request)) {
        logger::error("Failed to parse MatchmakingStop");
        return;
    }
    
    logger::info("Player %llu requesting to stop matchmaking", steamId);
    
    auto* mmManager = MatchmakingManager::GetInstance();
    
    // Remove player from queue
    if (mmManager->RemovePlayerFromQueue(steamId)) {
        logger::info("Player %llu removed from matchmaking queue", steamId);
        
        // Send confirmation
        CMsgGCCStrike15_v2_MatchmakingGC2ClientUpdate update;
        update.set_matchtype(0); // Not in matchmaking
        
        NetworkMessage updateMsg = NetworkMessage::FromProto(update,
                                                            k_EMsgGCCStrike15_v2_MatchmakingGC2ClientUpdate);
        updateMsg.WriteToSocket(p2psocket, true);
    }
}

void GCNetwork_Matchmaking::HandleMatchmakingAccept(SNetSocket_t p2psocket, void* message,
                                                   uint32_t msgsize, uint64_t steamId) {
    logger::info("Player %llu accepting match", steamId);
    
    auto* mmManager = MatchmakingManager::GetInstance();
    
    if (mmManager->AcceptMatch(steamId)) {
        // Check if match is ready
        auto match = mmManager->GetMatchByPlayer(steamId);
        if (match && match->state == MatchState::IN_PROGRESS) {
            // Send final reservation details
            CMsgGCCStrike15_v2_MatchmakingGC2ClientReserve reserve;
            mmManager->BuildMatchReservation(reserve, *match, steamId);
            
            NetworkMessage reserveMsg = NetworkMessage::FromProto(reserve,
                                                                 k_EMsgGCCStrike15_v2_MatchmakingGC2ClientReserve);
            reserveMsg.WriteToSocket(p2psocket, true);
            
            logger::info("Match %llu is ready - sent reservation to player %llu", 
                        match->matchId, steamId);
        } else if (match) {
            // Send update on accepted players count
            SendMatchUpdate(p2psocket, *match);
        }
    } else {
        logger::error("Failed to accept match for player %llu", steamId);
    }
}

void GCNetwork_Matchmaking::HandleMatchmakingDecline(SNetSocket_t p2psocket, void* message,
                                                    uint32_t msgsize, uint64_t steamId) {
    logger::info("Player %llu declining match", steamId);
    
    auto* mmManager = MatchmakingManager::GetInstance();
    
    if (mmManager->DeclineMatch(steamId)) {
        // Send confirmation that player is out of matchmaking
        CMsgGCCStrike15_v2_MatchmakingGC2ClientUpdate update;
        update.set_matchtype(0); // Not in matchmaking
        
        NetworkMessage updateMsg = NetworkMessage::FromProto(update,
                                                            k_EMsgGCCStrike15_v2_MatchmakingGC2ClientUpdate);
        updateMsg.WriteToSocket(p2psocket, true);
    }
}

void GCNetwork_Matchmaking::HandleMatchEnd(SNetSocket_t p2psocket, void* message,
                                          uint32_t msgsize, uint64_t steamId, MYSQL* ranked_db) {
    NetworkMessage netMsg(message, msgsize);
    CMsgGCCStrike15_v2_MatchmakingServerMatchEnd request;
    
    if (!netMsg.ParseTo(&request)) {
        logger::error("Failed to parse MatchmakingServerMatchEnd");
        return;
    }
    
    if (!request.has_stats()) {
        logger::error("MatchEnd message missing stats");
        return;
    }
    
    const auto& stats = request.stats();
    uint64_t matchId = stats.has_match() ? stats.match() : 0;
    
    logger::info("Received match end for match %llu", matchId);
    
    auto* mmManager = MatchmakingManager::GetInstance();
    auto match = mmManager->GetMatch(matchId);
    
    if (!match) {
        logger::error("Match %llu not found", matchId);
        return;
    }
    
    // Calculate MMR changes
    CalculateMMRChange(*match, ranked_db);
    
    // Update match state
    mmManager->UpdateMatchState(matchId, MatchState::COMPLETED);
    
    // Update player stats
    if (stats.kills_size() > 0 && stats.deaths_size() > 0) {
        size_t playerIndex = 0;
        
        // Process team A stats
        for (const auto& player : match->teamA) {
            if (playerIndex < stats.kills_size()) {
                bool won = (stats.match_result() == 1); // Team A won
                UpdatePlayerStats(player->steamId, won,
                                stats.kills(playerIndex),
                                stats.deaths(playerIndex),
                                stats.mvps_size() > playerIndex ? stats.mvps(playerIndex) : 0,
                                ranked_db);
            }
            playerIndex++;
        }
        
        // Process team B stats
        for (const auto& player : match->teamB) {
            if (playerIndex < stats.kills_size()) {
                bool won = (stats.match_result() == 2); // Team B won
                UpdatePlayerStats(player->steamId, won,
                                stats.kills(playerIndex),
                                stats.deaths(playerIndex),
                                stats.mvps_size() > playerIndex ? stats.mvps(playerIndex) : 0,
                                ranked_db);
            }
            playerIndex++;
        }
    }
    
    logger::info("Match %llu completed and stats updated", matchId);
}

void GCNetwork_Matchmaking::HandleMatchRoundStats(SNetSocket_t p2psocket, void* message,
                                                 uint32_t msgsize, uint64_t steamId) {
    NetworkMessage netMsg(message, msgsize);
    CMsgGCCStrike15_v2_MatchmakingServerRoundStats request;
    
    if (!netMsg.ParseTo(&request)) {
        logger::error("Failed to parse MatchmakingServerRoundStats");
        return;
    }
    
    uint64_t matchId = request.has_match() ? request.match() : 0;
    int round = request.has_round() ? request.round() : 0;
    
    logger::info("Received round %d stats for match %llu", round, matchId);
    
    // Could broadcast round updates to spectators or store for replay system
}

void GCNetwork_Matchmaking::SendMatchFound(SNetSocket_t socket, const Match& match, uint64_t steamId) {
    auto* mmManager = MatchmakingManager::GetInstance();
    
    // Send match reservation
    CMsgGCCStrike15_v2_MatchmakingGC2ClientReserve reserve;
    mmManager->BuildMatchReservation(reserve, match, steamId);
    
    NetworkMessage reserveMsg = NetworkMessage::FromProto(reserve,
                                                         k_EMsgGCCStrike15_v2_MatchmakingGC2ClientReserve);
    reserveMsg.WriteToSocket(socket, true);
    
    logger::info("Sent match found notification to player %llu for match %llu",
                steamId, match.matchId);
}

void GCNetwork_Matchmaking::SendMatchUpdate(SNetSocket_t socket, const Match& match) {
    auto* mmManager = MatchmakingManager::GetInstance();
    
    CMsgGCCStrike15_v2_MatchmakingGC2ClientUpdate update;
    mmManager->BuildMatchUpdate(update, match);
    
    NetworkMessage updateMsg = NetworkMessage::FromProto(update,
                                                        k_EMsgGCCStrike15_v2_MatchmakingGC2ClientUpdate);
    updateMsg.WriteToSocket(socket, true);
}

void GCNetwork_Matchmaking::SendMatchAbandoned(SNetSocket_t socket, uint64_t matchId, uint64_t abandonerId) {
    CMsgGCCStrike15_v2_MatchmakingGC2ClientAbandon abandon;
    abandon.set_account_id(abandonerId & 0xFFFFFFFF);
    abandon.set_penalty_seconds(1800); // 30 min cooldown
    abandon.set_penalty_reason(1); // Abandoned match
    
    NetworkMessage abandonMsg = NetworkMessage::FromProto(abandon,
                                                         k_EMsgGCCStrike15_v2_MatchmakingGC2ClientAbandon);
    abandonMsg.WriteToSocket(socket, true);
    
    logger::info("Sent abandon notification for player %llu in match %llu", abandonerId, matchId);
}

void GCNetwork_Matchmaking::SendQueueStatus(SNetSocket_t socket, uint64_t steamId) {
    auto* mmManager = MatchmakingManager::GetInstance();
    auto stats = mmManager->GetQueueStatistics();
    
    CMsgGCCStrike15_v2_MatchmakingGC2ClientUpdate update;
    update.set_matchtype(1); // Competitive
    update.set_waiting_players(stats.totalPlayers);
    update.set_est_wait_time(stats.avgWaitTime.count());
    
    NetworkMessage updateMsg = NetworkMessage::FromProto(update,
                                                        k_EMsgGCCStrike15_v2_MatchmakingGC2ClientUpdate);
    updateMsg.WriteToSocket(socket, true);
    
    logger::info("Sent queue status to player %llu (Queue size: %zu)", steamId, stats.totalPlayers);
}

void GCNetwork_Matchmaking::CalculateMMRChange(const Match& match, MYSQL* ranked_db) {
    if (!ranked_db) return;
    
    // Calculate average MMR for each team
    uint32_t teamA_MMR = 0, teamB_MMR = 0;
    
    for (const auto& player : match.teamA) {
        teamA_MMR += player->skillRating.mmr;
    }
    for (const auto& player : match.teamB) {
        teamB_MMR += player->skillRating.mmr;
    }
    
    teamA_MMR /= match.teamA.size();
    teamB_MMR /= match.teamB.size();
    
    // Simple ELO calculation
    const float K = 32.0f; // K-factor for rating changes
    
    float expectedA = 1.0f / (1.0f + powf(10.0f, (teamB_MMR - teamA_MMR) / 400.0f));
    float expectedB = 1.0f - expectedA;
    
    // Assuming we know the match result (would come from match end message)
    // For now, let's say team A won
    float scoreA = 1.0f; // 1 for win, 0 for loss
    float scoreB = 0.0f;
    
    int changeA = static_cast<int>(K * (scoreA - expectedA));
    int changeB = static_cast<int>(K * (scoreB - expectedB));
    
    auto* mmManager = MatchmakingManager::GetInstance();
    
    // Update team A players
    for (auto& player : match.teamA) {
        player->skillRating.mmr += changeA;
        if (scoreA > 0.5f) player->skillRating.wins++;
        
        // Update rank based on MMR
        player->skillRating.rank = player->skillRating.mmr / 100; // Simplified rank calculation
        if (player->skillRating.rank > 18) player->skillRating.rank = 18;
        
        mmManager->UpdatePlayerRating(player->steamId, player->skillRating, ranked_db);
        logger::info("Player %llu MMR: %d -> %d", player->steamId, 
                    player->skillRating.mmr - changeA, player->skillRating.mmr);
    }
    
    // Update team B players
    for (auto& player : match.teamB) {
        player->skillRating.mmr += changeB;
        if (scoreB > 0.5f) player->skillRating.wins++;
        
        // Update rank based on MMR
        player->skillRating.rank = player->skillRating.mmr / 100;
        if (player->skillRating.rank > 18) player->skillRating.rank = 18;
        
        mmManager->UpdatePlayerRating(player->steamId, player->skillRating, ranked_db);
        logger::info("Player %llu MMR: %d -> %d", player->steamId,
                    player->skillRating.mmr - changeB, player->skillRating.mmr);
    }
}

void GCNetwork_Matchmaking::UpdatePlayerStats(uint64_t steamId, bool won, uint32_t kills,
                                             uint32_t deaths, uint32_t mvps, MYSQL* ranked_db) {
    if (!ranked_db) return;
    
    char query[512];
    snprintf(query, sizeof(query),
            "INSERT INTO player_stats (steamid64, matches_played, matches_won, total_kills, total_deaths, total_mvps) "
            "VALUES (%llu, 1, %d, %u, %u, %u) "
            "ON DUPLICATE KEY UPDATE "
            "matches_played = matches_played + 1, "
            "matches_won = matches_won + %d, "
            "total_kills = total_kills + %u, "
            "total_deaths = total_deaths + %u, "
            "total_mvps = total_mvps + %u",
            steamId, won ? 1 : 0, kills, deaths, mvps,
            won ? 1 : 0, kills, deaths, mvps);
    
    if (mysql_query(ranked_db, query) != 0) {
        logger::error("Failed to update player stats: %s", mysql_error(ranked_db));
    } else {
        logger::info("Updated stats for player %llu: K:%u D:%u MVP:%u Won:%d",
                    steamId, kills, deaths, mvps, won);
    }
}
