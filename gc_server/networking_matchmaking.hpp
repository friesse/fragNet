#pragma once

#include "steam/steam_api.h"
#include <mariadb/mysql.h>
#include "cc_gcmessages.pb.h"
#include "cstrike15_gcmessages.pb.h"
#include "matchmaking_manager.hpp"

class GCNetwork_Matchmaking {
public:
    // Main matchmaking message handlers
    static void HandleMatchmakingClient2GCHello(SNetSocket_t p2psocket, void* message, uint32_t msgsize,
                                                uint64_t steamId, MYSQL* ranked_db);
    
    static void HandleMatchmakingStart(SNetSocket_t p2psocket, void* message, uint32_t msgsize,
                                       uint64_t steamId, MYSQL* ranked_db);
    
    static void HandleMatchmakingStop(SNetSocket_t p2psocket, void* message, uint32_t msgsize,
                                      uint64_t steamId);
    
    static void HandleMatchmakingAccept(SNetSocket_t p2psocket, void* message, uint32_t msgsize,
                                        uint64_t steamId);
    
    static void HandleMatchmakingDecline(SNetSocket_t p2psocket, void* message, uint32_t msgsize,
                                         uint64_t steamId);
    
    // Match state handlers
    static void HandleMatchEnd(SNetSocket_t p2psocket, void* message, uint32_t msgsize,
                               uint64_t steamId, MYSQL* ranked_db);
    
    static void HandleMatchRoundStats(SNetSocket_t p2psocket, void* message, uint32_t msgsize,
                                      uint64_t steamId);
    
    // Helper functions
    static void SendMatchFound(SNetSocket_t socket, const Match& match, uint64_t steamId);
    static void SendMatchUpdate(SNetSocket_t socket, const Match& match);
    static void SendMatchAbandoned(SNetSocket_t socket, uint64_t matchId, uint64_t abandonerId);
    static void SendQueueStatus(SNetSocket_t socket, uint64_t steamId);
    
    // MMR calculation
    static void CalculateMMRChange(const Match& match, MYSQL* ranked_db);
    static void UpdatePlayerStats(uint64_t steamId, bool won, uint32_t kills, uint32_t deaths, 
                                 uint32_t mvps, MYSQL* ranked_db);
};
