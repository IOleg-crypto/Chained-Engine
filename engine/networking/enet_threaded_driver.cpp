#include "enet_threaded_driver.h"
#include <chrono>
#include <cstring>

namespace Chained
{
	ENetThreadedDriver::ENetThreadedDriver() = default;

	ENetThreadedDriver::~ENetThreadedDriver()
	{
		Shutdown();
	}

	NetworkError ENetThreadedDriver::Initialize()
	{
		if (m_Initialized.load(std::memory_order_relaxed))
		{
			return NetworkError::None;
		}

		if (enet_initialize() != 0)
		{
			CH_CORE_ERROR("ENetThreadedDriver: Failed to initialize ENet.");
			return NetworkError::ENetInitFailed;
		}

		m_Initialized.store(true, std::memory_order_relaxed);
		m_Role.store(Role::Offline, std::memory_order_relaxed);
		CH_CORE_INFO("ENetThreadedDriver: Initialized ENet successfully.");
		return NetworkError::None;
	}

	void ENetThreadedDriver::Shutdown()
	{
		Disconnect();

		if (m_Initialized.exchange(false, std::memory_order_relaxed))
		{
			enet_deinitialize();
			CH_CORE_INFO("ENetThreadedDriver: Shutdown complete.");
		}
	}

	NetworkError ENetThreadedDriver::Host(uint16_t port, int maxClients)
	{
		if (!m_Initialized.load(std::memory_order_relaxed))
		{
			NetworkError initErr = Initialize();
			if (initErr != NetworkError::None)
			{
				return initErr;
			}
		}

		if (m_Role.load(std::memory_order_relaxed) != Role::Offline)
		{
			CH_CORE_ERROR("ENetThreadedDriver: Cannot host — already active in role {}.",
						  static_cast<int>(m_Role.load()));
			return NetworkError::AlreadyConnected;
		}

		StopWorker();

		ENetAddress address = {};
		address.host = ENET_HOST_ANY;
		address.port = port;

		// 4 channels: SYSTEM(0), SYNC(1), EVENT(2), SCRIPT(3)
		m_Host = enet_host_create(&address, maxClients, ePacketChannel::COUNT, 0, 0);
		if (!m_Host)
		{
			CH_CORE_ERROR("ENetThreadedDriver: Failed to create ENet host on port {}.", port);
			return NetworkError::CreateHostFailed;
		}

		m_MaxClients.store(maxClients, std::memory_order_relaxed);
		m_Port.store(port, std::memory_order_relaxed);
		m_Role.store(Role::Host, std::memory_order_relaxed);
		m_FullyConnected.store(true, std::memory_order_relaxed);

		{
			std::lock_guard<std::mutex> lock(m_StateMutex);
			char ip[64] = {};
			if (enet_address_get_host_ip(&m_Host->address, ip, sizeof(ip)) == 0)
			{
				m_ListenAddress = std::string(ip) + ":" + std::to_string(port);
			}
			else
			{
				m_ListenAddress = "127.0.0.1:" + std::to_string(port);
			}
		}

		StartWorker();
		CH_CORE_INFO("ENetThreadedDriver: Hosting on port {} (max {} clients, {} channels).", port, maxClients,
					 static_cast<int>(ePacketChannel::COUNT));
		return NetworkError::None;
	}

	NetworkError ENetThreadedDriver::Connect(const std::string& ip, uint16_t port)
	{
		if (!m_Initialized.load(std::memory_order_relaxed))
		{
			NetworkError initErr = Initialize();
			if (initErr != NetworkError::None)
			{
				return initErr;
			}
		}

		if (m_Role.load(std::memory_order_relaxed) != Role::Offline)
		{
			CH_CORE_ERROR("ENetThreadedDriver: Cannot connect — already active in role {}.",
						  static_cast<int>(m_Role.load()));
			return NetworkError::AlreadyConnected;
		}

		StopWorker();

		// Client host with 1 outgoing connection and 4 channels
		m_Host = enet_host_create(nullptr, 1, ePacketChannel::COUNT, 0, 0);
		if (!m_Host)
		{
			CH_CORE_ERROR("ENetThreadedDriver: Failed to create ENet client host.");
			return NetworkError::CreateHostFailed;
		}

		ENetAddress address = {};
		if (enet_address_set_host(&address, ip.c_str()) != 0)
		{
			CH_CORE_ERROR("ENetThreadedDriver: Failed to resolve host '{}'.", ip);
			enet_host_destroy(m_Host);
			m_Host = nullptr;
			return NetworkError::ConnectFailed;
		}
		address.port = port;

		char ipBuf[64] = {};
		enet_address_get_host_ip(&address, ipBuf, sizeof(ipBuf));
		CH_CORE_INFO("ENetThreadedDriver: Connecting to {}:{} ({}:{})...", ip, port, ipBuf, address.port);

		m_ServerPeer = enet_host_connect(m_Host, &address, ePacketChannel::COUNT, 0);
		if (!m_ServerPeer)
		{
			CH_CORE_ERROR("ENetThreadedDriver: Failed to connect to {}:{}.", ip, port);
			enet_host_destroy(m_Host);
			m_Host = nullptr;
			return NetworkError::ConnectFailed;
		}

		enet_host_flush(m_Host);

		m_MaxClients.store(1, std::memory_order_relaxed);
		m_Port.store(port, std::memory_order_relaxed);
		m_Role.store(Role::Client, std::memory_order_relaxed);
		m_FullyConnected.store(false, std::memory_order_relaxed);

		StartWorker();
		return NetworkError::None;
	}

