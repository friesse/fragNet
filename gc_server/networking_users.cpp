#include "networking_users.hpp"

std::string GCNetwork_Users::SteamID64ToSteamID2(uint64_t steamId64)
{
    char steamId2[32];
    uint32_t y = steamId64 & 1;
    uint32_t z = ((steamId64 & 0xFFFFFFFF) - y) / 2;
    snprintf(steamId2, sizeof(steamId2), "STEAM_1:%u:%u", y, z);
    return std::string(steamId2);
}

RankId ScoreToRankId(int score)
{
    if (score < 100)
        return RankNone;
    else if (score < 150)
        return RankSilver1;
    else if (score < 200)
        return RankSilver2;
    else if (score < 300)
        return RankSilver3;
    else if (score < 400)
        return RankSilver4;
    else if (score < 500)
        return RankSilverElite;
    else if (score < 600)
        return RankSilverEliteMaster;
    else if (score < 750)
        return RankGoldNova1;
    else if (score < 900)
        return RankGoldNova2;
    else if (score < 1050)
        return RankGoldNova3;
    else if (score < 1200)
        return RankGoldNovaMaster;
    else if (score < 1400)
        return RankMasterGuardian1;
    else if (score < 1600)
        return RankMasterGuardian2;
    else if (score < 1800)
        return RankMasterGuardianElite;
    else if (score < 2000)
        return RankDistinguishedMasterGuardian;
    else if (score < 2200)
        return RankLegendaryEagle;
    else if (score < 2400)
        return RankLegendaryEagleMaster;
    else if (score < 2700)
        return RankSupremeMasterFirstClass;
    return RankGlobalElite;
}

uint32_t GCNetwork_Users::GetPlayerRankId(const std::string &steamId2, MYSQL *ranked_db)
{
    char query[512];
    snprintf(query, sizeof(query),
             "SELECT score FROM ranked WHERE steam = '%s'",
             steamId2.c_str());

    if (mysql_query(ranked_db, query) == 0)
    {
        MYSQL_RES *result = mysql_store_result(ranked_db);
        if (result)
        {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row)
            {
                int score = row[0] ? atoi(row[0]) : 0;
                mysql_free_result(result);
                return static_cast<uint32_t>(ScoreToRankId(score));
            }
            mysql_free_result(result);
        }
    }
    else
    {
        logger::error("Failed to query rank info: %s", mysql_error(ranked_db));
    }
    return static_cast<uint32_t>(RankNone);
}

uint32_t GCNetwork_Users::GetPlayerWins(const std::string &steamId2, MYSQL *ranked_db)
{
    char query[512];
    snprintf(query, sizeof(query),
             "SELECT match_win FROM ranked WHERE steam = '%s'",
             steamId2.c_str());

    if (mysql_query(ranked_db, query) == 0)
    {
        MYSQL_RES *result = mysql_store_result(ranked_db);
        if (result)
        {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row)
            {
                uint32_t wins = row[0] ? atoi(row[0]) : 0;
                mysql_free_result(result);
                return wins;
            }
            mysql_free_result(result);
        }
    }
    else
    {
        logger::error("Failed to query wins info: %s", mysql_error(ranked_db));
    }
    return 0;
}

// COMMENDS

// fetch commends
GCNetwork_Users::PlayerCommends GCNetwork_Users::GetPlayerCommends(uint64_t steamId, MYSQL *inventory_db)
{
    PlayerCommends commends = {0, 0, 0}; // Initialize all to 0

    char query[512];
    snprintf(query, sizeof(query),
             "SELECT type, COUNT(*) as count "
             "FROM player_commends "
             "WHERE receiver_steamid64 = %llu "
             "GROUP BY type",
             steamId);

    if (mysql_query(inventory_db, query) == 0)
    {
        MYSQL_RES *result = mysql_store_result(inventory_db);
        if (result)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result)))
            {
                int type = atoi(row[0]);
                int count = atoi(row[1]);

                switch (type)
                {
                case 1:
                    commends.friendly = count;
                    break;
                case 2:
                    commends.teaching = count;
                    break;
                case 3:
                    commends.leader = count;
                    break;
                }
            }
            mysql_free_result(result);
        }
    }
    else
    {
        logger::error("Failed to query commendations: %s", mysql_error(inventory_db));
    }

    return commends;
}

