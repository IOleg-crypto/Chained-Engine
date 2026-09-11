#ifndef CH_NETWORK_TRANSPORT_H
#define CH_NETWORK_TRANSPORT_H

#include "network_types.h"
#include "net_packet.h"

#include <cstdint>
#include <functional>
#include <vector>
#include <unordered_map>
#include <mutex>

namespace Chained
{

	class NetworkSession;

	class NetworkTransport
	{
	public:
		using PacketCallback = std::function<void(int peerIndex, MessageType type, const uint8_t* data, size_t len)>;

		NetworkTransport() = default;
		~NetworkTransport() = default;

		NetworkTransport(const NetworkTransport&) = delete;
		NetworkTransport& operator=(const NetworkTransport&) = delete;

		void SetSession(NetworkSession* session)
		{
			m_Session = session;
		}

		void SetPacketCallback(PacketCallback callback);
		void ClearPacketCallback();

		void SendPacket(int peerIndex, MessageType type, const void* data, size_t len, bool reliable = true);
		void SendPacket(int peerIndex, ePacketChannel channel, MessageType type, const void* data, size_t len,
						bool reliable);

		void BroadcastPacket(MessageType type, bool reliable, const std::function<void(ByteWriter&)>& populate);
		void BroadcastPacket(ePacketChannel channel, MessageType type, bool reliable,
							 const std::function<void(ByteWriter&)>& populate);

		void SendToServer(MessageType type, const void* data, size_t len, bool reliable = true);
		void SendToServer(ePacketChannel channel, MessageType type, const void* data, size_t len, bool reliable);

		void BroadcastSceneChange(const char* scenePath);

		void SendPlayerInfoToHost(const char* name, uint8_t skinIndex, uint64_t localNetworkID);
		void BroadcastPlayerList(const std::vector<struct PlayerNetInfo>& playerList);
		void SendChatMessage(const char* message, uint64_t localNetworkID, const std::string& localPlayerName);

		void HandleIncomingPacket(int peerIndex, const uint8_t* data, size_t len);

		void ClearPendingChatMessages()
		{
			std::lock_guard<std::mutex> lock(m_ChatMutex);
			m_PendingChatMessages.clear();
		}
		bool HasPendingChatMessages() const
		{
			std::lock_guard<std::mutex> lock(m_ChatMutex);
			return !m_PendingChatMessages.empty();
		}
		std::vector<ChatMessagePacket> GetPendingChatMessages() const
		{
			std::lock_guard<std::mutex> lock(m_ChatMutex);
			return m_PendingChatMessages;
		}
		void StorePendingChatMessage(const ChatMessagePacket& pkt)
		{
			std::lock_guard<std::mutex> lock(m_ChatMutex);
			m_PendingChatMessages.push_back(pkt);
		}

	private:
		void SendRaw(int peerIndex, ePacketChannel channel, MessageType type, const void* payload, size_t payloadLen,
					 bool reliable);

		NetworkSession* m_Session = nullptr;
		PacketCallback m_PacketCallback;
		mutable std::mutex m_ChatMutex;
		std::vector<ChatMessagePacket> m_PendingChatMessages;
	};

} // namespace Chained

#endif // CH_NETWORK_TRANSPORT_H