	void ENetThreadedDriver::Disconnect()
	{
		StopWorker();

		if (m_Host)
		{
			if (m_Role.load(std::memory_order_relaxed) == Role::Host)
			{
				{
					std::lock_guard<std::mutex> lock(m_PeerMutex);
					for (auto& [idx, peer] : m_PeerMap)
					{
						if (peer)
						{
							enet_peer_disconnect_now(peer, 0);
						}
					}
				}
				enet_host_flush(m_Host);
			}
			else if (m_ServerPeer)
			{
				enet_peer_disconnect_now(m_ServerPeer, 0);
				enet_host_flush(m_Host);
				m_ServerPeer = nullptr;
			}

			enet_host_destroy(m_Host);
			m_Host = nullptr;
		}

		{
			std::lock_guard<std::mutex> lock(m_PeerMutex);
			m_PeerMap.clear();
		}
		m_Role.store(Role::Offline, std::memory_order_relaxed);
		m_FullyConnected.store(false, std::memory_order_relaxed);
		m_Port.store(0, std::memory_order_relaxed);

		{
			std::lock_guard<std::mutex> lock(m_StateMutex);
			m_ListenAddress.clear();
		}
		{
			std::lock_guard<std::mutex> lock(m_RttMutex);
			m_PeerRtt.clear();
			m_ServerRtt = 0;
		}
		{
			std::lock_guard<std::mutex> lock(m_OutboundMutex);
			m_OutboundQueue.clear();
		}
		{
			std::lock_guard<std::mutex> lock(m_InboundMutex);
			m_InboundQueue.clear();
		}

		CH_CORE_INFO("ENetThreadedDriver: Disconnected.");
	}

	void ENetThreadedDriver::SendPacket(int peerIndex, ePacketChannel channel, const void* data, size_t len,
										bool reliable)
	{
		if (m_Role.load(std::memory_order_relaxed) == Role::Offline || !data || len == 0)
		{
			return;
		}

		OutboundPacket pkt;
		pkt.PeerIndex = peerIndex;
		pkt.Channel = channel;
		pkt.Reliable = reliable;
		const uint8_t* bytePtr = static_cast<const uint8_t*>(data);
		pkt.Data.assign(bytePtr, bytePtr + len);

		{
			std::lock_guard<std::mutex> lock(m_OutboundMutex);
			m_OutboundQueue.push_back(std::move(pkt));
		}
		m_OutboundCV.notify_one();
	}

	void ENetThreadedDriver::DisconnectPeer(int peerIndex)
	{
		if (m_Role.load(std::memory_order_relaxed) == Role::Offline)
		{
			return;
		}

		OutboundPacket pkt;
		pkt.CmdType = OutboundPacket::Type::DisconnectPeer;
		pkt.PeerIndex = peerIndex;

		{
			std::lock_guard<std::mutex> lock(m_OutboundMutex);
			m_OutboundQueue.push_back(std::move(pkt));
		}
		m_OutboundCV.notify_one();
	}

	void ENetThreadedDriver::BroadcastPacket(ePacketChannel channel, const void* data, size_t len, bool reliable)
	{
		SendPacket(kInvalidPeerHandle, channel, data, len, reliable);
	}