// get tokens
int GCNetwork_Users::GetPlayerCommendTokens(uint64_t steamId, MYSQL *inventory_db)
{
    // Default tokens (could be based on playtime or other factors)
    const int DEFAULT_TOKENS = 3;

    char query[512];
    snprintf(query, sizeof(query),
             "SELECT COUNT(DISTINCT receiver_steamid64) as unique_receivers "
             "FROM player_commends "
             "WHERE sender_steamid64 = %llu "
             "AND created_at > DATE_SUB(NOW(), INTERVAL 1 DAY)",
             steamId);

    if (mysql_query(inventory_db, query) == 0)
    {
        MYSQL_RES *result = mysql_store_result(inventory_db);
        if (result)
        {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row)
            {
                int used_tokens = row[0] ? atoi(row[0]) : 0;
                mysql_free_result(result);

                // Return remaining tokens
                return std::max(0, DEFAULT_TOKENS - used_tokens);
            }
            mysql_free_result(result);
        }
    }
    else
    {
        logger::error("Failed to query commend tokens: %s", mysql_error(inventory_db));
    }

    return DEFAULT_TOKENS; // Return default if query fails
}

// handle query
void GCNetwork_Users::HandleCommendPlayerQuery(SNetSocket_t p2psocket, void *message, uint32 msgsize,
                                               uint64_t senderSteamId, MYSQL *inventory_db)
{
    NetworkMessage netMsg(message, msgsize);
    CMsgGC_CC_ClientCommendPlayer request;
    if (!netMsg.ParseTo(&request))
    {
        logger::error("Failed to parse commend player query");
        return;
    }

    // Extract target information from the request
    uint32_t targetAccountId = request.account_id();
    uint64_t targetSteamId = ((uint64_t)1 << 56) | ((uint64_t)1 << 52) | ((uint64_t)1 << 32) | targetAccountId;

    // Check if sender has available commend tokens
    int availableTokens = GetPlayerCommendTokens(senderSteamId, inventory_db);

    // Check if already commended this player in each category
    int friendlyCommend = 0;
    int teachingCommend = 0;
    int leaderCommend = 0;

    char query[512];
    snprintf(query, sizeof(query),
             "SELECT type FROM player_commends "
             "WHERE sender_steamid64 = %llu "
             "AND receiver_steamid64 = %llu "
             "AND created_at > DATE_SUB(NOW(), INTERVAL 3 MONTH)",
             senderSteamId, targetSteamId);

    if (mysql_query(inventory_db, query) == 0)
    {
        MYSQL_RES *result = mysql_store_result(inventory_db);
        if (result)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result)))
            {
                int type = row[0] ? atoi(row[0]) : 0;
                switch (type)
                {
                case 1:
                    friendlyCommend = 1;
                    break;
                case 2:
                    teachingCommend = 1;
                    break;
                case 3:
                    leaderCommend = 1;
                    break;
                }
            }
            mysql_free_result(result);
        }
    }
    else
    {
        logger::error("Failed to query player commend history: %s", mysql_error(inventory_db));
    }

    // Get target player's total commendations for display
    PlayerCommends totalCommends = GetPlayerCommends(targetSteamId, inventory_db);

    CMsgGC_CC_ClientCommendPlayer response;
    response.set_account_id(targetAccountId);

    // Set existing commendation info
    auto commendation = response.mutable_commendation();
    commendation->set_cmd_friendly(friendlyCommend);
    commendation->set_cmd_teaching(teachingCommend);
    commendation->set_cmd_leader(leaderCommend);

    response.set_tokens(availableTokens);

    // Send response back to client
    NetworkMessage responseMsg = NetworkMessage::FromProto(response, k_EMsgGC_CC_GC2CL_ClientCommendPlayerQueryResponse);
    responseMsg.WriteToSocket(p2psocket, true);

    logger::info("Sent commendation query response: from=%llu, to=%llu, friendly=%d, teaching=%d, leader=%d, tokens=%d",
                 senderSteamId, targetSteamId, friendlyCommend, teachingCommend, leaderCommend, availableTokens);
}

