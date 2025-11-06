#pragma once
#include <thread>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <string>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    typedef SOCKET socket_t;
    #define INVALID_SOCKET_VALUE INVALID_SOCKET
    #define SOCKET_ERROR_VALUE SOCKET_ERROR
    #define CLOSE_SOCKET closesocket
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
    typedef int socket_t;
    #define INVALID_SOCKET_VALUE -1
    #define SOCKET_ERROR_VALUE -1
    #define CLOSE_SOCKET close
#endif

class TCPNetworking {
public:
    struct ClientConnection {
        socket_t socket;
        std::string address;
        uint16_t port;
        uint64_t steamId;
        bool authenticated;
        std::vector<uint8_t> receiveBuffer;
        time_t lastActivity;
        
        ClientConnection() : socket(INVALID_SOCKET_VALUE), port(0), steamId(0), 
                            authenticated(false), lastActivity(0) {}
    };

private:
    socket_t m_listenSocket;
    std::string m_bindAddress;
    uint16_t m_port;
    std::atomic<bool> m_running;
    std::thread m_acceptThread;
    
    std::map<socket_t, ClientConnection> m_clients;
    std::mutex m_clientsMutex;
    
    // Message queue for thread-safe processing
    struct QueuedMessage {
        socket_t clientSocket;
        std::vector<uint8_t> data;
    };
    std::vector<QueuedMessage> m_messageQueue;
    std::mutex m_messageMutex;
    
    void AcceptClients();
    void ReceiveFromClient(socket_t clientSocket);
    bool SetSocketNonBlocking(socket_t socket);
    
public:
    TCPNetworking();
    ~TCPNetworking();
    
    bool Init(const char* bindAddress, uint16_t port);
    void Shutdown();
    
    // Send data to a specific client
    bool SendToClient(socket_t clientSocket, const void* data, size_t size);
    
    // Get pending messages (called from main thread)
    bool GetNextMessage(socket_t& clientSocket, std::vector<uint8_t>& data);
    
    // Client management
    void DisconnectClient(socket_t clientSocket);
    ClientConnection* GetClient(socket_t clientSocket);
    socket_t GetClientBysteamId(uint64_t steamId);
    void SetClientSteamId(socket_t clientSocket, uint64_t steamId);
    void SetClientAuthenticated(socket_t clientSocket, bool authenticated);
    
    // Cleanup inactive clients
    void CleanupInactiveClients(int timeoutSeconds = 60);
    
    // Get all connected clients
    std::vector<socket_t> GetConnectedClients();
    
    bool IsRunning() const { return m_running; }
};
