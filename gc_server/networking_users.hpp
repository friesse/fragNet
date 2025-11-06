#pragma once

#include "gc_const.hpp"
#include "gc_const_csgo.hpp"
#include "networking.hpp"
#include "steam_network_message.hpp" // for NetworkMessage class

#include "cc_gcmessages.pb.h"

#include <mariadb/mysql.h>

RankId ScoreToRankId(int score);

class GCNetwork_Users
{
public:
    struct PlayerCommends
    {
        uint32_t friendly;
        uint32_t teaching;
        uint32_t leader;
    };

    static void BuildMatchmakingHello(CMsgGC_CC_GC2CL_BuildMatchmakingHello &message,
                                      uint64_t steamId, MYSQL *classiccounter_db,
                                      MYSQL *inventory_db, MYSQL *ranked_db);

    static void ViewPlayersProfile(SNetSocket_t p2psocket, void *message, uint32 msgsize,
                                   MYSQL *classiccounter_db, MYSQL *inventory_db, MYSQL *ranked_db);

    // commends
    static PlayerCommends GetPlayerCommends(uint64_t steamId, MYSQL *inventory_db);
    static int GetPlayerCommendTokens(uint64_t steamId, MYSQL *inventory_db);

    static void HandleCommendPlayerQuery(SNetSocket_t p2psocket, void *message, uint32 msgsize,
                                         uint64_t senderSteamId, MYSQL *inventory_db);
    static void HandleCommendPlayer(SNetSocket_t p2psocket, void *message, uint32 msgsize,
                                    uint64_t senderSteamId, MYSQL *inventory_db);

    // reports
    static int GetPlayerReportTokens(uint64_t steamId, MYSQL *inventory_db);
    static void HandlePlayerReport(SNetSocket_t p2psocket, void *message, uint32 msgsize,
                                   uint64_t senderSteamId, MYSQL *inventory_db);

    // helpers
    static std::string SteamID64ToSteamID2(uint64_t steamId64);
    static bool IsPlayerBanned(const std::string &steamId2, MYSQL *classiccounter_db);
    static void GetPlayerCooldownInfo(const std::string &steamId2, CMsgGC_CC_GC2CL_BuildMatchmakingHello &message, MYSQL *classiccounter_db);
    static uint32_t GetPlayerRankId(const std::string &steamId2, MYSQL *ranked_db);
    static uint32_t GetPlayerWins(const std::string &steamId2, MYSQL *ranked_db);
    static void GetPlayerMedals(uint64_t steamId, PlayerMedalsInfo *medals, MYSQL *inventory_db);
};