// actual commend
void GCNetwork_Users::HandleCommendPlayer(SNetSocket_t p2psocket, void *message, uint32 msgsize,
                                          uint64_t senderSteamId, MYSQL *inventory_db)
{
    NetworkMessage netMsg(message, msgsize);
    CMsgGC_CC_ClientCommendPlayer request;

    if (!netMsg.ParseTo(&request))
    {
        logger::error("Failed to parse commend player request");
        return;
    }

    uint32_t targetAccountId = request.account_id();
    uint64_t targetSteamId = ((uint64_t)1 << 56) | ((uint64_t)1 << 52) | ((uint64_t)1 << 32) | targetAccountId;

    if (senderSteamId == 0)
    {
        logger::error("CommendPlayer: No valid session for this socket");
        return;
    }

    // Extract new commendation values from the request
    bool newFriendly = request.has_commendation() && request.commendation().cmd_friendly() > 0;
    bool newTeaching = request.has_commendation() && request.commendation().cmd_teaching() > 0;
    bool newLeader = request.has_commendation() && request.commendation().cmd_leader() > 0;

    // Get existing commendations for this sender/target pair
    char checkQuery[512];
    snprintf(checkQuery, sizeof(checkQuery),
             "SELECT type FROM player_commends "
             "WHERE sender_steamid64 = %llu "
             "AND receiver_steamid64 = %llu "
             "AND created_at > DATE_SUB(NOW(), INTERVAL 3 MONTH)",
             senderSteamId, targetSteamId);

    bool existingFriendly = false;
    bool existingTeaching = false;
    bool existingLeader = false;
    bool existingAny = false;

    if (mysql_query(inventory_db, checkQuery) == 0)
    {
        MYSQL_RES *result = mysql_store_result(inventory_db);
        if (result)
        {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(result)))
            {
                int type = row[0] ? atoi(row[0]) : 0;
                existingAny = true;
                switch (type)
                {
                case 1:
                    existingFriendly = true;
                    break;
                case 2:
                    existingTeaching = true;
                    break;
                case 3:
                    existingLeader = true;
                    break;
                }
            }
            mysql_free_result(result);
        }
    }
    else
    {
        logger::error("Failed to check existing commends: %s", mysql_error(inventory_db));
    }

    // Determine if we're adding new commendations or just modifying/removing
    bool addingNewCommendations = false;

    // We're adding a new commendation if any type is being added that didn't exist before
    if ((newFriendly && !existingFriendly) ||
        (newTeaching && !existingTeaching) ||
        (newLeader && !existingLeader))
    {
        addingNewCommendations = true;
    }

    // Check for tokens only if we're adding new commendations and not just modifying
    // existing ones. We don't check for tokens when:
    // 1. Only uncommending (removing commendations)
    // 2. Modifying existing commendations (uncommending one type, adding another)
    const int DEFAULT_TOKENS = 3;
    int availableTokens = DEFAULT_TOKENS;
    bool needToken = addingNewCommendations && !existingAny;

    if (needToken)
    {
        availableTokens = GetPlayerCommendTokens(senderSteamId, inventory_db);
        if (availableTokens <= 0)
        {
            // No tokens available
            logger::info("Commendation rejected: sender=%llu has no tokens available", senderSteamId);
            return;
        }
    }

    // Track if any changes were made
    bool commendAdded = false;
    bool commendRemoved = false;

    // Process friendly commendation
    if (newFriendly != existingFriendly)
    {
        if (newFriendly)
        {
            // Add friendly commendation
            char query[512];
            snprintf(query, sizeof(query),
                     "INSERT INTO player_commends (sender_steamid64, receiver_steamid64, type) "
                     "VALUES (%llu, %llu, 1)",
                     senderSteamId, targetSteamId);

            if (mysql_query(inventory_db, query) == 0)
            {
                commendAdded = true;
                logger::info("Friendly commendation added: sender=%llu, target=%llu", senderSteamId, targetSteamId);
            }
            else
            {
                logger::error("Failed to insert friendly commendation: %s", mysql_error(inventory_db));
            }
        }
        else
        {
            // Remove friendly commendation
            char query[512];
            snprintf(query, sizeof(query),
                     "DELETE FROM player_commends "
                     "WHERE sender_steamid64 = %llu "
                     "AND receiver_steamid64 = %llu "
                     "AND type = 1",
                     senderSteamId, targetSteamId);

            if (mysql_query(inventory_db, query) == 0)
            {
                commendRemoved = true;
                logger::info("Friendly commendation removed: sender=%llu, target=%llu", senderSteamId, targetSteamId);
            }
            else
            {
                logger::error("Failed to remove friendly commendation: %s", mysql_error(inventory_db));
            }
        }
    }

    // Process teaching commendation
    if (newTeaching != existingTeaching)
    {
        if (newTeaching)
        {
            // Add teaching commendation
            char query[512];
            snprintf(query, sizeof(query),
                     "INSERT INTO player_commends (sender_steamid64, receiver_steamid64, type) "
                     "VALUES (%llu, %llu, 2)",
                     senderSteamId, targetSteamId);

            if (mysql_query(inventory_db, query) == 0)
            {
                commendAdded = true;
                logger::info("Teaching commendation added: sender=%llu, target=%llu", senderSteamId, targetSteamId);
            }
            else
            {
                logger::error("Failed to insert teaching commendation: %s", mysql_error(inventory_db));
            }
        }
        else
        {
            // Remove teaching commendation
            char query[512];
            snprintf(query, sizeof(query),
                     "DELETE FROM player_commends "
                     "WHERE sender_steamid64 = %llu "
                     "AND receiver_steamid64 = %llu "
                     "AND type = 2",
                     senderSteamId, targetSteamId);

            if (mysql_query(inventory_db, query) == 0)
            {
                commendRemoved = true;
                logger::info("Teaching commendation removed: sender=%llu, target=%llu", senderSteamId, targetSteamId);
            }
            else
            {
                logger::error("Failed to remove teaching commendation: %s", mysql_error(inventory_db));
            }
        }
    }

    // Process leader commendation
    if (newLeader != existingLeader)
    {
        if (newLeader)
        {
            // Add leader commendation
            char query[512];
            snprintf(query, sizeof(query),
                     "INSERT INTO player_commends (sender_steamid64, receiver_steamid64, type) "
                     "VALUES (%llu, %llu, 3)",
                     senderSteamId, targetSteamId);

            if (mysql_query(inventory_db, query) == 0)
            {
                commendAdded = true;
                logger::info("Leader commendation added: sender=%llu, target=%llu", senderSteamId, targetSteamId);
            }
            else
            {
                logger::error("Failed to insert leader commendation: %s", mysql_error(inventory_db));
            }
        }
        else
        {
            // Remove leader commendation
            char query[512];
            snprintf(query, sizeof(query),
                     "DELETE FROM player_commends "
                     "WHERE sender_steamid64 = %llu "
                     "AND receiver_steamid64 = %llu "
                     "AND type = 3",
                     senderSteamId, targetSteamId);

            if (mysql_query(inventory_db, query) == 0)
            {
                commendRemoved = true;
                logger::info("Leader commendation removed: sender=%llu, target=%llu", senderSteamId, targetSteamId);
            }
            else
            {
                logger::error("Failed to remove leader commendation: %s", mysql_error(inventory_db));
            }
        }
    }

    // Log if any changes were made
    if (commendAdded || commendRemoved)
    {
        if (needToken)
        {
            logger::info("Commendation transaction complete: sender=%llu, target=%llu, tokens_remaining=%d",
                         senderSteamId, targetSteamId, availableTokens - 1);
        }
        else if (commendAdded && commendRemoved)
        {
            logger::info("Commendations modified: sender=%llu, target=%llu (no token used - swapped types)",
                         senderSteamId, targetSteamId);
        }
        else if (commendAdded)
        {
            logger::info("Commendations added to existing: sender=%llu, target=%llu (no token used - added to existing)",
                         senderSteamId, targetSteamId);
        }
        else
        {
            logger::info("Commendations removed: sender=%llu, target=%llu (no token used for uncommend)",
                         senderSteamId, targetSteamId);
        }
    }
    else
    {
        logger::info("No commendation changes: sender=%llu, target=%llu", senderSteamId, targetSteamId);
    }

    // No response needed
}

