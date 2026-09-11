#include "network_transport.h"
#include "network_session.h"
#include "net_packet.h"
#include <cstring>

namespace Chained
{
	void NetworkTransport::SetPacketCallback(PacketCallback callback)
	{
		m_PacketCallback = std::move(callback);
	}

	void NetworkTransport::ClearPacketCallback()
	{
		m_PacketCallback = nullptr;
	}

	void NetworkTransport::SendRaw(int peerIndex, ePacketChannel channel, MessageType type, const void* payload,
								   size_t payloadLen, bool reliable)
	{
		if (!m_Session || !m_Session->GetDriver())
		{
			return;
		}

		size_t totalLen = sizeof(type) + payloadLen;
		std::vector<uint8_t> packet(totalLen);
		std::memcpy(packet.data(), &type, sizeof(type));
		if (payloadLen > 0 && payload)
		{
			std::memcpy(packet.data() + sizeof(type), payload, payloadLen);
		}

		if (peerIndex == kInvalidPeerHandle)
		{
			m_Session->GetDriver()->BroadcastPacket(channel, packet.data(), totalLen, reliable);
		}
		else
		{
			m_Session->GetDriver()->SendPacket(peerIndex, channel, packet.data(), totalLen, reliable);
		}
	}

	void NetworkTransport::HandleIncomingPacket(int peerIndex, const uint8_t* data, size_t len)
	{
		if (len < sizeof(uint16_t))
		{
			return;
		}

		const uint8_t* payload = data;
		size_t payloadLen = len;

		uint16_t type = 0;
		std::memcpy(&type, payload, sizeof(type));
		payload += sizeof(type);
		payloadLen -= sizeof(type);

		if (type >= MessageType_Count)
		{
			return;
		}
		if (m_PacketCallback)
		{
			m_PacketCallback(peerIndex, static_cast<MessageType>(type), payload, payloadLen);
		}
	}

	void NetworkTransport::SendPacket(int peerIndex, MessageType type, const void* data, size_t len, bool reliable)
	{
		ePacketChannel channel = GetChannelForMessageType(type);
		SendRaw(peerIndex, channel, type, data, len, reliable);
	}

	void NetworkTransport::SendPacket(int peerIndex, ePacketChannel channel, MessageType type, const void* data,
									  size_t len, bool reliable)
	{
		SendRaw(peerIndex, channel, type, data, len, reliable);
	}

	void NetworkTransport::BroadcastPacket(MessageType type, bool reliable,
										   const std::function<void(ByteWriter&)>& populate)
	{
		ePacketChannel channel = GetChannelForMessageType(type);
		BroadcastPacket(channel, type, reliable, populate);
	}

	void NetworkTransport::BroadcastPacket(ePacketChannel channel, MessageType type, bool reliable,
										   const std::function<void(ByteWriter&)>& populate)
	{
		if (!m_Session || !m_Session->IsHost() || !m_Session->GetDriver())
		{
			return;
		}
		ByteWriter w;
		if (populate)
		{
			populate(w);
		}

		size_t totalLen = sizeof(type) + w.Data().size();
		std::vector<uint8_t> packet(totalLen);
		std::memcpy(packet.data(), &type, sizeof(type));
		if (!w.Data().empty())
		{
			std::memcpy(packet.data() + sizeof(type), w.Data().data(), w.Data().size());
		}

		m_Session->GetDriver()->BroadcastPacket(channel, packet.data(), totalLen, reliable);
	}

	void NetworkTransport::SendToServer(MessageType type, const void* data, size_t len, bool reliable)
	{
		ePacketChannel channel = GetChannelForMessageType(type);
		SendToServer(channel, type, data, len, reliable);
	}

	void NetworkTransport::SendToServer(ePacketChannel channel, MessageType type, const void* data, size_t len,
										bool reliable)
	{
		if (!m_Session || !m_Session->IsClient())
		{
			return;
		}
		SendRaw(kInvalidPeerHandle, channel, type, data, len, reliable);
	}

