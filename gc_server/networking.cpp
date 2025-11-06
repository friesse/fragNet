#include "stdafx.h"
#include "networking.hpp"
#include "networking_inventory.hpp"
#include "networking_users.hpp"
#include "networking_matchmaking.hpp"
#include "matchmaking_manager.hpp"
#include <sstream>

#include <steam/steam_api.h>
#include "logger.hpp"
#include "steam_network_message.hpp"
#include <steam/steam_gameserver.h>

void ip_to_str(char* ip, int ipsize, uint32_t uip)
{
    snprintf(ip, ipsize, "%u.%u.%u.%u"
        , (uip & 0xff000000) >> 24
        , (uip & 0x00ff0000) >> 16
        , (uip & 0x0000ff00) >> 8
        , (uip & 0x000000ff));
}

SNetListenSocket_t listen_socket;

GCNetwork::GCNetwork()
    : m_SocketStatusCallback()
    , m_mysql1(NULL)
    , m_mysql2(NULL)
    , m_mysql3(NULL)
{
    if (!GCNetwork_Inventory::Init()) {
        logger::error("Failed to initialize inventory system in GCNetwork constructor");
    }
}

GCNetwork::~GCNetwork()
{
    GCNetwork_Inventory::Cleanup();

    CloseDatabases();
    SteamGameServerNetworking()->DestroyListenSocket(listen_socket, true);
}

bool GCNetwork::InitDatabases() 
{
    // Initialize MySQL objects
    m_mysql1 = mysql_init(NULL);
    m_mysql2 = mysql_init(NULL);
    m_mysql3 = mysql_init(NULL);

    if (m_mysql1 == NULL || m_mysql2 == NULL || m_mysql3 == NULL) {
        logger::error("Failed to initialize MySQL objects");
        return false;
    }

    // 1ST DB CONNECTION
    // used for checking players bans, whitelist, cooldowns
    if (!mysql_real_connect(m_mysql1, "localhost", "gc", "61lol61w", "classiccounter", 3306, NULL, 0)) {
        logger::error("Failed to connect to database1: %s", mysql_error(m_mysql1));
        return false;
    }
    logger::info("Connected to classiccounter DB successfully!");

    // 2ND DB CONNECTION
    // used for inventory, equips, commends, reports
    if (!mysql_real_connect(m_mysql2, "localhost", "gc", "61lol61w", "ollum_inventory", 3306, NULL, 0)) {
        logger::error("Failed to connect to database2: %s", mysql_error(m_mysql2));
        mysql_close(m_mysql1);
        return false;
    }
    logger::info("Connected to ollum_inventory DB successfully!");

    // 3RD DB CONNECTION
    // used for rank, wins
    if (!mysql_real_connect(m_mysql3, "localhost", "gc", "61lol61w", "ollum_ranked", 3306, NULL, 0)) {
        logger::error("Failed to connect to database3: %s", mysql_error(m_mysql3));
        mysql_close(m_mysql1);
        mysql_close(m_mysql2);
        return false;
    }
    logger::info("Connected to ollum_ranked DB successfully!");

    return true;
}

bool GCNetwork::ExecuteQuery(MYSQL* connection, const char* query) {
    if (mysql_query(connection, query) != 0) {
        logger::error("Query execution failed: %s", mysql_error(connection));
        return false;
    }
    return true;
}

void GCNetwork::CloseDatabases() {
    if (m_mysql1) {
        mysql_close(m_mysql1);
        m_mysql1 = NULL;
    }
    if (m_mysql2) {
        mysql_close(m_mysql2);
        m_mysql2 = NULL;
    }
    if (m_mysql3) {
        mysql_close(m_mysql3);
        m_mysql3 = NULL;
    }
}