// REPORTS

// get tokens
int GCNetwork_Users::GetPlayerReportTokens(uint64_t steamId, MYSQL *inventory_db)
{
    // Default tokens (could be based on playtime or other factors)
    const int DEFAULT_TOKENS = 6;

    char query[512];
    snprintf(query, sizeof(query),
             "SELECT COUNT(DISTINCT receiver_steamid64) as unique_receivers "
             "FROM player_reports "
             "WHERE sender_steamid64 = %llu "
             "AND created_at > DATE_SUB(NOW(), INTERVAL 1 WEEK)",
             steamId);

    if (mysql_query(inventory_db, query) == 0)
    {
        MYSQL_RES *result = mysql_store_result(inventory_db);
        if (result)
        {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row)
            {
                int used_tokens = row[0] ? atoi(row[0]) : 0;
                mysql_free_result(result);

                // Return remaining tokens
                return std::max(0, DEFAULT_TOKENS - used_tokens);
            }
            mysql_free_result(result);
        }
    }
    else
    {
        logger::error("Failed to query report tokens: %s", mysql_error(inventory_db));
    }

    return DEFAULT_TOKENS; // Default if query fails
}

void GCNetwork_Users::HandlePlayerReport(SNetSocket_t p2psocket, void *message, uint32 msgsize,
                                         uint64_t senderSteamId, MYSQL *inventory_db)
{
    NetworkMessage netMsg(message, msgsize);
    CMsgGC_CC_CL2GC_ClientReportPlayer request;

    if (!netMsg.ParseTo(&request))
    {
        logger::error("Failed to parse player report request");
        return;
    }

    uint32_t targetAccountId = request.account_id();
    uint64_t targetSteamId = ((uint64_t)1 << 56) | ((uint64_t)1 << 52) | ((uint64_t)1 << 32) | targetAccountId;

    // Check if sender has available report tokens
    int availableTokens = GetPlayerReportTokens(senderSteamId, inventory_db);

    CMsgGC_CC_GC2CL_ClientReportResponse response;
    response.set_account_id(targetAccountId);
    response.set_confirmation_id(rand()); // Generate a random confirmation ID

    // Set match ID if provided
    if (request.has_match_id())
    {
        response.set_server_ip(0); // This would be the server IP for the match if available
    }

    if (availableTokens <= 0)
    {
        // No tokens available
        response.set_response_type(0);   // Error response
        response.set_response_result(2); // No tokens available
        response.set_tokens(0);

        logger::info("Report rejected: sender=%llu has no tokens available", senderSteamId);
    }
    else
    {
        // Check if player has already reported this target in the past week
        char checkQuery[512];
        snprintf(checkQuery, sizeof(checkQuery),
                 "SELECT COUNT(*) as report_count "
                 "FROM player_reports "
                 "WHERE sender_steamid64 = %llu "
                 "AND receiver_steamid64 = %llu "
                 "AND created_at > DATE_SUB(NOW(), INTERVAL 1 WEEK)",
                 senderSteamId, targetSteamId);

        bool canReport = true;
        if (mysql_query(inventory_db, checkQuery) == 0)
        {
            MYSQL_RES *result = mysql_store_result(inventory_db);
            if (result)
            {
                MYSQL_ROW row = mysql_fetch_row(result);
                if (row)
                {
                    int existing_reports = row[0] ? atoi(row[0]) : 0;
                    canReport = (existing_reports == 0);
                }
                mysql_free_result(result);
            }
        }
        else
        {
            logger::error("Failed to check existing reports: %s", mysql_error(inventory_db));
        }

        if (!canReport)
        {
            // Already reported this player in the past week
            response.set_response_type(0);   // Error response
            response.set_response_result(3); // Custom error: already reported this week
            response.set_tokens(availableTokens);

            logger::info("Report rejected: sender=%llu already reported target=%llu this week",
                         senderSteamId, targetSteamId);
        }
        else
        {
            // Process reports - check all possible report types
            bool reportSubmitted = false;
            uint64_t matchId = request.has_match_id() ? request.match_id() : 0;

            // Structure to track which report types were submitted
            struct ReportTypeInfo
            {
                const char *name;
                bool submitted;
            };

            ReportTypeInfo reportTypes[] = {
                {"aimbot", request.rpt_aimbot() > 0},               // Aim Hacking
                {"wallhack", request.rpt_wallhack() > 0},           // Wall Hacking
                {"speedhack", request.rpt_speedhack() > 0},         // Other Hacking
                {"teamharm", request.rpt_teamharm() > 0},           // Griefing
                {"textabuse", request.rpt_textabuse() > 0},         // Abusive Text Chat
                {"voiceabuse", request.rpt_voiceabuse() > 0}};      // Abusive Voice Chat

            // Count how many reports we're submitting
            int reportCount = 0;
            for (const auto &rt : reportTypes)
            {
                if (rt.submitted)
                    reportCount++;
            }

            if (reportCount == 0)
            {
                // No valid report types specified
                response.set_response_type(0);   // Error
                response.set_response_result(1); // General error
                response.set_tokens(availableTokens);

                logger::error("Report rejected: No valid report types specified by sender=%llu",
                              senderSteamId);
            }
            else
            {
                // Process all report types that were selected
                for (int i = 0; i < 6; i++)
                {
                    if (reportTypes[i].submitted)
                    {
                        char query[1024];
                        snprintf(query, sizeof(query),
                                 "INSERT INTO player_reports (sender_steamid64, receiver_steamid64, type, match_id) "
                                 "VALUES (%llu, %llu, %d, %llu)",
                                 senderSteamId, targetSteamId, i + 1, matchId);

                        if (mysql_query(inventory_db, query) == 0)
                        {
                            reportSubmitted = true;
                            logger::info("Report type '%s' submitted: sender=%llu, target=%llu",
                                         reportTypes[i].name, senderSteamId, targetSteamId);
                        }
                        else
                        {
                            logger::error("Failed to insert '%s' report: %s",
                                          reportTypes[i].name, mysql_error(inventory_db));
                        }
                    }
                }

                if (reportSubmitted)
                {
                    response.set_response_type(0);            // Success
                    response.set_response_result(0);          // Success
                    response.set_tokens(availableTokens - 1); // Decrease available tokens

                    logger::info("Reports processed successfully: sender=%llu, target=%llu, types=%d, tokens_remaining=%d",
                                 senderSteamId, targetSteamId, reportCount, availableTokens - 1);
                }
                else
                {
                    response.set_response_type(0);   // Error
                    response.set_response_result(1); // General error
                    response.set_tokens(availableTokens);

                    logger::error("All reports failed for sender=%llu, target=%llu",
                                  senderSteamId, targetSteamId);
                }
            }
        }
    }

    // Send response back to client
    NetworkMessage responseMsg = NetworkMessage::FromProto(response, k_EMsgGC_CC_GC2CL_ClientReportResponse);
    responseMsg.WriteToSocket(p2psocket, true);
}

