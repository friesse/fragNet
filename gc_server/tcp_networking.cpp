#include "tcp_networking.hpp"
#include "logger.hpp"
#include <cstring>
#include <algorithm>

#ifdef _WIN32
    #pragma comment(lib, "ws2_32.lib")
    
    struct WinsockInitializer {
        WinsockInitializer() {
            WSADATA wsaData;
            WSAStartup(MAKEWORD(2, 2), &wsaData);
        }
        ~WinsockInitializer() {
            WSACleanup();
        }
    } g_winsockInit;
#endif

TCPNetworking::TCPNetworking() 
    : m_listenSocket(INVALID_SOCKET_VALUE)
    , m_port(0)
    , m_running(false) {
}

TCPNetworking::~TCPNetworking() {
    Shutdown();
}

bool TCPNetworking::Init(const char* bindAddress, uint16_t port) {
    m_bindAddress = bindAddress;
    m_port = port;
    
    // Create socket
    m_listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenSocket == INVALID_SOCKET_VALUE) {
        logger::error("Failed to create TCP socket");
        return false;
    }
    
    // Allow socket reuse
    int opt = 1;
    if (setsockopt(m_listenSocket, SOL_SOCKET, SO_REUSEADDR, 
                   reinterpret_cast<const char*>(&opt), sizeof(opt)) < 0) {
        logger::warning("Failed to set SO_REUSEADDR");
    }
    
    // Setup address structure
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    
    // Parse bind address
    if (strcmp(bindAddress, "0.0.0.0") == 0) {
        addr.sin_addr.s_addr = INADDR_ANY;
        logger::info("Binding TCP socket to all interfaces on port %u", port);
    } else {
        if (inet_pton(AF_INET, bindAddress, &addr.sin_addr) != 1) {
            logger::error("Invalid bind address: %s", bindAddress);
            CLOSE_SOCKET(m_listenSocket);
            m_listenSocket = INVALID_SOCKET_VALUE;
            return false;
        }
        logger::info("Binding TCP socket to %s:%u", bindAddress, port);
    }
    
    // Bind socket
    if (bind(m_listenSocket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR_VALUE) {
#ifdef _WIN32
        logger::error("Failed to bind socket: %d", WSAGetLastError());
#else
        logger::error("Failed to bind socket: %s", strerror(errno));
#endif
        CLOSE_SOCKET(m_listenSocket);
        m_listenSocket = INVALID_SOCKET_VALUE;
        return false;
    }
    
    // Start listening
    if (listen(m_listenSocket, SOMAXCONN) == SOCKET_ERROR_VALUE) {
        logger::error("Failed to listen on socket");
        CLOSE_SOCKET(m_listenSocket);
        m_listenSocket = INVALID_SOCKET_VALUE;
        return false;
    }
    
    logger::info("TCP server listening on %s:%u", bindAddress, port);
    
    // Start accept thread
    m_running = true;
    m_acceptThread = std::thread(&TCPNetworking::AcceptClients, this);
    
    return true;
}

void TCPNetworking::Shutdown() {
    if (!m_running) return;
    
    m_running = false;
    
    // Close listen socket to unblock accept()
    if (m_listenSocket != INVALID_SOCKET_VALUE) {
        CLOSE_SOCKET(m_listenSocket);
        m_listenSocket = INVALID_SOCKET_VALUE;
    }
    
    // Wait for accept thread
    if (m_acceptThread.joinable()) {
        m_acceptThread.join();
    }
    
    // Disconnect all clients
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    for (auto& pair : m_clients) {
        CLOSE_SOCKET(pair.second.socket);
    }
    m_clients.clear();
}