void GCNetwork::Init(const char* bind_ip, uint16 port) 
{
    // Convert IP string to SteamIPAddress_t
    SteamIPAddress_t steam_ip;
    steam_ip.m_eType = k_ESteamIPTypeIPv4;
    
    if (strcmp(bind_ip, "0.0.0.0") == 0) {
        // Bind to all interfaces
        steam_ip.m_unIPv4 = 0;
        logger::info("Attempting to bind GC network socket to 0.0.0.0:%d (all interfaces)", port);
    } else {
        // Parse specific IP
        unsigned int a, b, c, d;
        if (sscanf(bind_ip, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
            // Host byte order
            steam_ip.m_unIPv4 = (a << 24) | (b << 16) | (c << 8) | d;
            logger::info("Attempting to bind GC network socket to %s:%d", bind_ip, port);
        } else {
            logger::error("Invalid IP address format: %s, defaulting to 0.0.0.0", bind_ip);
            steam_ip.m_unIPv4 = 0;
        }
    }
    
    listen_socket = SteamGameServerNetworking()->CreateListenSocket(0, steam_ip, port, true);
    m_SocketStatusCallback.Register(this, &GCNetwork::SocketStatusCallback);

    SteamIPAddress_t uip; uint16 uport;
    SteamGameServerNetworking()->GetListenSocketInfo(listen_socket, &uip, &uport);

    char ip[16]; ip_to_str(ip, sizeof(ip), uip.m_unIPv4);
    logger::info("Created a listen socket on (%u) %s:%u", uip.m_unIPv4, ip, uport);
    
    // Log detailed information about what we bound to
    if (uip.m_unIPv4 == 0) {
        logger::warning("Socket bound to 0.0.0.0 (may be interpreted as localhost by Steamworks!)");
    } else if (strcmp(bind_ip, "127.0.0.1") == 0) {
        logger::warning("Socket bound to 127.0.0.1 (LOCALHOST ONLY - not accessible from network!)");
    } else {
        logger::info("Socket successfully bound to specific IP: %s", ip);
    }

    // init db connections
    if (!InitDatabases()) {
        logger::error("Failed to initialize databases");
    }
}

void GCNetwork::ReadAuthTicket(SNetSocket_t p2psocket, void* message, uint32 msgsize, MYSQL* classiccounter_db, MYSQL* inventory_db, MYSQL* ranked_db) 
{ 
    logger::info("Starting ReadAuthTicket - Raw message size: %u", msgsize);
    const uint8_t* bytes = static_cast<const uint8_t*>(message);
    logger::info("First 8 bytes: %02x %02x %02x %02x %02x %02x %02x %02x",
        bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7]);

    NetworkMessage netMsg(message, msgsize);

    CMsgGC_CC_GCWelcome welcomeMsg;
    if (!netMsg.ParseTo(&welcomeMsg)) {
        logger::error("Failed to parse welcome message");
        return;
    }

    logger::info("Parsed welcome message - Steam ID: %llu, Ticket Size: %u", 
                welcomeMsg.steam_id(), welcomeMsg.auth_ticket_size());

    // !!! important !!! get raw pointer to ticket data instead of using c_str()
    const void* ticketData = welcomeMsg.auth_ticket().data();

    SteamGameServer()->EndAuthSession(CSteamID(static_cast<uint64>(welcomeMsg.steam_id())));
    EBeginAuthSessionResult res = SteamGameServer()->BeginAuthSession(
        ticketData,  // Use raw ticket data
        welcomeMsg.auth_ticket_size(),
        CSteamID(static_cast<uint64>(welcomeMsg.steam_id())));

    switch (res)
    {
        case k_EBeginAuthSessionResultOK:
            logger::info("begin auth session result for %llu: OK!", welcomeMsg.steam_id());
            break;
        case k_EBeginAuthSessionResultInvalidTicket:
            logger::info("begin auth session result for %llu: INVALID TICKET!", welcomeMsg.steam_id());
            break;
        case k_EBeginAuthSessionResultDuplicateRequest:
            logger::info("begin auth session result for %llu: DUPLICATE REQUEST!", welcomeMsg.steam_id());
            break;
        case k_EBeginAuthSessionResultInvalidVersion:
            logger::info("begin auth session result for %llu: INVALID VERSION!", welcomeMsg.steam_id());
            break;
        case k_EBeginAuthSessionResultGameMismatch:
            logger::info("begin auth session result for %llu: GAME MISMATCH", welcomeMsg.steam_id());
            break;
        case k_EBeginAuthSessionResultExpiredTicket:
            logger::info("begin auth session result for %llu: EXPIRED TICKET!", welcomeMsg.steam_id());
            break;
    }

    if (res == k_EBeginAuthSessionResultOK)
    {
        // store session
        auto steamID = welcomeMsg.steam_id();

        // Whitelist disabled - all authenticated Steam users allowed
        logger::info("Auth accepted for user %llu (whitelist disabled)", steamID);
       
        // find/create session
        auto it = m_activeSessions.find(steamID);
        if (it != m_activeSessions.end())
        {
            // update existing one
            it->second.isAuthenticated = true;
            it->second.socket = p2psocket;
            it->second.updateActivity();
           
            // init lastCheckedItemId
            if (!it->second.itemIdInitialized) {
                it->second.lastCheckedItemId = GCNetwork_Inventory::GetLatestItemIdForUser(steamID, inventory_db);
                it->second.itemIdInitialized = true;
            }
        } else {
            // create new session
            ClientSessions session(CSteamID(static_cast<uint64>(steamID)));
            session.isAuthenticated = true;
            session.socket = p2psocket;
            session.lastCheckedItemId = GCNetwork_Inventory::GetLatestItemIdForUser(steamID, inventory_db);
            session.itemIdInitialized = true;
            m_activeSessions.insert(std::make_pair(steamID, session));
        }
       
        auto logIt = m_activeSessions.find(steamID);
        logger::info("Created/updated session for %llu with lastCheckedItemId %llu, total sessions: %zu",
                    steamID, logIt->second.lastCheckedItemId, m_activeSessions.size());
    
        auto response = Messages::CreateAuthConfirm(res);
        response.WriteToSocket(p2psocket, true);
        logger::info("Sent back an auth ticket confirmation to the client!");
    }
    else
    {
        logger::error("Auth failed with result: %d", res);
    }
}