// HELPERS

void GCNetwork_Users::GetPlayerMedals(uint64_t steamId, PlayerMedalsInfo *medals, MYSQL *inventory_db)
{
    std::string steamId2 = SteamID64ToSteamID2(steamId);

    char query[512];
    snprintf(query, sizeof(query),
             "SELECT item_id, equipped_t, equipped_ct "
             "FROM csgo_items "
             "WHERE owner_steamid2 = '%s' "
             "AND item_id LIKE 'collectible-%%'",
             steamId2.c_str());

    if (mysql_query(inventory_db, query) == 0)
    {
        MYSQL_RES *result = mysql_store_result(inventory_db);
        if (result)
        {
            MYSQL_ROW row;
            bool found_featured = false;

            while ((row = mysql_fetch_row(result)))
            {
                // parse defindex from item_id
                const char *item_id = row[0];
                const char *defindex_str = strchr(item_id, '-');
                if (!defindex_str)
                    continue;

                uint32_t defindex = atoi(defindex_str + 1); // skip '-'
                if (defindex == 0)
                    continue;

                // add
                medals->add_display_items_defidx(defindex);

                // for set_featured_display_item_defidx
                bool equipped_t = row[1] ? atoi(row[1]) == 1 : false;
                bool equipped_ct = row[2] ? atoi(row[2]) == 1 : false;

                if (equipped_t && equipped_ct && !found_featured)
                {
                    medals->set_featured_display_item_defidx(defindex);
                    found_featured = true;
                }
            }

            if (!found_featured)
            {
                medals->set_featured_display_item_defidx(0);
            }

            mysql_free_result(result);
        }
    }
    else
    {
        logger::error("Failed to query medals: %s", mysql_error(inventory_db));
    }
}

