// steam_network_message.hpp
#pragma once
#include "gc_const.hpp"
#include <steam/steam_api.h>
#include <memory>
#include <string>
#include "cc_gcmessages.pb.h"

class NetworkMessage {
	public:
		static constexpr size_t MAX_CHUNK_SIZE = 1024;

		explicit NetworkMessage(const void* data, uint32_t size);
	
		// create proto msgs
		template<typename T>
		static NetworkMessage FromProto(const T& msg, uint32_t msgType) {
			NetworkMessage message;
			message.m_type = msgType;
			size_t size = msg.ByteSizeLong();
			message.m_data.resize(size);
			msg.SerializeToArray(message.m_data.data(), size);
			return message;
		}

		bool WriteToSocket(SNetSocket_t socket, bool reliable, uint32_t chunks = 0) const;

		// parse msg types
		template<typename T>
		bool ParseTo(T* msg) const {
			return msg->ParseFromArray(m_data.data(), m_data.size());
		}

		// get msg type
		uint32_t GetType() const { return m_type & ~CCProtoMask; }
		
		// get raw data
		const std::vector<uint8_t>& GetData() const { return m_data; }

		// get total size
		uint32_t GetTotalSize() const {
			return sizeof(uint32_t) +  // type
				   sizeof(uint32_t) +  // header size
				   m_data.size();      // payload
		}

		static uint16_t GetTypeFromData(const void* data, uint32_t size);

	private:
		NetworkMessage() = default;
		uint32_t m_type = 0;
		std::vector<uint8_t> m_data;

		// helpers for WriteToSocket
		bool WriteSingleMsg(SNetSocket_t socket, bool reliable) const;
		bool WriteChunkMsg(SNetSocket_t socket, bool reliable, uint32_t chunks) const;
	};

	// helper msgs
	namespace Messages {
		inline NetworkMessage CreateWelcome(uint64_t steamId, const char* authTicket, uint32_t ticketSize) {
			CMsgGC_CC_GCWelcome msg;
			msg.set_steam_id(steamId);
			msg.set_auth_ticket(authTicket, ticketSize);
			msg.set_auth_ticket_size(ticketSize);
			return NetworkMessage::FromProto(msg, k_EMsgGC_CC_GCWelcome);
		}

		inline NetworkMessage CreateAuthConfirm(uint32_t authResult) {
			CMsgGC_CC_GCConfirmAuth msg;
			msg.set_auth_result(authResult);
			return NetworkMessage::FromProto(msg, k_EMsgGC_CC_GCConfirmAuth);
		}

		inline NetworkMessage CreateHeartbeat() {
			CMsgGC_CC_GCHeartbeat msg;
			return NetworkMessage::FromProto(msg, k_EMsgGC_CC_GCHeartbeat);
		}
	}