void GCNetwork::CleanupSessions()
{
    time_t currentTime;
    time(&currentTime);

    std::vector<uint64> sessionsToRemove;

    // find sessions that are too old
    for (auto& pair : m_activeSessions)
    {
        if ((currentTime - pair.second.lastActivity) > (24 * 60 * 60)) // 24h
        {
            sessionsToRemove.push_back(pair.first);
        }
    }

    // remove
    for (auto& id : sessionsToRemove)
    {
        logger::info("Removing expired session for %llu", id);
        m_activeSessions.erase(id);
    }
}

uint64_t GCNetwork::GetSessionSteamId(SNetSocket_t socket) {
    for (auto& pair : m_activeSessions) {
        if (pair.second.socket == socket) {
            return pair.first;
        }
    }
    return 0;
}

void GCNetwork::CheckNewItemsForActiveSessions() 
{
    for (auto& pair : m_activeSessions) {
        auto& session = pair.second;
        uint64_t steamId = session.steamID.ConvertToUint64();
        
        // Skip if not authenticated, not initialized, or no valid socket
        if (!session.isAuthenticated || 
            !session.itemIdInitialized || 
            session.socket == k_HSteamNetConnection_Invalid) {
            continue;
        }
        
        // Check for new items and update the session's lastCheckedItemId if found
        if (GCNetwork_Inventory::CheckAndSendNewItemsSince(
                session.socket, 
                steamId, 
                session.lastCheckedItemId, 
                m_mysql2)) {
            // Update the session's last activity time if items were found and sent
            session.updateActivity();
        }
    }
}

void SendHeartbeat(SNetSocket_t p2psocket) {
    auto message = Messages::CreateHeartbeat();
    message.WriteToSocket(p2psocket, true);
}