bool GCNetwork_Users::IsPlayerBanned(const std::string &steamId2, MYSQL *classiccounter_db)
{
    char query[512];
    snprintf(query, sizeof(query),
             "SELECT COUNT(*) as ban_count FROM sb_bans WHERE authid = '%s' AND length = 0 AND RemoveType IS NULL",
             steamId2.c_str());

    if (mysql_query(classiccounter_db, query) == 0)
    {
        MYSQL_RES *result = mysql_store_result(classiccounter_db);
        if (result)
        {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row)
            {
                int ban_count = row[0] ? atoi(row[0]) : 0;
                mysql_free_result(result);
                return ban_count > 0;
            }
            mysql_free_result(result);
        }
    }
    else
    {
        logger::error("Failed to query bans: %s", mysql_error(classiccounter_db));
    }
    return false;
}

void GCNetwork_Users::GetPlayerCooldownInfo(const std::string &steamId2, CMsgGC_CC_GC2CL_BuildMatchmakingHello &message, MYSQL *classiccounter_db)
{
    char query[512];
    snprintf(query, sizeof(query),
             "SELECT cooldown_reason, cooldown_expire, acknowledged "
             "FROM cooldowns "
             "WHERE sid = '%s' "
             "ORDER BY id DESC LIMIT 1",
             steamId2.c_str());

    if (mysql_query(classiccounter_db, query) == 0)
    {
        MYSQL_RES *result = mysql_store_result(classiccounter_db);
        if (result)
        {
            MYSQL_ROW row = mysql_fetch_row(result);
            if (row)
            {
                int acknowledged = row[2] ? atoi(row[2]) : 0;

                // only set if cooldown is unacknowledged
                if (acknowledged == 0)
                {
                    // get reason
                    int reason = row[0] ? atoi(row[0]) : 0;

                    // get time
                    time_t expire_time = row[1] ? atol(row[1]) : 0;
                    time_t current_time = time(NULL);

                    // calculate seconds
                    int penalty_seconds = 0;
                    if (expire_time > 0)
                    {
                        penalty_seconds = (expire_time > current_time) ? static_cast<int>(expire_time - current_time) : 0;
                    }

                    message.set_penalty_reason(reason);
                    message.set_penalty_seconds(penalty_seconds);

                    logger::info("Setting cooldown for %s: reason=%d, seconds=%d", steamId2.c_str(), reason, penalty_seconds);
                }
            }
            mysql_free_result(result);
        }
    }
    else
    {
        logger::error("Failed to query cooldown info: %s", mysql_error(classiccounter_db));
    }
}