	void NetworkTransport::BroadcastSceneChange(const char* scenePath)
	{
		if (!m_Session || !m_Session->IsHost())
		{
			return;
		}
		BroadcastPacket(ePacketChannel::SYSTEM, MessageType_SceneChange, true, [scenePath](ByteWriter& bw) {
			SceneChangeMessage msg;
			std::strncpy(msg.ScenePath, scenePath, sizeof(msg.ScenePath) - 1);
			msg.ScenePath[sizeof(msg.ScenePath) - 1] = '\0';
			msg.Encode(bw);
		});
		CH_CORE_INFO("NetworkTransport: Broadcast scene change -> {}", scenePath);
	}

	void NetworkTransport::SendPlayerInfoToHost(const char* name, uint8_t skinIndex, uint64_t /*localNetworkID*/)
	{
		if (!m_Session || m_Session->GetRole() != Role::Client)
		{
			return;
		}
		PlayerInfoMessage msg;
		std::strncpy(msg.Name, name ? name : "Player", sizeof(msg.Name) - 1);
		msg.Name[sizeof(msg.Name) - 1] = '\0';
		msg.SkinIndex = skinIndex;
		ByteWriter w;
		msg.Encode(w);
		SendToServer(ePacketChannel::SYSTEM, MessageType_PlayerInfo, w.Data().data(), w.Data().size(), true);
		CH_CORE_INFO("NetworkTransport: Sent player info (name='{}', skin={}).", msg.Name, (int)skinIndex);
	}

	void NetworkTransport::BroadcastPlayerList(const std::vector<PlayerNetInfo>& playerList)
	{
		if (!m_Session || !m_Session->IsHost())
		{
			return;
		}
		uint8_t count = static_cast<uint8_t>(std::min(playerList.size(), size_t(64)));
		BroadcastPacket(ePacketChannel::SYSTEM, MessageType_PlayerList, true, [&playerList, count](ByteWriter& bw) {
			PlayerListMessage msg;
			msg.Count = count;
			for (int i = 0; i < count && i < 64; ++i)
			{
				msg.Entries[i].NetworkID = playerList[i].NetworkID;
				std::strncpy(msg.Entries[i].Name, playerList[i].Name.c_str(), sizeof(msg.Entries[i].Name) - 1);
				msg.Entries[i].Name[sizeof(msg.Entries[i].Name) - 1] = '\0';
				msg.Entries[i].SkinIndex = playerList[i].SkinIndex;
				msg.Entries[i].IsHost = playerList[i].IsHost;
			}
			msg.Encode(bw);
		});
		CH_CORE_INFO("NetworkTransport: Broadcast player list ({} players).", count);
	}

	void NetworkTransport::SendChatMessage(const char* message, uint64_t localNetworkID,
										   const std::string& localPlayerName)
	{
		if (!message || !message[0] || !m_Session)
		{
			return;
		}

		if (m_Session->IsHost())
		{
			BroadcastPacket(ePacketChannel::EVENT, MessageType_ChatMessage, true, [&](ByteWriter& bw) {
				ChatMessageMessage msg;
				msg.SenderNetworkID = localNetworkID;
				std::strncpy(msg.SenderName, localPlayerName.c_str(), sizeof(msg.SenderName) - 1);
				msg.SenderName[sizeof(msg.SenderName) - 1] = '\0';
				std::strncpy(msg.Message, message, sizeof(msg.Message) - 1);
				msg.Message[sizeof(msg.Message) - 1] = '\0';
				msg.Encode(bw);
			});
			ChatMessagePacket pkt;
			pkt.SenderNetworkID = localNetworkID;
			pkt.SenderName = localPlayerName;
			pkt.Message = message;
			StorePendingChatMessage(pkt);
		}
		else if (m_Session->IsClient())
		{
			ChatMessageMessage msg;
			msg.SenderNetworkID = localNetworkID;
			std::strncpy(msg.SenderName, localPlayerName.c_str(), sizeof(msg.SenderName) - 1);
			msg.SenderName[sizeof(msg.SenderName) - 1] = '\0';
			std::strncpy(msg.Message, message, sizeof(msg.Message) - 1);
			msg.Message[sizeof(msg.Message) - 1] = '\0';
			ByteWriter w;
			msg.Encode(w);
			SendToServer(ePacketChannel::EVENT, MessageType_ChatMessage, w.Data().data(), w.Data().size(), true);
		}
	}
} // namespace Chained