void GCNetwork::Update() 
{
    // cleanup sessions
    static int updateCounter = 0;
    static int itemCheckCounter = 0;
    static int matchmakingCounter = 0;
    
    if (++updateCounter >= 1000) {
        CleanupSessions();
        updateCounter = 0;
    }
    
    // check for new items every 5 seconds
    if (++itemCheckCounter >= 250) {
        CheckNewItemsForActiveSessions();
        itemCheckCounter = 0;
    }
    
    // update matchmaking every frame
    if (++matchmakingCounter >= 50) { // Update every ~1 second at 20 ticks/sec
        MatchmakingManager::GetInstance()->Update();
        matchmakingCounter = 0;
    }

    SteamGameServer_RunCallbacks();

    SNetSocket_t p2psocket;
    uint32_t msgsize;

    while (SteamGameServerNetworking()->IsDataAvailable(listen_socket, &msgsize, &p2psocket)) {
        std::vector<uint8_t> buffer(msgsize);
        
        if (!SteamGameServerNetworking()->RetrieveDataFromSocket(
            p2psocket, buffer.data(), msgsize, &msgsize)) {
            continue;
        }

        // get raw 32-bit type
        uint32_t raw_type;
        memcpy(&raw_type, buffer.data(), sizeof(uint32_t));

        // unmask dat bitch
        constexpr uint32_t CCProtoMask = 0x90000000;
        uint32_t real_type = raw_type & ~CCProtoMask;

        logger::info("Received message - Raw: %08X, Unmasked: %u (0x%X)", 
                    raw_type, real_type, real_type);

        switch (real_type) {
            case k_EMsgGC_CC_GCWelcome:
                logger::info("Received GCWelcome");
                this->ReadAuthTicket(p2psocket, buffer.data(), msgsize, m_mysql1, m_mysql2, m_mysql3);
                break;

            case k_EMsgGC_CC_GCConfirmAuth:
                logger::info("Received GCConfirmAuth");
                break;

            case k_EMsgGC_CC_CL2GC_BuildMatchmakingHelloRequest:
                logger::info("Received BuildMatchmakingHelloRequest");
                {
                    NetworkMessage netMsg(buffer.data(), msgsize);
                    CMsgGC_CC_CL2GC_BuildMatchmakingHelloRequest request;
                    if (netMsg.ParseTo(&request)) {
                        CMsgGC_CC_GC2CL_BuildMatchmakingHello response;
                        GCNetwork_Users::BuildMatchmakingHello(response, request.steam_id(), m_mysql1, m_mysql2, m_mysql3);
                        NetworkMessage matchmakingMsg = NetworkMessage::FromProto(response, k_EMsgGC_CC_GC2CL_BuildMatchmakingHello);
                        matchmakingMsg.WriteToSocket(p2psocket, true);
                    }
                }
                break;

            case k_EMsgGC_CC_CL2GC_SOCacheSubscribedRequest:
                logger::info("Received SOCacheSubscribedRequest");
                {
                    NetworkMessage netMsg(buffer.data(), msgsize);
                    CMsgGC_CC_CL2GC_SOCacheSubscribedRequest request;
                    if (netMsg.ParseTo(&request)) {
                        GCNetwork_Inventory::SendSOCache(p2psocket, request.steam_id(), m_mysql2);
                    }
                }
                break;

            case k_EMsgGC_CC_GCHeartbeat:
                logger::info("Received GCHeartbeat");
                SendHeartbeat(p2psocket);
                break;

            // INVENTORY ACTIONS

            case k_EMsgGC_CC_CL2GC_ItemAcknowledged:
                logger::info("Received ItemAcknowledged");
                {
                    NetworkMessage netMsg(buffer.data(), msgsize);
                    CMsgGC_CC_CL2GC_ItemAcknowledged request;
                    if (netMsg.ParseTo(&request)) {
                        uint64_t steamId = GetSessionSteamId(p2psocket);
                        if (steamId != 0) {
                            GCNetwork_Inventory::ProcessClientAcknowledgment(p2psocket, steamId, request, m_mysql2);
                        } else {
                            logger::error("ItemAcknowledged: No valid session for this socket");
                        }
                    }
                }
                break;

            case k_EMsgGC_CC_CL2GC_UnlockCrate:
                logger::info("Received UnlockCrate request");
                {
                    NetworkMessage netMsg(buffer.data(), msgsize);
                    CMsgGC_CC_CL2GC_UnlockCrate request;
                    if (netMsg.ParseTo(&request)) {
                        uint64_t steamId = GetSessionSteamId(p2psocket);
                        if (steamId != 0) {
                            uint64_t crateItemId = request.crate_id();
                            bool success = GCNetwork_Inventory::HandleUnboxCrate(p2psocket, steamId, crateItemId, m_mysql2);
                            
                            if (success) {
                                logger::info("Successfully processed crate unlock for user %llu, crate %llu", 
                                            steamId, crateItemId);
                            } else {
                                logger::error("Failed to process crate unlock for user %llu, crate %llu", 
                                            steamId, crateItemId);
                            }
                        } else {
                            logger::error("UnlockCrate: No valid session for this socket");
                        }
                    } else {
                        logger::error("UnlockCrate: Failed to parse request");
                    }
                }
                break;

            case k_EMsgGC_CC_CL2GC_AdjustItemEquippedState:
                logger::info("Received AdjustItemEquippedState request");
                {
                    NetworkMessage netMsg(buffer.data(), msgsize);
                    CMsgGC_CC_CL2GC_AdjustItemEquippedState request;
                    if (netMsg.ParseTo(&request)) {
                        uint64_t steamId = GetSessionSteamId(p2psocket);
                        if (steamId != 0) {
                            uint64_t itemId = request.item_id();
                            uint32_t classId = request.new_class();
                            uint32_t slotId = request.new_slot();
                            
                            logger::info("AdjustItemEquippedState: User %llu wants to equip item %llu in class %u slot %u",
                                        steamId, itemId, classId, slotId);
                            
                            bool success;
                            success = GCNetwork_Inventory::EquipItem(p2psocket, steamId, itemId, classId, slotId, m_mysql2);
                        }
                    }
                }
                break;

            case k_EMsgGC_CC_DeleteItem:
                logger::info("Received DeleteItem request");
                {
                    NetworkMessage netMsg(buffer.data(), msgsize);
                    CMsgGC_CC_DeleteItem request;
                    if (netMsg.ParseTo(&request)) {
                        uint64_t steamId = GetSessionSteamId(p2psocket);
                        if (steamId != 0) {
                            uint64_t itemId = request.item_id();
                            GCNetwork_Inventory::DeleteItem(p2psocket, steamId, itemId, m_mysql2);
                        } else {
                            logger::error("DeleteItem: No valid session for this socket");
                        }
                    }
                }
                break;

            case k_EMsgGC_CC_CL2GC_NameItem:
                logger::info("Received NameItem request");
                {
                    NetworkMessage netMsg(buffer.data(), msgsize);
                    CMsgGC_CC_CL2GC_NameItem request;
                    if (netMsg.ParseTo(&request)) {
                        uint64_t steamId = GetSessionSteamId(p2psocket);
                        if (steamId != 0) {
                            uint64_t itemId = request.item_id();
                            std::string name = request.name();
                            
                            logger::info("NameItem: User %llu wants to name item %llu to '%s'",
                                        steamId, itemId, name.c_str());
                            
                            bool success = GCNetwork_Inventory::HandleNameItem(
                                p2psocket, steamId, itemId, name, m_mysql2);
                        }
                    }
                }
                break;
            
            case k_EMsgGC_CC_CL2GC_NameBaseItem:
                logger::info("Received NameBaseItem request");
                {
                    NetworkMessage netMsg(buffer.data(), msgsize);
                    CMsgGC_CC_CL2GC_NameBaseItem request;
                    if (netMsg.ParseTo(&request)) {
                        uint64_t steamId = GetSessionSteamId(p2psocket);
                        if (steamId != 0) {
                            uint32_t defIndex = request.defindex();
                            std::string name = request.name();
                            
                            logger::info("NameBaseItem: User %llu wants to create base item %u with name '%s'",
                                        steamId, defIndex, name.c_str());
                            
                            bool success = GCNetwork_Inventory::HandleNameBaseItem(
                                p2psocket, steamId, defIndex, name, m_mysql2);
                        }
                    }
                }
                break;
            
            case k_EMsgGC_CC_CL2GC_RemoveItemName:
                logger::info("Received RemoveItemName request");
                {
                    NetworkMessage netMsg(buffer.data(), msgsize);
                    CMsgGC_CC_CL2GC_RemoveItemName request;
                    if (netMsg.ParseTo(&request)) {
                        uint64_t steamId = GetSessionSteamId(p2psocket);
                        if (steamId != 0) {
                            uint64_t itemId = request.item_id();
                            
                            logger::info("RemoveItemName: User %llu wants to remove name from item %llu",
                                        steamId, itemId);
                            
                            bool success = GCNetwork_Inventory::HandleRemoveItemName(
                                p2psocket, steamId, itemId, m_mysql2);
                        }
                    }
                }
                break;

            case k_EMsgGC_CC_CL2GC_ApplySticker:
                logger::info("Received ApplySticker request");
                {
                    NetworkMessage netMsg(buffer.data(), msgsize);
                    CMsgGC_CC_CL2GC_ApplySticker request;
                    if (netMsg.ParseTo(&request)) {
                        uint64_t steamId = GetSessionSteamId(p2psocket);
                        if (steamId != 0) {
                            bool isApplying = request.has_sticker_item_id() && request.sticker_item_id() > 0;
                            
                            logger::info("ApplySticker: User %llu is %s sticker, item: %llu, sticker: %llu, slot: %u", 
                                        steamId, 
                                        isApplying ? "applying" : "scraping",
                                        request.has_item_item_id() ? request.item_item_id() : 0,
                                        request.has_sticker_item_id() ? request.sticker_item_id() : 0,
                                        request.has_sticker_slot() ? request.sticker_slot() : 0);
                            
                            bool success = GCNetwork_Inventory::ProcessStickerAction(
                                p2psocket, steamId, request, m_mysql2);
                        }
                    }
                }
                break;



            // OTHERS

            /*case k_EMsgGC_CC_CL2GC_StorePurchaseInit:
                logger::info("Received StorePurchaseInit request");
                {
                    NetworkMessage netMsg(buffer.data(), msgsize);
                    CMsgGC_CC_CL2GC_StorePurchaseInit request;
                    if (netMsg.ParseTo(&request)) {
                        uint64_t steamId = GetSessionSteamId(p2psocket);
                        if (steamId != 0) {
                            logger::info("StorePurchaseInit: User %llu is making a purchase with %d items", 
                                        steamId, request.line_items_size());
                            
                            bool success = GCNetwork_Inventory::HandleStorePurchaseInit(
                                p2psocket, steamId, request, m_mysql2);
                                
                            if (success) {
                                logger::info("Successfully processed store purchase for user %llu", steamId);
                            } else {
                                logger::error("Failed to process store purchase for user %llu", steamId);
                            }
                        } else {
                            logger::error("StorePurchaseInit: No valid session for this socket");
                        }
                    } else {
                        logger::error("StorePurchaseInit: Failed to parse request");
                    }
                }
                break;*/

            case k_EMsgGC_CC_CL2GC_ClientCommendPlayerQuery:
                logger::info("Received commendation query request");
                {
                    uint64_t querySenderId = GetSessionSteamId(p2psocket);
                    GCNetwork_Users::HandleCommendPlayerQuery(p2psocket, buffer.data(), msgsize, querySenderId, m_mysql2);
                }
                break;

            case k_EMsgGC_CC_CL2GC_ClientCommendPlayer:
                logger::info("Received commendation request");
                {
                    uint64_t senderSteamId = GetSessionSteamId(p2psocket);
                    if (senderSteamId != 0) {
                        GCNetwork_Users::HandleCommendPlayer(p2psocket, buffer.data(), msgsize, senderSteamId, m_mysql2);
                    } else {
                        logger::error("CommendPlayer: No valid session for this socket");
                    }
                }
                break;
                
            case k_EMsgGC_CC_CL2GC_ClientReportPlayer:
                logger::info("Received player report request");
                {
                    uint64_t senderSteamId = GetSessionSteamId(p2psocket);
                    if (senderSteamId != 0) {
                        GCNetwork_Users::HandlePlayerReport(p2psocket, buffer.data(), msgsize, senderSteamId, m_mysql2);
                    } else {
                        logger::error("ReportPlayer: No valid session for this socket");
                    }
                }
                break;

            case k_EMsgGC_CC_CL2GC_ViewPlayersProfileRequest:
                logger::info("Received view profile request");
                GCNetwork_Users::ViewPlayersProfile(p2psocket, buffer.data(), msgsize, m_mysql1, m_mysql2, m_mysql3);
                break;

            // MATCHMAKING MESSAGES
            case k_EMsgGCCStrike15_v2_MatchmakingClient2GCHello:
                logger::info("Received MatchmakingClient2GCHello");
                {
                    uint64_t steamId = GetSessionSteamId(p2psocket);
                    if (steamId != 0) {
                        GCNetwork_Matchmaking::HandleMatchmakingClient2GCHello(p2psocket, buffer.data(), msgsize, steamId, m_mysql3);
                    } else {
                        logger::error("MatchmakingClient2GCHello: No valid session for this socket");
                    }
                }
                break;

            case k_EMsgGCCStrike15_v2_MatchmakingStart:
                logger::info("Received MatchmakingStart");
                {
                    uint64_t steamId = GetSessionSteamId(p2psocket);
                    if (steamId != 0) {
                        GCNetwork_Matchmaking::HandleMatchmakingStart(p2psocket, buffer.data(), msgsize, steamId, m_mysql3);
                    } else {
                        logger::error("MatchmakingStart: No valid session for this socket");
                    }
                }
                break;

            case k_EMsgGCCStrike15_v2_MatchmakingStop:
                logger::info("Received MatchmakingStop");
                {
                    uint64_t steamId = GetSessionSteamId(p2psocket);
                    if (steamId != 0) {
                        GCNetwork_Matchmaking::HandleMatchmakingStop(p2psocket, buffer.data(), msgsize, steamId);
                    } else {
                        logger::error("MatchmakingStop: No valid session for this socket");
                    }
                }
                break;

            case k_EMsgGCCStrike15_v2_MatchmakingClient2GCAccept:
                logger::info("Received MatchmakingClient2GCAccept");
                {
                    uint64_t steamId = GetSessionSteamId(p2psocket);
                    if (steamId != 0) {
                        GCNetwork_Matchmaking::HandleMatchmakingAccept(p2psocket, buffer.data(), msgsize, steamId);
                    } else {
                        logger::error("MatchmakingAccept: No valid session for this socket");
                    }
                }
                break;

            case k_EMsgGCCStrike15_v2_MatchmakingClient2GCDecline:
                logger::info("Received MatchmakingClient2GCDecline");
                {
                    uint64_t steamId = GetSessionSteamId(p2psocket);
                    if (steamId != 0) {
                        GCNetwork_Matchmaking::HandleMatchmakingDecline(p2psocket, buffer.data(), msgsize, steamId);
                    } else {
                        logger::error("MatchmakingDecline: No valid session for this socket");
                    }
                }
                break;

            case k_EMsgGCCStrike15_v2_MatchmakingServerMatchEnd:
                logger::info("Received MatchmakingServerMatchEnd");
                {
                    uint64_t steamId = GetSessionSteamId(p2psocket);
                    if (steamId != 0) {
                        GCNetwork_Matchmaking::HandleMatchEnd(p2psocket, buffer.data(), msgsize, steamId, m_mysql3);
                    } else {
                        logger::error("MatchEnd: No valid session for this socket");
                    }
                }
                break;

            case k_EMsgGCCStrike15_v2_MatchmakingServerRoundStats:
                logger::info("Received MatchmakingServerRoundStats");
                {
                    uint64_t steamId = GetSessionSteamId(p2psocket);
                    if (steamId != 0) {
                        GCNetwork_Matchmaking::HandleMatchRoundStats(p2psocket, buffer.data(), msgsize, steamId);
                    } else {
                        logger::error("RoundStats: No valid session for this socket");
                    }
                }
                break;

            default:
                logger::error("Unknown message type: %u", real_type);
                break;
        }
    }
}

