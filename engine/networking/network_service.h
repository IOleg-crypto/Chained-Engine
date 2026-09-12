#ifndef CH_NETWORK_SERVICE_H
#define CH_NETWORK_SERVICE_H

#include "net_packet.h"
#include "network_types.h"
#include "network_session.h"
#include "network_transport.h"
#include "network_player_manager.h"
#include "upnp_port_mapper.h"
#include "stun_client.h"
#include "engine/core/service.h"

#include <functional>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <unordered_map>

namespace Chained
{
	class Network : public Service
	{
	public:
		using PacketCallback = std::function<void(int, MessageType, const uint8_t*, size_t)>;

		Network() = default;
		~Network() override;

		void Initialize() override;
		void Shutdown() override;

		void HostGame(uint16_t port = kDefaultPort, int maxClients = 4);
		void ConnectTo(const std::string& ip, uint16_t port = kDefaultPort);
		void Disconnect();

		void Update(float dt);

		void SendPacket(int clientIndex, MessageType type, const void* data, size_t len, bool reliable = true);
		void SendPacket(int clientIndex, ePacketChannel channel, MessageType type, const void* data, size_t len,
						bool reliable);
		void BroadcastPacket(MessageType type, bool reliable, const std::function<void(ByteWriter&)>& populate);
		void BroadcastPacket(ePacketChannel channel, MessageType type, bool reliable,
							 const std::function<void(ByteWriter&)>& populate);
		void SendToServer(MessageType type, const void* data, size_t len, bool reliable = true);
		void SendToServer(ePacketChannel channel, MessageType type, const void* data, size_t len, bool reliable);

		void BroadcastSceneChange(const char* scenePath);

		uint32_t GetPing() const
		{
			return m_Session.GetPeerRtt(0);
		}

		void SetPacketCallback(PacketCallback callback);
		void ClearPacketCallback();

		std::string GetListenAddress();
		std::string GetPublicAddress();

		// STUN / NAT traversal
		void QueryStunPublicEndpoint(uint16_t localPort);
		void QueryStunPublicEndpointSync(uint16_t localPort);
		void StartHolePunch(const std::string& remoteIP, uint16_t remotePort);
		void PerformHolePunch(uint16_t localPort, const std::string& remoteIP, uint16_t remotePort);
		const std::string& GetStunPublicIP() const
		{
			return m_StunClient.GetLastPublicIP();
		}
		uint16_t GetStunPublicPort() const
		{
			return m_StunClient.GetLastPublicPort();
		}
		bool HasStunResult() const
		{
			return m_StunClient.HasResult();
		}
		void UpdateHolePunch(float dt);
		void SetOnHolePunchComplete(std::function<void(bool)> cb)
		{
			m_HolePunchCallback = std::move(cb);
		}

		bool HasPendingSceneChange() const
		{
			return !m_PendingSceneChange.empty();
		}
		const std::string& GetPendingSceneChange() const
		{
			return m_PendingSceneChange;
		}
		void ClearPendingSceneChange()
		{
			m_PendingSceneChange.clear();
		}
		void SetPendingSceneChange(const std::string& path)
		{
			m_PendingSceneChange = path;
		}

		void SendPlayerInfoToHost(const char* name, uint8_t skinIndex);
		void BroadcastPlayerList();
		void SendChatMessage(const char* message);
		void StorePendingChatMessage(const ChatMessagePacket& pkt);
		bool HasPendingChatMessages() const
		{
			return m_Transport.HasPendingChatMessages();
		}
		std::vector<ChatMessagePacket> GetPendingChatMessages() const
		{
			return m_Transport.GetPendingChatMessages();
		}
		void ClearPendingChatMessages()
		{
			m_Transport.ClearPendingChatMessages();
		}