	void ENetThreadedDriver::PunchHole(const std::string& targetIP, uint16_t targetPort, int count)
	{
		if (targetIP.empty() || targetPort == 0)
		{
			return;
		}

		OutboundPacket pkt;
		pkt.CmdType = OutboundPacket::Type::PunchHole;
		pkt.PunchIP = targetIP;
		pkt.PunchPort = targetPort;
		pkt.PunchCount = count > 0 ? count : 10;

		{
			std::lock_guard<std::mutex> lock(m_OutboundMutex);
			m_OutboundQueue.push_back(std::move(pkt));
		}
		m_OutboundCV.notify_one();
	}

	void ENetThreadedDriver::PollEvents(std::vector<NetworkDriverEvent>& outEvents)
	{
		std::lock_guard<std::mutex> lock(m_InboundMutex);
		if (!m_InboundQueue.empty())
		{
			outEvents.swap(m_InboundQueue);
		}
		else
		{
			outEvents.clear();
		}
	}

	std::string ENetThreadedDriver::GetListenAddress() const
	{
		std::lock_guard<std::mutex> lock(m_StateMutex);
		return m_ListenAddress;
	}

	uint32_t ENetThreadedDriver::GetPeerRtt(int peerIndex) const
	{
		std::lock_guard<std::mutex> lock(m_RttMutex);
		if (m_Role.load(std::memory_order_relaxed) == Role::Client)
		{
			return m_ServerRtt;
		}
		auto it = m_PeerRtt.find(peerIndex);
		return (it != m_PeerRtt.end()) ? it->second : 0;
	}

	bool ENetThreadedDriver::IsPeerConnected(int peerIndex) const
	{
		if (m_Role.load(std::memory_order_relaxed) == Role::Host)
		{
			std::lock_guard<std::mutex> lock(m_PeerMutex);
			return m_PeerMap.find(peerIndex) != m_PeerMap.end();
		}
		if (m_Role.load(std::memory_order_relaxed) == Role::Client)
		{
			return m_FullyConnected.load(std::memory_order_relaxed);
		}
		return false;
	}

	void ENetThreadedDriver::StartWorker()
	{
		m_WorkerRunning.store(true, std::memory_order_release);
		m_WorkerThread = std::thread(&ENetThreadedDriver::WorkerLoop, this);
	}

	void ENetThreadedDriver::StopWorker()
	{
		if (m_WorkerRunning.exchange(false, std::memory_order_acq_rel))
		{
			m_OutboundCV.notify_all();
			if (m_WorkerThread.joinable())
			{
				m_WorkerThread.join();
			}
		}
	}