// WHITELIST DISABLED - Function no longer used
/*
bool GCNetwork::IsUserWhitelisted(uint64_t steamID64, MYSQL* classiccounter_db) 
{
    if (m_maintenanceMode) {
        for (const auto& allowedID : m_maintenanceAllowlist) {
            if (allowedID == steamID64) {
                logger::info("User %llu allowed in maintenance mode", steamID64);
                return true;
            }
        }
        logger::info("User %llu denied - maintenance mode active and not in allowlist", steamID64);
        return false;
    }

    std::string steamID2 = GCNetwork_Users::SteamID64ToSteamID2(steamID64);
    std::string query = "SELECT COUNT(*) FROM mysql_whitelist WHERE steamid = '" + steamID2 + "'";

    if (mysql_query(classiccounter_db, query.c_str()) != 0) {
        logger::error("Whitelist check failed: %s", mysql_error(classiccounter_db));
        return false;
    }

    MYSQL_RES* result = mysql_store_result(classiccounter_db);
    if (!result) {
        logger::error("Failed to retrieve whitelist result: %s", mysql_error(classiccounter_db));
        return false;
    }

    MYSQL_ROW row = mysql_fetch_row(result);
    int count = row ? atoi(row[0]) : 0;
    mysql_free_result(result);

    bool isWhitelisted = (count > 0);

    if (isWhitelisted) {
        logger::info("User %llu (SteamID2: %s) is whitelisted!", steamID64, steamID2.c_str());
    } else {
        logger::info("User %llu (SteamID2: %s) is not whitelisted.", steamID64, steamID2.c_str());
    }

    return isWhitelisted;
}
*/

void GCNetwork::SocketStatusCallback(SocketStatusCallback_t* pParam) 
{
    uint64_t steamId = pParam->m_steamIDRemote.ConvertToUint64();
    logger::info("Networking: received a socket connection from %llu", steamId);
    
    // store socket in session
    auto it = m_activeSessions.find(steamId);
    if (it != m_activeSessions.end()) 
    {
        // update session
        it->second.socket = pParam->m_hSocket;
        it->second.updateActivity();
    }
    else
    {
        // create session
        ClientSessions newSession(pParam->m_steamIDRemote);
        newSession.socket = pParam->m_hSocket;
        m_activeSessions.insert(std::make_pair(steamId, newSession));
    }
}