		Role GetRole() const
		{
			return m_Session.GetRole();
		}
		bool IsHost() const
		{
			return m_Session.IsHost();
		}
		bool IsClient() const
		{
			return m_Session.IsClient();
		}
		bool IsConnected() const
		{
			return m_Session.IsConnected();
		}
		bool IsFullyConnected() const
		{
			return m_Session.IsFullyConnected();
		}

		size_t GetClientCount() const;
		std::vector<int> GetClients() const;
		int GetServerConnection() const
		{
			return m_Session.GetServerConnection();
		}
		uint16_t GetPort() const
		{
			return m_Session.GetPort();
		}

		uint64_t GetLocalNetworkID() const
		{
			return m_PlayerManager.GetLocalNetworkID();
		}
		void SetLocalNetworkID(uint64_t id)
		{
			m_PlayerManager.SetLocalNetworkID(id);
		}

		void SetLocalPlayerInfo(const char* name, uint8_t skinIndex);

		uint64_t GetNetworkIDForConnection(int clientIndex) const
		{
			return m_PlayerManager.GetNetworkIDForConnection(clientIndex);
		}

		std::vector<PlayerNetInfo> GetPlayerList() const
		{
			return m_PlayerManager.GetPlayerList();
		}
		void UpdatePlayerInfo(uint64_t networkID, const char* name, uint8_t skinIndex)
		{
			m_PlayerManager.UpdatePlayerInfo(networkID, name, skinIndex);
		}
		void SetPlayerListFromMessage(const std::vector<PlayerNetInfo>& list)
		{
			m_PlayerManager.SetPlayerListFromMessage(list);
		}

		int GetMaxClients() const
		{
			return m_Session.GetMaxClients();
		}
		bool IsClientConnected(int clientIndex) const;
		bool IsUpnpAvailable() const
		{
			return m_UpnpMapper.IsPortMapped();
		}

		void SetTestMode(bool enabled)
		{
			m_TestMode = enabled;
		}
		bool IsTestMode() const
		{
			return m_TestMode;
		}

		NetworkSession& GetSession()
		{
			return m_Session;
		}
		NetworkTransport& GetTransport()
		{
			return m_Transport;
		}
		NetworkPlayerManager& GetPlayerManager()
		{
			return m_PlayerManager;
		}

	private:
		void OnClientConnectedInternal(int clientIndex, uint64_t networkID);
		void OnClientDisconnectedInternal(int clientIndex, uint64_t networkID);

		NetworkSession m_Session;
		NetworkTransport m_Transport;
		NetworkPlayerManager m_PlayerManager;
		StunClient m_StunClient;

		std::string m_PendingSceneChange;
		UpnpPortMapper m_UpnpMapper;

		std::string m_ResolvedPublicIP;
		std::string m_CachedPublicIP;
		std::mutex m_PublicIPMutex;
		std::thread m_IPFetchThread;
		bool m_TestMode = false;

		// Hole punch state
		bool m_HolePunchActive = false;
		float m_HolePunchTimer = 0.0f;
		float m_HolePunchInterval = 0.5f;
		int m_HolePunchCount = 0;
		int m_HolePunchMaxAttempts = 8;
		std::string m_HolePunchRemoteIP;
		uint16_t m_HolePunchRemotePort = 0;
		std::function<void(bool)> m_HolePunchCallback;
		int m_HolePunchSocket = -1;

		// Reconnect state (client only)
		bool m_ReconnectPending = false;
		int m_ReconnectAttempts = 0;
		static constexpr int kMaxReconnectAttempts = 3;
		float m_ReconnectTimer = 0.0f;
		std::string m_ReconnectIP;
		uint16_t m_ReconnectPort = 0;
		bool m_WasClientConnected = false;

		// Heartbeat / timeout (host side)
		float m_HeartbeatTimer = 0.0f;
		static constexpr float kHeartbeatInterval = 2.0f;	  // Send heartbeat every 2s
		static constexpr float kClientTimeoutSeconds = 30.0f; // Disconnect after 30s silence
	};

} // namespace Chained

#endif /* CH_NETWORK_SERVICE_H */
