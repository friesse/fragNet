#include "gameserver_manager.hpp"
#include "logger.hpp"
#include <algorithm>

GameServerManager* GameServerManager::s_instance = nullptr;

GameServerManager* GameServerManager::GetInstance() {
    if (!s_instance) {
        s_instance = new GameServerManager();
    }
    return s_instance;
}

void GameServerManager::Destroy() {
    if (s_instance) {
        delete s_instance;
        s_instance = nullptr;
    }
}

GameServerManager::~GameServerManager() {
    m_servers.clear();
    m_socketToServer.clear();
}

bool GameServerManager::RegisterServer(SNetSocket_t socket, uint64_t serverSteamId, 
                                       const std::string& address, uint16_t port) {
    ServerInfo info;
    info.address = address;
    info.port = port;
    info.serverSteamId = serverSteamId;
    info.socket = socket;
    info.isAvailable = true;
    info.currentMatchId = 0;
    info.lastHeartbeat = std::chrono::steady_clock::now();
    info.isAuthenticated = true;
    
    m_servers[serverSteamId] = info;
    m_socketToServer[socket] = serverSteamId;
    
    logger::info("Game server registered: %s:%u (SteamID: %llu)", 
                 address.c_str(), port, serverSteamId);
    
    return true;
}

void GameServerManager::UnregisterServer(uint64_t serverSteamId) {
    auto it = m_servers.find(serverSteamId);
    if (it != m_servers.end()) {
        m_socketToServer.erase(it->second.socket);
        logger::info("Game server unregistered: %s:%u", 
                     it->second.address.c_str(), it->second.port);
        m_servers.erase(it);
    }
}

void GameServerManager::UpdateServerStatus(uint64_t serverSteamId, 
                                           const CMsgGCCStrike15_v2_MatchmakingGC2ServerReserve& status) {
    auto it = m_servers.find(serverSteamId);
    if (it != m_servers.end()) {
        // Update server status based on reservation info
        // This would be called when server reports its status
        UpdateHeartbeat(serverSteamId);
    }
}

GameServerManager::ServerInfo* GameServerManager::FindAvailableServer() {
    for (auto& [steamId, server] : m_servers) {
        if (server.isAvailable && server.isAuthenticated) {
            return &server;
        }
    }
    return nullptr;
}

GameServerManager::ServerInfo* GameServerManager::GetServerInfo(uint64_t serverSteamId) {
    auto it = m_servers.find(serverSteamId);
    if (it != m_servers.end()) {
        return &it->second;
    }
    return nullptr;
}

GameServerManager::ServerInfo* GameServerManager::GetServerBySocket(SNetSocket_t socket) {
    auto it = m_socketToServer.find(socket);
    if (it != m_socketToServer.end()) {
        return GetServerInfo(it->second);
    }
    return nullptr;
}

bool GameServerManager::IsServerAvailable(uint64_t serverSteamId) const {
    auto it = m_servers.find(serverSteamId);
    return it != m_servers.end() && it->second.isAvailable;
}

bool GameServerManager::AssignMatchToServer(uint64_t serverSteamId, uint64_t matchId) {
    auto it = m_servers.find(serverSteamId);
    if (it != m_servers.end() && it->second.isAvailable) {
        it->second.isAvailable = false;
        it->second.currentMatchId = matchId;
        logger::info("Assigned match %llu to server %s:%u", 
                     matchId, it->second.address.c_str(), it->second.port);
        return true;
    }
    return false;
}

void GameServerManager::ReleaseServer(uint64_t serverSteamId) {
    auto it = m_servers.find(serverSteamId);
    if (it != m_servers.end()) {
        it->second.isAvailable = true;
        it->second.currentMatchId = 0;
        it->second.currentPlayers = 0;
        logger::info("Released server %s:%u", 
                     it->second.address.c_str(), it->second.port);
    }
}

void GameServerManager::UpdateHeartbeat(uint64_t serverSteamId) {
    auto it = m_servers.find(serverSteamId);
    if (it != m_servers.end()) {
        it->second.lastHeartbeat = std::chrono::steady_clock::now();
    }
}

void GameServerManager::CheckServerTimeouts() {
    auto now = std::chrono::steady_clock::now();
    std::vector<uint64_t> timedOutServers;
    
    for (const auto& [steamId, server] : m_servers) {
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - server.lastHeartbeat);
        if (elapsed > SERVER_TIMEOUT) {
            timedOutServers.push_back(steamId);
        }
    }
    
    for (uint64_t steamId : timedOutServers) {
        logger::warning("Game server timed out: SteamID %llu", steamId);
        UnregisterServer(steamId);
    }
}

void GameServerManager::BuildServerReservation(CMsgGCCStrike15_v2_MatchmakingGC2ServerReserve& message,
                                               uint64_t matchId,
                                               const std::vector<uint64_t>& playerSteamIds,
                                               const std::string& mapName) {
    message.set_serverid(matchId);
    message.set_map(mapName);
    
    // Add all players to reservation
    for (uint64_t steamId : playerSteamIds) {
        auto* account = message.add_account_ids();
        *account = steamId & 0xFFFFFFFF; // Account ID
    }
    
    logger::info("Built server reservation for match %llu with %zu players on %s",
                 matchId, playerSteamIds.size(), mapName.c_str());
}

size_t GameServerManager::GetAvailableServerCount() const {
    size_t count = 0;
    for (const auto& [steamId, server] : m_servers) {
        if (server.isAvailable) {
            count++;
        }
    }
    return count;
}

size_t GameServerManager::GetTotalServerCount() const {
    return m_servers.size();
}

std::vector<GameServerManager::ServerInfo> GameServerManager::GetAllServers() const {
    std::vector<ServerInfo> servers;
    for (const auto& [steamId, server] : m_servers) {
        servers.push_back(server);
    }
    return servers;
}