void TCPNetworking::AcceptClients() {
    while (m_running) {
        sockaddr_in clientAddr;
        socklen_t addrLen = sizeof(clientAddr);
        
        socket_t clientSocket = accept(m_listenSocket, 
                                      reinterpret_cast<sockaddr*>(&clientAddr), 
                                      &addrLen);
        
        if (clientSocket == INVALID_SOCKET_VALUE) {
            if (!m_running) break;
            logger::error("Failed to accept client connection");
            continue;
        }
        
        // Get client address
        char addrStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, addrStr, sizeof(addrStr));
        uint16_t clientPort = ntohs(clientAddr.sin_port);
        
        logger::info("Accepted connection from %s:%u (socket: %d)", addrStr, clientPort, clientSocket);
        
        // Set non-blocking mode
        if (!SetSocketNonBlocking(clientSocket)) {
            logger::warning("Failed to set client socket to non-blocking mode");
        }
        
        // Add to client list
        {
            std::lock_guard<std::mutex> lock(m_clientsMutex);
            ClientConnection client;
            client.socket = clientSocket;
            client.address = addrStr;
            client.port = clientPort;
            client.lastActivity = time(nullptr);
            m_clients[clientSocket] = client;
        }
        
        // Start receiving thread for this client
        std::thread receiveThread(&TCPNetworking::ReceiveFromClient, this, clientSocket);
        receiveThread.detach();
    }
}

void TCPNetworking::ReceiveFromClient(socket_t clientSocket) {
    std::vector<uint8_t> buffer(65536);
    
    while (m_running) {
        int received = recv(clientSocket, reinterpret_cast<char*>(buffer.data()), buffer.size(), 0);
        
        if (received > 0) {
            // Update last activity
            {
                std::lock_guard<std::mutex> lock(m_clientsMutex);
                auto it = m_clients.find(clientSocket);
                if (it != m_clients.end()) {
                    it->second.lastActivity = time(nullptr);
                    
                    // Append to client's receive buffer
                    it->second.receiveBuffer.insert(
                        it->second.receiveBuffer.end(),
                        buffer.begin(),
                        buffer.begin() + received
                    );
                    
                    // Check if we have a complete message
                    // Messages start with a 4-byte size header
                    while (it->second.receiveBuffer.size() >= sizeof(uint32_t)) {
                        uint32_t messageSize;
                        memcpy(&messageSize, it->second.receiveBuffer.data(), sizeof(uint32_t));
                        
                        // Check if we have the complete message
                        if (it->second.receiveBuffer.size() >= sizeof(uint32_t) + messageSize) {
                            // Extract the message (skip the size header)
                            std::vector<uint8_t> message(
                                it->second.receiveBuffer.begin() + sizeof(uint32_t),
                                it->second.receiveBuffer.begin() + sizeof(uint32_t) + messageSize
                            );
                            
                            // Queue the message
                            {
                                std::lock_guard<std::mutex> msgLock(m_messageMutex);
                                m_messageQueue.push_back({clientSocket, std::move(message)});
                            }
                            
                            // Remove processed data from buffer
                            it->second.receiveBuffer.erase(
                                it->second.receiveBuffer.begin(),
                                it->second.receiveBuffer.begin() + sizeof(uint32_t) + messageSize
                            );
                        } else {
                            // Not enough data yet
                            break;
                        }
                    }
                }
            }
        } else if (received == 0) {
            // Connection closed
            logger::info("Client disconnected (socket: %d)", clientSocket);
            DisconnectClient(clientSocket);
            break;
        } else {
            // Error
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
#else
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
#endif
                logger::error("Receive error on socket %d", clientSocket);
                DisconnectClient(clientSocket);
                break;
            }
            
            // Would block, sleep a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

bool TCPNetworking::SetSocketNonBlocking(socket_t socket) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(socket, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(socket, F_GETFL, 0);
    if (flags == -1) return false;
    return fcntl(socket, F_SETFL, flags | O_NONBLOCK) != -1;
#endif
}

bool TCPNetworking::SendToClient(socket_t clientSocket, const void* data, size_t size) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    auto it = m_clients.find(clientSocket);
    if (it == m_clients.end()) {
        logger::error("Attempted to send to unknown client socket: %d", clientSocket);
        return false;
    }
    
    // Prepare message with size header
    std::vector<uint8_t> packet;
    packet.resize(sizeof(uint32_t) + size);
    
    uint32_t msgSize = static_cast<uint32_t>(size);
    memcpy(packet.data(), &msgSize, sizeof(uint32_t));
    memcpy(packet.data() + sizeof(uint32_t), data, size);
    
    // Send all data
    size_t totalSent = 0;
    while (totalSent < packet.size()) {
        int sent = send(clientSocket, 
                       reinterpret_cast<const char*>(packet.data() + totalSent),
                       packet.size() - totalSent, 0);
        
        if (sent > 0) {
            totalSent += sent;
        } else {
#ifdef _WIN32
            int error = WSAGetLastError();
            if (error != WSAEWOULDBLOCK) {
#else
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
#endif
                logger::error("Send error on socket %d", clientSocket);
                return false;
            }
            
            // Would block, sleep a bit
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    return true;
}

bool TCPNetworking::GetNextMessage(socket_t& clientSocket, std::vector<uint8_t>& data) {
    std::lock_guard<std::mutex> lock(m_messageMutex);
    
    if (m_messageQueue.empty()) {
        return false;
    }
    
    QueuedMessage msg = std::move(m_messageQueue.front());
    m_messageQueue.erase(m_messageQueue.begin());
    
    clientSocket = msg.clientSocket;
    data = std::move(msg.data);
    
    return true;
}

void TCPNetworking::DisconnectClient(socket_t clientSocket) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    auto it = m_clients.find(clientSocket);
    if (it != m_clients.end()) {
        CLOSE_SOCKET(it->second.socket);
        logger::info("Disconnected client %s:%u (socket: %d)", 
                    it->second.address.c_str(), it->second.port, clientSocket);
        m_clients.erase(it);
    }
}

TCPNetworking::ClientConnection* TCPNetworking::GetClient(socket_t clientSocket) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    auto it = m_clients.find(clientSocket);
    if (it != m_clients.end()) {
        return &it->second;
    }
    return nullptr;
}

socket_t TCPNetworking::GetClientBysteamId(uint64_t steamId) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    for (auto& pair : m_clients) {
        if (pair.second.steamId == steamId) {
            return pair.first;
        }
    }
    return INVALID_SOCKET_VALUE;
}

