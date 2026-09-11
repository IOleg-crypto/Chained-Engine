#ifndef CH_NETWORK_SESSION_H
#define CH_NETWORK_SESSION_H

#include "network_types.h"
#include "net_packet.h"
#include "network_driver.h"

#include <cstdint>
#include <functional>
#include <string>
#include <memory>
#include <unordered_map>

namespace Chained
{

	class NetworkSession
	{
	public:
		using EventCallback = std::function<void(int peerIndex, MessageType type, const uint8_t* data, size_t len)>;
		using ConnectionCallback = std::function<void(int peerIndex, uint64_t networkID)>;
		using DisconnectionCallback = std::function<void(int peerIndex, uint64_t networkID)>;

		NetworkSession();
		~NetworkSession();

		NetworkSession(const NetworkSession&) = delete;
		NetworkSession& operator=(const NetworkSession&) = delete;

		NetworkError Initialize();
		void Shutdown();

		NetworkError HostGame(uint16_t port, int maxClients);
		NetworkError ConnectTo(const std::string& ip, uint16_t port);
		void Disconnect();

		void Update(float dt);

		Role GetRole() const;
		bool IsHost() const
		{
			return GetRole() == Role::Host;
		}
		bool IsClient() const
		{
			return GetRole() == Role::Client;
		}
		bool IsConnected() const;
		bool IsFullyConnected() const;

		int GetMaxClients() const;
		uint16_t GetPort() const;
		std::string GetListenAddress() const;
		void PunchHole(const std::string& targetIP, uint16_t targetPort, int count = 10);

		void SetEventCallback(EventCallback cb)
		{
			m_EventCallback = std::move(cb);
		}
		void SetConnectionCallback(ConnectionCallback cb)
		{
			m_ConnectionCallback = std::move(cb);
		}
		void SetDisconnectionCallback(DisconnectionCallback cb)
		{
			m_DisconnectionCallback = std::move(cb);
		}

		INetworkDriver* GetDriver() const
		{
			return m_Driver.get();
		}
		void SetDriver(std::unique_ptr<INetworkDriver> driver)
		{
			m_Driver = std::move(driver);
		}

		void SetDriverType(DriverType type);
		DriverType GetDriverType() const
		{
			return m_DriverType;
		}

		bool IsClientConnected(int clientIndex) const;
		uint32_t GetPeerRtt(int peerIndex) const;

		void SetServerConnection(int conn)
		{
			m_ServerConnection = conn;
		}
		int GetServerConnection() const
		{
			return m_ServerConnection;
		}

		// Heartbeat / timeout tracking
		void RecordPeerActivity(int peerIndex);
		float GetTimeSinceLastActivity(int peerIndex) const;
		void CheckPeerTimeouts(float dt, float timeoutSeconds);

	private:
		DriverType m_DriverType = DriverType::ENet;
		std::unique_ptr<INetworkDriver> m_Driver;
		int m_ServerConnection = kInvalidPeerHandle;

		EventCallback m_EventCallback;
		ConnectionCallback m_ConnectionCallback;
		DisconnectionCallback m_DisconnectionCallback;

		std::unordered_map<int, float> m_PeerLastActivityTime;
		bool m_IsUpdating = false;
	};

} // namespace Chained

#endif // CH_NETWORK_SESSION_H