	void ENetThreadedDriver::WorkerLoop()
	{
		std::vector<OutboundPacket> pendingOutbound;
		uint64_t loopIterations = 0;
		uint64_t eventsProcessed = 0;
		uint64_t serviceCallErrors = 0;
		auto lastHeartbeat = std::chrono::steady_clock::now();

		while (m_WorkerRunning.load(std::memory_order_relaxed))
		{
			loopIterations++;
			// 1. Drain outbound commands
			{
				std::unique_lock<std::mutex> lock(m_OutboundMutex);
				if (m_OutboundQueue.empty())
				{
					// Wait up to 2ms for incoming outbound packets or shutdown signal
					m_OutboundCV.wait_for(lock, std::chrono::milliseconds(2), [this] {
						return !m_OutboundQueue.empty() || !m_WorkerRunning.load(std::memory_order_relaxed);
					});
				}
				if (!m_OutboundQueue.empty())
				{
					pendingOutbound.swap(m_OutboundQueue);
				}
			}

			// 2. Transmit queued outbound packets
			if (m_Host && !pendingOutbound.empty())
			{
				Role role = m_Role.load(std::memory_order_relaxed);

				for (auto& pkt : pendingOutbound)
				{
					if (pkt.CmdType == OutboundPacket::Type::DisconnectPeer)
					{
						if (role == Role::Host)
						{
							std::lock_guard<std::mutex> lock(m_PeerMutex);
							auto it = m_PeerMap.find(pkt.PeerIndex);
							if (it != m_PeerMap.end() && it->second)
							{
								enet_peer_disconnect_now(it->second, 0);
								m_PeerMap.erase(it);
								{
									std::lock_guard<std::mutex> rttLock(m_RttMutex);
									m_PeerRtt.erase(pkt.PeerIndex);
								}
							}
						}
						continue;
					}

					if (pkt.CmdType == OutboundPacket::Type::PunchHole)
					{
						if (m_Host && m_Host->socket != ENET_SOCKET_NULL && !pkt.PunchIP.empty() && pkt.PunchPort != 0)
						{
							ENetAddress targetAddr = {};
							if (enet_address_set_host(&targetAddr, pkt.PunchIP.c_str()) == 0)
							{
								targetAddr.port = pkt.PunchPort;
								const char* punchPayload = "CH_PUNCH";
								ENetBuffer buffer;
								buffer.data = (void*)punchPayload;
								buffer.dataLength = strlen(punchPayload);

								for (int i = 0; i < pkt.PunchCount; ++i)
								{
									enet_socket_send(m_Host->socket, &targetAddr, &buffer, 1);
								}
								CH_CORE_INFO("[ENet] Sent {} NAT punch packets directly from ENet game socket to {}:{}",
											 pkt.PunchCount, pkt.PunchIP, pkt.PunchPort);
							}
						}
						continue;
					}

					if (pkt.Data.empty())
					{
						continue;
					}

					enet_uint32 flags = pkt.Reliable ? ENET_PACKET_FLAG_RELIABLE : 0;
					ENetPacket* enetPacket = enet_packet_create(pkt.Data.data(), pkt.Data.size(), flags);
					if (!enetPacket)
					{
						continue;
					}

					enet_uint8 channel = static_cast<enet_uint8>(pkt.Channel);

					if (pkt.PeerIndex == kInvalidPeerHandle)
					{
						// Broadcast
						if (role == Role::Host)
						{
							enet_host_broadcast(m_Host, channel, enetPacket);
						}
						else if (role == Role::Client && m_ServerPeer)
						{
							if (enet_peer_send(m_ServerPeer, channel, enetPacket) != 0)
							{
								if (enetPacket->referenceCount == 0)
								{
									enet_packet_destroy(enetPacket);
								}
							}
						}
						else if (enetPacket->referenceCount == 0)
						{
							enet_packet_destroy(enetPacket);
						}
					}
					else
					{
						// Targeted send
						bool sent = false;
						if (role == Role::Host)
						{
							std::lock_guard<std::mutex> lock(m_PeerMutex);
							auto it = m_PeerMap.find(pkt.PeerIndex);
							if (it != m_PeerMap.end() && it->second)
							{
								if (enet_peer_send(it->second, channel, enetPacket) == 0)
								{
									sent = true;
								}
							}
						}
						else if (role == Role::Client && m_ServerPeer)
						{
							if (enet_peer_send(m_ServerPeer, channel, enetPacket) == 0)
							{
								sent = true;
							}
						}

						if (!sent && enetPacket->referenceCount == 0)
						{
							enet_packet_destroy(enetPacket);
						}
					}
				}

				pendingOutbound.clear();
				enet_host_flush(m_Host);
			}

			// 3. Service ENet events
			if (m_Host)
			{
				ENetEvent event;
				while (m_Host)
				{
					int svcResult = enet_host_service(m_Host, &event, 0);
					if (svcResult < 0)
					{
						serviceCallErrors++;
						CH_CORE_WARN("[ENet][Worker] enet_host_service returned error: {}", svcResult);
						break;
					}
					else if (svcResult == 0)
					{
						// No events available — normal, exit loop
						break;
					}

					eventsProcessed++;

					Role role = m_Role.load(std::memory_order_relaxed);

					switch (event.type)
					{
					case ENET_EVENT_TYPE_CONNECT: {
						if (role == Role::Host)
						{
							int clientIndex = 0;
							{
								std::lock_guard<std::mutex> lock(m_PeerMutex);
								while (m_PeerMap.find(clientIndex) != m_PeerMap.end())
								{
									++clientIndex;
								}
								m_PeerMap[clientIndex] = event.peer;
							}
							event.peer->data = reinterpret_cast<void*>(static_cast<intptr_t>(clientIndex));
							enet_peer_timeout(event.peer, 32, 15000, 45000);

							char peerIp[64] = {};
							enet_address_get_host_ip(&event.peer->address, peerIp, sizeof(peerIp));
							CH_CORE_INFO(
								"[Network][Host] Low-level ENet peer connected from {}:{} -> assigned ClientIndex={}",
								peerIp, event.peer->address.port, clientIndex);

							NetworkDriverEvent ev;
							ev.Type = NetworkDriverEventType::Connected;
							ev.PeerIndex = clientIndex;

							std::lock_guard<std::mutex> lock(m_InboundMutex);
							m_InboundQueue.push_back(std::move(ev));
						}
						else if (role == Role::Client)
						{
							m_FullyConnected.store(true, std::memory_order_relaxed);
							enet_peer_timeout(event.peer, 32, 15000, 45000);

							CH_CORE_INFO("[Network][Client] Low-level ENet connection confirmed! (RTT: {}ms)",
										 event.peer->roundTripTime);

							NetworkDriverEvent ev;
							ev.Type = NetworkDriverEventType::Connected;
							ev.PeerIndex = 0;

							std::lock_guard<std::mutex> lock(m_InboundMutex);
							m_InboundQueue.push_back(std::move(ev));
						}
						break;
					}

					case ENET_EVENT_TYPE_DISCONNECT:
					case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT: {
						bool isTimeout = (event.type == ENET_EVENT_TYPE_DISCONNECT_TIMEOUT);
						if (role == Role::Host)
						{
							int clientIndex = static_cast<int>(reinterpret_cast<intptr_t>(event.peer->data));
							{
								std::lock_guard<std::mutex> lock(m_PeerMutex);
								for (auto it = m_PeerMap.begin(); it != m_PeerMap.end(); ++it)
								{
									if (it->second == event.peer)
									{
										m_PeerMap.erase(it);
										break;
									}
								}
							}

							{
								std::lock_guard<std::mutex> lock(m_RttMutex);
								m_PeerRtt.erase(clientIndex);
							}

							char peerIp[64] = {};
							enet_address_get_host_ip(&event.peer->address, peerIp, sizeof(peerIp));
							CH_CORE_WARN("[Network][Host] Low-level ENet peer #{}({}:{}) {} (code={}).", clientIndex,
										 peerIp, event.peer->address.port, isTimeout ? "timed out" : "disconnected",
										 event.data);

							NetworkDriverEvent ev;
							ev.Type = NetworkDriverEventType::Disconnected;
							ev.PeerIndex = clientIndex;

							std::lock_guard<std::mutex> lock(m_InboundMutex);
							m_InboundQueue.push_back(std::move(ev));
						}
						else if (role == Role::Client)
						{
							m_FullyConnected.store(false, std::memory_order_relaxed);
							m_Role.store(Role::Offline, std::memory_order_relaxed);
							m_ServerPeer = nullptr;

							CH_CORE_WARN("[Network][Client] Low-level ENet {} (code={}).",
										 isTimeout ? "connection timed out (no response from server)"
												   : "disconnected from server",
										 event.data);

							NetworkDriverEvent ev;
							ev.Type = NetworkDriverEventType::Disconnected;
							ev.PeerIndex = 0;

							std::lock_guard<std::mutex> lock(m_InboundMutex);
							m_InboundQueue.push_back(std::move(ev));
						}
						break;
					}

					case ENET_EVENT_TYPE_RECEIVE: {
						int peerIndex = 0;
						if (role == Role::Host)
						{
							peerIndex = static_cast<int>(reinterpret_cast<intptr_t>(event.peer->data));
						}

						NetworkDriverEvent ev;
						ev.Type = NetworkDriverEventType::PacketReceived;
						ev.PeerIndex = peerIndex;
						ev.Channel = event.channelID;
						ev.Data.assign(event.packet->data, event.packet->data + event.packet->dataLength);

						// Free low-level packet immediately on network thread
						enet_packet_destroy(event.packet);

						std::lock_guard<std::mutex> lock(m_InboundMutex);
						m_InboundQueue.push_back(std::move(ev));
						break;
					}

					default:
						break;
					}
				}

				// 3.5. Diagnostic heartbeat — log every 5 seconds
				{
					auto now = std::chrono::steady_clock::now();
					if (now - lastHeartbeat >= std::chrono::seconds(5))
					{
						Role role = m_Role.load(std::memory_order_relaxed);
						CH_CORE_INFO("[ENet][Worker] Alive — role={}, loop={}, events={}, service-errors={}, host={}",
									 static_cast<int>(role), loopIterations, eventsProcessed, serviceCallErrors,
									 (void*)m_Host);
						lastHeartbeat = now;
					}
				}

				// 4. Update RTT
				{
					std::lock_guard<std::mutex> lock(m_RttMutex);
					if (m_Role.load(std::memory_order_relaxed) == Role::Client && m_ServerPeer)
					{
						m_ServerRtt = enet_peer_get_rtt(m_ServerPeer);
					}
					else if (m_Role.load(std::memory_order_relaxed) == Role::Host)
					{
						std::lock_guard<std::mutex> peerLock(m_PeerMutex);
						for (auto& [idx, peer] : m_PeerMap)
						{
							if (peer)
							{
								m_PeerRtt[idx] = enet_peer_get_rtt(peer);
							}
						}
					}
				}
			}
		}
	}

} // namespace Chained
