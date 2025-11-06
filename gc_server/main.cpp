#include "stdafx.h"
#include "main.hpp"
#include "platform.hpp"

#include <dlfcn.h>
#define STEAM_API_EXPORTS
#include <steam/steam_gameserver.h>
#include <cstring>
#include <cstdlib>  // for getenv, atoi

// Configuration - Can be overridden by environment variables GC_BIND_IP and GC_PORT
const char* get_bind_ip() {
    const char* env_ip = getenv("GC_BIND_IP");
    return env_ip ? env_ip : "0.0.0.0";  // Bind to all interfaces (allows both local and remote connections)
}

uint16 get_game_port() {
    const char* env_port = getenv("GC_PORT");
    return env_port ? (uint16)atoi(env_port) : 27016;
}

const char* BIND_IP = get_bind_ip();
const uint16 GAME_PORT = get_game_port();

// Convert IP string to uint32 in host byte order
uint32 ip_string_to_uint32(const char* ip_str) {
    if (strcmp(ip_str, "0.0.0.0") == 0) {
        return 0; // INADDR_ANY
    }
    
    unsigned int a, b, c, d;
    if (sscanf(ip_str, "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
        // Steam uses HOST byte order for unIP parameter
        return (a << 24) | (b << 16) | (c << 8) | d;
    }
    
    logger::error("Invalid IP address format: %s, defaulting to 0.0.0.0", ip_str);
    return 0;
}

int main() {
#ifdef _WIN32
    if (!platform::win32_enable_vt_mode()) {
        printf("Couldn't enable virtual terminal mode! Continuing with colors disabled!");
        logger::disable_colors();
    }
#endif

#ifdef _WIN32
    if (!GetEnvironmentVariableA("SteamAppId", nullptr, 0)) {
        SetEnvironmentVariableA("SteamAppId", "730");
    }
#else
    setenv("SteamAppId", "730", 0);
#endif

    uint32 bind_ip = ip_string_to_uint32(BIND_IP);
    
    logger::info("Initializing Steam Game Server on %s:%d", BIND_IP, GAME_PORT);
    
    if (!SteamGameServer_Init(bind_ip, GAME_PORT, STEAMGAMESERVER_QUERY_PORT_SHARED, eServerModeAuthentication, "1.0.0")) {
        logger::error("Failed to initialize Steam!");
        return 0;
    }
    
    logger::info("Steam Game Server initialized successfully");
    SteamGameServer()->LogOnAnonymous();
    
    // Log the public IP that Steam assigned us
    SteamIPAddress_t publicIP = SteamGameServer()->GetPublicIP();
    logger::info("Steam reports public IP: %u.%u.%u.%u", 
        (publicIP.m_unIPv4 >> 24) & 0xFF,
        (publicIP.m_unIPv4 >> 16) & 0xFF,
        (publicIP.m_unIPv4 >> 8) & 0xFF,
        publicIP.m_unIPv4 & 0xFF);

    m_network.Init(BIND_IP, GAME_PORT);
	while (true) {
        m_network.Update();
	}
	return 1;
}