// PROTOBUF MESSAGES

void GCNetwork_Users::BuildMatchmakingHello(CMsgGC_CC_GC2CL_BuildMatchmakingHello &message,
                                            uint64_t steamId, MYSQL *classiccounter_db,
                                            MYSQL *inventory_db, MYSQL *ranked_db)
{
    uint32_t accountId = steamId & 0xFFFFFFFF;
    message.set_account_id(accountId);

    std::string steamId2 = SteamID64ToSteamID2(steamId);

    // GLOBAL
    auto globalStats = message.mutable_global_stats();
    globalStats->set_players_online(0);
    globalStats->set_servers_online(0);
    globalStats->set_players_searching(0);
    globalStats->set_servers_available(0);
    globalStats->set_ongoing_matches(0);
    globalStats->set_search_time_avg(0);

    globalStats->set_main_post_url("http://blog.counter-strike.net/");

    globalStats->set_pricesheet_version(1680057676);
    globalStats->set_twitch_streams_version(2);
    globalStats->set_active_tournament_eventid(20);
    globalStats->set_active_survey_id(0);

    globalStats->set_required_appid_version(ClientVersion);

    // banned?
    message.set_vac_banned(IsPlayerBanned(steamId2, classiccounter_db) ? 1 : 0);

    // RANK
    auto ranking = message.mutable_ranking();
    ranking->set_account_id(accountId);
    ranking->set_rank_id(GetPlayerRankId(steamId2, ranked_db));
    ranking->set_wins(GetPlayerWins(steamId2, ranked_db));
    ranking->set_rank_change(0.0f);

    // COMMENDS
    auto commends = GetPlayerCommends(steamId, inventory_db);
    auto commendation = message.mutable_commendation();
    commendation->set_cmd_friendly(commends.friendly);
    commendation->set_cmd_teaching(commends.teaching);
    commendation->set_cmd_leader(commends.leader);

    // COOLDOWN
    GetPlayerCooldownInfo(steamId2, message, classiccounter_db);

    // uhhh soon...?
    message.set_player_level(1); // todo: fetch from db
    message.set_player_cur_xp(0);
    // idk what this does
    message.set_player_xp_bonus_flags(0);
}

