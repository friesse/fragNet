// steam_network_message.cpp
#include "steam_network_message.hpp"
#include "logger.hpp"
#include <steam/steam_gameserver.h>
#include <arpa/inet.h>

NetworkMessage::NetworkMessage(const void* data, uint32_t size) 
{
    if (size < sizeof(uint32_t)) {
        logger::error("Message too small for header");
        return;
    }

    memcpy(&m_type, data, sizeof(uint32_t));

    // header = type, header size, chunk count
    const size_t headerSize = sizeof(uint32_t) * 3;
    if (size < headerSize) {
        logger::error("Message too small for full header");
        m_data.clear();
        return;
    }

    uint32_t chunks;
    memcpy(&chunks, static_cast<const uint8_t*>(data) + sizeof(uint32_t) * 2, sizeof(uint32_t));
    
    const size_t dataSize = size - headerSize;
    if (dataSize > 0) {
        m_data.resize(dataSize);
        memcpy(m_data.data(),
            static_cast<const uint8_t*>(data) + headerSize,
            dataSize);
    }
}

bool NetworkMessage::WriteToSocket(SNetSocket_t socket, bool reliable, uint32_t chunks) const {
    // AutoChunkCalcuator™️
    if (chunks == 0) {
        size_t totalSize = GetTotalSize();
        chunks = (totalSize + MAX_CHUNK_SIZE - 1) / MAX_CHUNK_SIZE;
        if (chunks == 0) chunks = 1; // 1 chunk minimum
    }
    
    return (chunks == 1) ? WriteSingleMsg(socket, reliable) 
                         : WriteChunkMsg(socket, reliable, chunks);
}

bool NetworkMessage::WriteSingleMsg(SNetSocket_t socket, bool reliable) const 
{
    std::vector<uint8_t> fullMessage;

    // type w mask
    uint32_t maskedType = m_type | CCProtoMask;
    fullMessage.insert(fullMessage.end(),
        reinterpret_cast<const uint8_t*>(&maskedType),
        reinterpret_cast<const uint8_t*>(&maskedType) + sizeof(maskedType));

    // header size
    uint32_t headerSize = 0;
    fullMessage.insert(fullMessage.end(),
        reinterpret_cast<const uint8_t*>(&headerSize),
        reinterpret_cast<const uint8_t*>(&headerSize) + sizeof(headerSize));

    // chunk count (always 1 in this function)
    uint32_t chunkCount = 1;
    fullMessage.insert(fullMessage.end(),
        reinterpret_cast<const uint8_t*>(&chunkCount),
        reinterpret_cast<const uint8_t*>(&chunkCount) + sizeof(chunkCount));

    // write
    fullMessage.insert(fullMessage.end(), m_data.begin(), m_data.end());

    return SteamGameServerNetworking()->SendDataOnSocket(
        socket,
        fullMessage.data(),
        fullMessage.size(),
        reliable ? k_EP2PSendReliable : k_EP2PSendUnreliable
    );
}

bool NetworkMessage::WriteChunkMsg(SNetSocket_t socket, bool reliable, uint32_t chunks) const 
{
    const size_t chunkSize = (m_data.size() + chunks - 1) / chunks;
    
    logger::info("Splitting message - Total size: %zu, Chunks: %u, Chunk size: %zu",
                 m_data.size(), chunks, chunkSize);

    for (uint32_t i = 0; i < chunks; i++) 
    {
        std::vector<uint8_t> chunkMessage;

        // type w mask
        uint32_t maskedType = m_type | CCProtoMask;
        chunkMessage.insert(chunkMessage.end(),
            reinterpret_cast<const uint8_t*>(&maskedType),
            reinterpret_cast<const uint8_t*>(&maskedType) + sizeof(maskedType));

        // header size
        uint32_t headerSize = 0;
        chunkMessage.insert(chunkMessage.end(),
            reinterpret_cast<const uint8_t*>(&headerSize),
            reinterpret_cast<const uint8_t*>(&headerSize) + sizeof(headerSize));

        // chunk count
        chunkMessage.insert(chunkMessage.end(),
            reinterpret_cast<const uint8_t*>(&chunks),
            reinterpret_cast<const uint8_t*>(&chunks) + sizeof(chunks));

        // Calculate chunk bounds
        size_t startPos = i * chunkSize;
        size_t endPos = std::min(startPos + chunkSize, m_data.size());
            
        // write payload
        chunkMessage.insert(chunkMessage.end(), 
            m_data.begin() + startPos,
            m_data.begin() + endPos);

        logger::info("Sending chunk %u/%u - Size: %zu", i + 1, chunks, chunkMessage.size());

        if (!SteamGameServerNetworking()->SendDataOnSocket(
            socket,
            chunkMessage.data(),
            chunkMessage.size(),
            reliable ? k_EP2PSendReliable : k_EP2PSendUnreliable
        )) {
            logger::error("Failed to send chunk %u/%u", i + 1, chunks);
            return false;
        }
    }

    return true;
}


uint16_t NetworkMessage::GetTypeFromData(const void* data, uint32_t size) 
{
    if (size < sizeof(uint16_t)) {
        return 0;
    }
    uint16_t type;
    memcpy(&type, data, sizeof(uint16_t));
    return type;
}