void TCPNetworking::SetClientSteamId(socket_t clientSocket, uint64_t steamId) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    auto it = m_clients.find(clientSocket);
    if (it != m_clients.end()) {
        it->second.steamId = steamId;
    }
}

void TCPNetworking::SetClientAuthenticated(socket_t clientSocket, bool authenticated) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    auto it = m_clients.find(clientSocket);
    if (it != m_clients.end()) {
        it->second.authenticated = authenticated;
    }
}

void TCPNetworking::CleanupInactiveClients(int timeoutSeconds) {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    time_t now = time(nullptr);
    std::vector<socket_t> toRemove;
    
    for (auto& pair : m_clients) {
        if (now - pair.second.lastActivity > timeoutSeconds) {
            toRemove.push_back(pair.first);
        }
    }
    
    for (socket_t socket : toRemove) {
        auto it = m_clients.find(socket);
        if (it != m_clients.end()) {
            CLOSE_SOCKET(it->second.socket);
            logger::info("Removed inactive client %s:%u (socket: %d)", 
                        it->second.address.c_str(), it->second.port, socket);
            m_clients.erase(it);
        }
    }
}

std::vector<socket_t> TCPNetworking::GetConnectedClients() {
    std::lock_guard<std::mutex> lock(m_clientsMutex);
    
    std::vector<socket_t> clients;
    for (const auto& pair : m_clients) {
        clients.push_back(pair.first);
    }
    return clients;
}
