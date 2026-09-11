#include "network_session.h"
#include "enet_threaded_driver.h"

namespace Chained
{

	NetworkSession::NetworkSession()
		: m_Driver(std::make_unique<ENetThreadedDriver>())
	{
	}

	NetworkSession::~NetworkSession()
	{
		Shutdown();
	}

	void NetworkSession::SetDriverType(DriverType type)
	{
		if (m_DriverType == type && m_Driver)
		{
			return;
		}

		Shutdown();
		m_DriverType = type;
		m_Driver = std::make_unique<ENetThreadedDriver>();
		m_Driver->Initialize();
	}

	NetworkError NetworkSession::Initialize()
	{
		if (!m_Driver)
		{
			m_Driver = std::make_unique<ENetThreadedDriver>();
		}
		return m_Driver->Initialize();
	}

	void NetworkSession::Shutdown()
	{
		if (m_Driver)
		{
			m_Driver->Shutdown();
		}
		m_ServerConnection = kInvalidPeerHandle;
		m_PeerLastActivityTime.clear();
	}

	NetworkError NetworkSession::HostGame(uint16_t port, int maxClients)
	{
		if (!m_Driver)
		{
			m_Driver = std::make_unique<ENetThreadedDriver>();
		}
		m_PeerLastActivityTime.clear();
		return m_Driver->Host(port, maxClients);
	}

	NetworkError NetworkSession::ConnectTo(const std::string& ip, uint16_t port)
	{
		if (!m_Driver)
		{
			m_Driver = std::make_unique<ENetThreadedDriver>();
		}
		m_ServerConnection = 0;
		m_PeerLastActivityTime.clear();
		return m_Driver->Connect(ip, port);
	}

	void NetworkSession::Disconnect()
	{
		if (m_Driver)
		{
			m_Driver->Disconnect();
		}
		m_ServerConnection = kInvalidPeerHandle;
		m_PeerLastActivityTime.clear();
	}

	void NetworkSession::Update(float /*dt*/)
	{
		if (!m_Driver || !m_Driver->IsConnected() || m_IsUpdating)
		{
			return;
		}

		m_IsUpdating = true;

		std::vector<NetworkDriverEvent> polledEvents;
		m_Driver->PollEvents(polledEvents);

		for (const auto& ev : polledEvents)
		{
			switch (ev.Type)
			{
			case NetworkDriverEventType::Connected: {
				if (m_Driver->GetRole() == Role::Host)
				{
					RecordPeerActivity(ev.PeerIndex);
					if (m_ConnectionCallback)
					{
						m_ConnectionCallback(ev.PeerIndex, 0);
					}
				}
				else if (m_Driver->GetRole() == Role::Client)
				{
					CH_CORE_INFO("NetworkSession: Connected to server.");
				}
				break;
			}

			case NetworkDriverEventType::Disconnected: {
				if (m_Driver->GetRole() == Role::Host)
				{
					m_PeerLastActivityTime.erase(ev.PeerIndex);
					if (m_DisconnectionCallback)
					{
						m_DisconnectionCallback(ev.PeerIndex, 0);
					}
				}
				else
				{
					CH_CORE_INFO("NetworkSession: Disconnected from server.");
					m_ServerConnection = kInvalidPeerHandle;
					if (m_DisconnectionCallback)
					{
						m_DisconnectionCallback(0, 0);
					}
				}
				break;
			}

			case NetworkDriverEventType::PacketReceived: {
				if (m_Driver->GetRole() == Role::Host)
				{
					RecordPeerActivity(ev.PeerIndex);
				}
				if (m_EventCallback)
				{
					m_EventCallback(ev.PeerIndex, MessageType_Count, ev.Data.data(), ev.Data.size());
				}
				break;
			}

			default:
				break;
			}
		}

		m_IsUpdating = false;
	}

	Role NetworkSession::GetRole() const
	{
		return m_Driver ? m_Driver->GetRole() : Role::Offline;
	}

	bool NetworkSession::IsConnected() const
	{
		return m_Driver ? m_Driver->IsConnected() : false;
	}

	bool NetworkSession::IsFullyConnected() const
	{
		return m_Driver ? m_Driver->IsFullyConnected() : false;
	}

	int NetworkSession::GetMaxClients() const
	{
		return m_Driver ? m_Driver->GetMaxClients() : 0;
	}

	uint16_t NetworkSession::GetPort() const
	{
		return m_Driver ? m_Driver->GetPort() : 0;
	}

	std::string NetworkSession::GetListenAddress() const
	{
		return m_Driver ? m_Driver->GetListenAddress() : std::string{};
	}

	bool NetworkSession::IsClientConnected(int clientIndex) const
	{
		return m_Driver ? m_Driver->IsPeerConnected(clientIndex) : false;
	}

	uint32_t NetworkSession::GetPeerRtt(int peerIndex) const
	{
		return m_Driver ? m_Driver->GetPeerRtt(peerIndex) : 0;
	}

	void NetworkSession::RecordPeerActivity(int peerIndex)
	{
		m_PeerLastActivityTime[peerIndex] = 0.0f;
	}

	float NetworkSession::GetTimeSinceLastActivity(int peerIndex) const
	{
		auto it = m_PeerLastActivityTime.find(peerIndex);
		return it != m_PeerLastActivityTime.end() ? it->second : 0.0f;
	}

	void NetworkSession::CheckPeerTimeouts(float dt, float timeoutSeconds)
	{
		if (!m_Driver || m_Driver->GetRole() != Role::Host)
		{
			return;
		}

		// Update all peer timers
		for (auto& [peerIndex, timer] : m_PeerLastActivityTime)
		{
			timer += dt;
		}

		// Find and disconnect timed-out peers
		std::vector<int> timedOut;
		for (auto& [peerIndex, timer] : m_PeerLastActivityTime)
		{
			if (timer >= timeoutSeconds)
			{
				timedOut.push_back(peerIndex);
			}
		}

		for (int peerIndex : timedOut)
		{
			CH_CORE_WARN("NetworkSession: Peer {} timed out (no data for {:.1f}s). Disconnecting.", peerIndex,
						 timeoutSeconds);
			m_PeerLastActivityTime.erase(peerIndex);
			if (m_Driver)
			{
				m_Driver->DisconnectPeer(peerIndex);
			}
			if (m_DisconnectionCallback)
			{
				m_DisconnectionCallback(peerIndex, 0);
			}
		}
	}

	void NetworkSession::PunchHole(const std::string& targetIP, uint16_t targetPort, int count)
	{
		if (m_Driver)
		{
			m_Driver->PunchHole(targetIP, targetPort, count);
		}
	}

} // namespace Chained
