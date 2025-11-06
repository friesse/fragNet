#pragma once

#include <string>
#include <map>
#include <vector>
#include <chrono>
#include "steam/steam_api.h"
#include "cstrike15_gcmessages.pb.h"

// Game server registration and communication
class GameServerManager {
public:
    struct ServerInfo {
        std::string address;
        uint16_t port;
        uint64_t serverSteamId;
        SNetSocket_t socket;
        bool isAvailable;
        uint64_t currentMatchId;
        uint32_t maxPlayers;
        uint32_t currentPlayers;
        std::string currentMap;
        std::chrono::steady_clock::time_point lastHeartbeat;
        bool isAuthenticated;
        
        ServerInfo() : 
            port(0), 
            serverSteamId(0),
            socket(k_HSteamNetConnection_Invalid),
            isAvailable(false),
            currentMatchId(0),
            maxPlayers(10),
            currentPlayers(0),
            isAuthenticated(false) {}
    };

private:
    static GameServerManager* s_instance;
    std::map<uint64_t, ServerInfo> m_servers; // Key: serverSteamId
    std::map<SNetSocket_t, uint64_t> m_socketToServer;
    
    const std::chrono::seconds SERVER_TIMEOUT{30};
    
    GameServerManager() {}

public:
    static GameServerManager* GetInstance();
    static void Destroy();
    
    // Server registration
    bool RegisterServer(SNetSocket_t socket, uint64_t serverSteamId, const std::string& address, uint16_t port);
    void UnregisterServer(uint64_t serverSteamId);
    void UpdateServerStatus(uint64_t serverSteamId, const CMsgGCCStrike15_v2_MatchmakingGC2ServerReserve& status);
    
    // Server queries
    ServerInfo* FindAvailableServer();
    ServerInfo* GetServerInfo(uint64_t serverSteamId);
    ServerInfo* GetServerBySocket(SNetSocket_t socket);
    bool IsServerAvailable(uint64_t serverSteamId) const;
    
    // Match assignment
    bool AssignMatchToServer(uint64_t serverSteamId, uint64_t matchId);
    void ReleaseServer(uint64_t serverSteamId);
    
    // Heartbeat and health
    void UpdateHeartbeat(uint64_t serverSteamId);
    void CheckServerTimeouts();
    
    // Message builders for game servers
    void BuildServerReservation(CMsgGCCStrike15_v2_MatchmakingGC2ServerReserve& message, 
                               uint64_t matchId,
                               const std::vector<uint64_t>& playerSteamIds,
                               const std::string& mapName);
    
    // Statistics
    size_t GetAvailableServerCount() const;
    size_t GetTotalServerCount() const;
    std::vector<ServerInfo> GetAllServers() const;
    
    ~GameServerManager();
};