void GCNetwork_Users::ViewPlayersProfile(SNetSocket_t p2psocket, void *message, uint32 msgsize,
                                         MYSQL *classiccounter_db, MYSQL *inventory_db, MYSQL *ranked_db)
{
    NetworkMessage netMsg(message, msgsize);
    CMsgGC_CC_CL2GC_ViewPlayersProfileRequest request;
    if (!netMsg.ParseTo(&request))
    {
        logger::error("Failed to parse view profile request");
        return;
    }

    uint32_t targetAccountId = request.account_id();
    uint64_t targetSteamId = ((uint64_t)1 << 56) | ((uint64_t)1 << 52) | ((uint64_t)1 << 32) | targetAccountId;
    std::string steamId2 = SteamID64ToSteamID2(targetSteamId);

    // logger::info("Processing profile request for account %u (STEAM_ID: %s)", targetAccountId, steamId2.c_str());

    CMsgGC_CC_GC2CL_ViewPlayersProfileResponse response;
    auto profile = response.add_account_profiles();

    // ACCOUNT
    profile->set_account_id(targetAccountId);

    // RANK
    auto ranking = profile->mutable_ranking();
    ranking->set_account_id(targetAccountId);
    ranking->set_rank_id(GetPlayerRankId(steamId2, ranked_db));
    ranking->set_wins(GetPlayerWins(steamId2, ranked_db));
    ranking->set_rank_change(0.0f);

    // COMMENDS
    auto commends = GetPlayerCommends(targetSteamId, inventory_db);
    auto commendation = profile->mutable_commendation();
    commendation->set_cmd_friendly(commends.friendly);
    commendation->set_cmd_teaching(commends.teaching);
    commendation->set_cmd_leader(commends.leader);

    // MEDALS
    auto medals = profile->mutable_medals();
    GetPlayerMedals(targetSteamId, medals, inventory_db);

    // OTHER (SOON)
    profile->set_player_level(1); // todo: fetch from db
    profile->set_player_cur_xp(0);

    NetworkMessage responseMsg = NetworkMessage::FromProto(response, k_EMsgGC_CC_GC2CL_ViewPlayersProfileResponse);
    responseMsg.WriteToSocket(p2psocket, true);

    logger::info("Sent profile data for account %u (medals: %d, commends: %d/%d/%d)",
                 targetAccountId,
                 medals->display_items_defidx_size(),
                 commends.friendly, commends.teaching, commends.leader);
}