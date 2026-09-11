#include "network_service.h"
#include "engine/common/platform_detection.h"
#include <thread>
#include <algorithm>
#include <cctype>
#include <chrono>

#if CH_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

namespace Chained
{
	Network::~Network()
	{
		if (m_Session.IsConnected())
		{
			CH_CORE_WARN("Network: Destructor called before explicit Shutdown().");
			Shutdown();
		}
	}

	void Network::Initialize()
	{
		NetworkError err = m_Session.Initialize();
		if (err != NetworkError::None)
		{
			SetEnabled(false);
			return;
		}

		m_Transport.SetSession(&m_Session);

		m_Session.SetConnectionCallback(
			[this](int peerIndex, uint64_t networkID) { OnClientConnectedInternal(peerIndex, networkID); });
		m_Session.SetDisconnectionCallback(
			[this](int peerIndex, uint64_t networkID) { OnClientDisconnectedInternal(peerIndex, networkID); });
		m_Session.SetEventCallback([this](int peerIndex, MessageType type, const uint8_t* data, size_t len) {
			if (type == MessageType_Count)
			{
				m_Transport.HandleIncomingPacket(peerIndex, data, len);
			}
		});

		CH_CORE_TRACE("[Network] Initialized in Offline mode.");

		// Configure Firebase Realtime Database for room-code signaling
		static constexpr const char* kFirebaseHost = "chained-decos-default-rtdb.europe-west1.firebasedatabase.app";
		m_SignalingClient.SetFirebaseUrl(kFirebaseHost);
		CH_CORE_TRACE("[Network] Signaling: Firebase RTDB ({})", kFirebaseHost);
	}

	void Network::Shutdown()
	{
		uint16_t portToUnmap = m_Session.GetPort();

		// Close hole punch socket
		if (m_HolePunchSocket >= 0)
		{
#if CH_PLATFORM_WINDOWS
			closesocket(m_HolePunchSocket);
#else
			close(m_HolePunchSocket);
#endif
			m_HolePunchSocket = -1;
			m_HolePunchActive = false;
		}

		// Join background threads before destroying state
		if (m_IPFetchThread.joinable())
		{
			m_IPFetchThread.join();
		}

		Disconnect();

		if (m_UpnpMapper.IsAvailable() && portToUnmap != 0)
		{
			m_UpnpMapper.RemoveMapping(portToUnmap, "UDP");
			m_UpnpMapper.Shutdown();
		}

		m_Session.Shutdown();

		CH_CORE_INFO("Network: Shutdown complete.");
	}

	void Network::HostGame(uint16_t port, int maxClients)
	{
		CH_CORE_INFO("[Network][Host] Starting server on port {} (max {} clients)...", port, maxClients);

		if (!m_TestMode)
		{
			if (!m_UpnpMapper.IsAvailable())
			{
				m_UpnpMapper.Initialize();
			}
			if (m_UpnpMapper.IsAvailable())
			{
				bool mapped = m_UpnpMapper.AddMapping(port, "UDP", "ChainedDecos");
				if (mapped)
				{
					CH_CORE_INFO("[Network][Host] UPnP: OK | Port {} (UDP) mapped | Router LAN: {}", port,
								 m_UpnpMapper.GetLanIP());
				}
				else
				{
					CH_CORE_WARN("[Network][Host] UPnP: Mapping failed — port {} may need manual forwarding.", port);
				}
			}
			else
			{
				CH_CORE_WARN("[Network][Host] UPnP: Unavailable (port {} must be forwarded manually if behind NAT).",
							 port);
			}

			// Query STUN synchronously BEFORE ENet binds the port,
			// so STUN can actually bind to localPort and verify it's reachable from the internet.
			QueryStunPublicEndpointSync(port);
		}

		m_Session.SetDriverType(DriverType::ENet);
		NetworkError err = m_Session.HostGame(port, maxClients);
		if (err != NetworkError::None)
		{
			CH_CORE_ERROR("[Network][Host] Failed to start host server (error={}).", static_cast<int>(err));
			return;
		}

		m_PlayerManager.Reset();
		m_PlayerManager.SetHostNetworkID(kHostNetworkID);
		m_PlayerManager.AddHostSelf(kHostNetworkID, m_PlayerManager.GetLocalPlayerName(),
									m_PlayerManager.GetLocalSkinIndex());

		CH_CORE_INFO("[Network][Host] Server listening on LAN: {} (ENet Host active).", GetListenAddress());
	}

	uint32_t Network::HostRoom(uint16_t port, int maxClients)
	{
		// Generate random 4-digit room code (1000..9999)
		uint32_t roomCode = 1000 + (static_cast<uint32_t>(rand()) % 9000);
		m_RoomCode = roomCode;

		CH_CORE_INFO("[Network][Host] Creating Room #{} on port {}...", roomCode, port);
		HostGame(port, maxClients);

		// Use m_CachedPublicIP which has the correct effective port
		// (accounts for UPnP mapping vs raw STUN port mismatch).
		// Raw STUN may return a different port than ENet listens on when
		// NAT maps STUN sockets to ephemeral ports differently.
		std::string stunIP;
		uint16_t stunPort = 0;
		{
			std::lock_guard<std::mutex> lock(m_PublicIPMutex);
			if (!m_CachedPublicIP.empty() && m_CachedPublicIP != "Fetching...")
			{
				size_t colon = m_CachedPublicIP.rfind(':');
				if (colon != std::string::npos)
				{
					stunIP = m_CachedPublicIP.substr(0, colon);
					try
					{
						stunPort = static_cast<uint16_t>(std::stoi(m_CachedPublicIP.substr(colon + 1)));
					} catch (...)
					{
						stunPort = port;
					}
				}
			}
		}

		if (stunIP.empty())
		{
			stunIP = "127.0.0.1";
			stunPort = port;
		}

		CH_CORE_INFO("[Network][Host] Registering Room #{} on signaling coordinator with endpoint {}:{}...", roomCode,
					 stunIP, stunPort);
		auto res = m_SignalingClient.CreateRoomSync(std::to_string(roomCode), stunIP, stunPort);
		if (res.Success)
		{
			m_SignalingPolling = true;
			m_SignalingPollTimer = 0.0f;
			CH_CORE_INFO("[Network][Host] Room #{} is LIVE! Share code '{}' with friends.", roomCode, roomCode);
		}
		else
		{
			CH_CORE_WARN("[Network][Host] Signaling registration failed: {} (direct IP connect still works)",
						 res.Error);
		}

		return roomCode;
	}

	void Network::ConnectRoom(uint32_t roomCode)
	{
		m_RoomCode = roomCode;
		CH_CORE_INFO("[Network][Client] Connecting to Room #{} via signaling coordinator...", roomCode);

		// Fast STUN query for client public endpoint
		std::string clientIP = "127.0.0.1";
		uint16_t clientPort = 0;

		auto stunResult = m_StunClient.QueryPublicEndpointSync(0, 1500);
		if (stunResult.Success)
		{
			clientIP = stunResult.PublicIP;
			clientPort = stunResult.PublicPort;
		}

		// Join room on signaling server
		auto joinRes = m_SignalingClient.JoinRoomSync(std::to_string(roomCode), clientIP, clientPort);
		if (!joinRes.Success || joinRes.HostIP.empty() || joinRes.HostPort == 0)
		{
			CH_CORE_ERROR("[Network][Client] Failed to join Room #{}: {}", roomCode, joinRes.Error);
			return;
		}

		CH_CORE_INFO("[Network][Client] Room #{} resolved to Host at {}:{}", roomCode, joinRes.HostIP,
					 joinRes.HostPort);

		// Connect to Host
		ConnectTo(joinRes.HostIP, joinRes.HostPort);

		// Simultaneous punch burst towards host from our newly created game socket
		m_Session.PunchHole(joinRes.HostIP, joinRes.HostPort, 10);
	}

	void Network::ConnectTo(const std::string& ip, uint16_t port)
	{
		CH_CORE_INFO("[Network][Client] Connect requested to target {} (port={})...", ip.substr(0, 30), port);

		// Check if the input is a pure numeric room code (4 to 6 digits)
		bool isNumericRoomCode = (ip.length() >= 4 && ip.length() <= 6 &&
								  std::all_of(ip.begin(), ip.end(), [](char c) { return std::isdigit(c); }));
		if (isNumericRoomCode)
		{
			ConnectRoom(static_cast<uint32_t>(std::stoul(ip)));
			return;
		}

		m_Session.SetDriverType(DriverType::ENet);

		// Skip hairpin check for local addresses — connect directly
		bool isLocal = (ip == "127.0.0.1" || ip == "localhost" || ip == "::1" || ip.rfind("192.168.", 0) == 0 ||
						ip.rfind("10.", 0) == 0 || ip.rfind("172.", 0) == 0);

		std::string resolvedIP = ip;

		if (!isLocal)
		{
			// Auto-detect hairpin NAT: if connecting to our own public IP, use localhost.
			// Populate the public IP cache via a quick STUN query if we don't have it yet
			// (e.g., client instance that never hosted).

			std::string myPubIP;
			{
				std::lock_guard<std::mutex> lock(m_PublicIPMutex);
				myPubIP = m_ResolvedPublicIP;
				if (myPubIP.empty() && !m_CachedPublicIP.empty() && m_CachedPublicIP != "Fetching...")
				{
					myPubIP = m_CachedPublicIP;
					size_t colon = myPubIP.rfind(':');
					if (colon != std::string::npos)
					{
						myPubIP = myPubIP.substr(0, colon);
					}
				}
			}

			// If we still don't know our public IP, do a fast sync STUN query now
			// (ephemeral port 0, 1500ms timeout) to enable hairpin detection.
			if (myPubIP.empty())
			{
				CH_CORE_INFO("[Network][Client] No public IP cached — running fast STUN for hairpin detection...");
				auto stunResult = m_StunClient.QueryPublicEndpointSync(0, 1500);
				if (stunResult.Success)
				{
					myPubIP = stunResult.PublicIP;
					std::lock_guard<std::mutex> lock(m_PublicIPMutex);
					// Cache it so future calls are instant
					m_CachedPublicIP = stunResult.PublicIP + ":" + std::to_string(stunResult.PublicPort);
					CH_CORE_INFO("[Network][Client] Fast STUN resolved public IP: {}", m_CachedPublicIP);
				}
			}

			if (!myPubIP.empty() && ip == myPubIP)
			{
				resolvedIP = "127.0.0.1";
				CH_CORE_INFO("[Network][Client] Hairpin NAT detected (target matches local public IP {}) — redirecting "
							 "to 127.0.0.1:{}",
							 myPubIP, port);
			}
		}

		NetworkError err = m_Session.ConnectTo(resolvedIP, port);
		if (err != NetworkError::None)
		{
			CH_CORE_ERROR("[Network][Client] Failed to initiate connection (error={}).", static_cast<int>(err));
			return;
		}

		m_PlayerManager.Reset();

		// Store connection info for potential reconnect
		m_ReconnectIP = ip;
		m_ReconnectPort = port;
		m_ReconnectAttempts = 0;
		m_ReconnectPending = false;
		m_ReconnectTimer = 0.0f;

		CH_CORE_INFO("[Network][Client] Connecting to {} (resolved: {}:{})...", ip, resolvedIP, port);
	}

	void Network::Disconnect()
	{
		// Clean up hole punch state
		if (m_HolePunchSocket >= 0)
		{
#if CH_PLATFORM_WINDOWS
			closesocket(m_HolePunchSocket);
#else
			close(m_HolePunchSocket);
#endif
			m_HolePunchSocket = -1;
		}
		m_HolePunchActive = false;
		m_HolePunchCount = 0;
		m_HolePunchTimer = 0.0f;
		m_HolePunchCallback = nullptr;

		// Cancel any pending reconnect
		m_ReconnectPending = false;
		m_ReconnectAttempts = 0;
		m_ReconnectTimer = 0.0f;
		m_ReconnectIP.clear();
		m_ReconnectPort = 0;

		m_RoomCode = 0;
		m_SignalingPolling = false;
		m_SignalingPollTimer = 0.0f;

		m_Session.Disconnect();
		m_Transport.ClearPacketCallback();
		m_PlayerManager.Reset();
		m_PendingSceneChange.clear();
		{
			std::lock_guard<std::mutex> lock(m_PublicIPMutex);
			m_CachedPublicIP.clear();
		}
		CH_CORE_INFO("Network: Disconnected.");
	}

	void Network::Update(float dt)
	{
		m_Session.Update(dt);
		UpdateHolePunch(dt);

		// Host: poll signaling server for incoming peers to punch NAT
		if (m_Session.IsHost() && m_SignalingPolling && m_RoomCode != 0)
		{
			m_SignalingPollTimer += dt;
			if (m_SignalingPollTimer >= kSignalingPollInterval)
			{
				m_SignalingPollTimer = 0.0f;
				auto pollRes = m_SignalingClient.PollRoomSync(std::to_string(m_RoomCode), 500);
				if (pollRes.Success && pollRes.ClientReady && !pollRes.ClientIP.empty() && pollRes.ClientPort != 0)
				{
					CH_CORE_INFO("[Network][Host] Peer joining detected from {}:{} -> executing NAT punch burst!",
								 pollRes.ClientIP, pollRes.ClientPort);
					m_Session.PunchHole(pollRes.ClientIP, pollRes.ClientPort, 10);
				}
			}
		}

		// Host: send heartbeat and check for dead clients
		if (m_Session.IsHost())
		{
			m_HeartbeatTimer += dt;
			if (m_HeartbeatTimer >= kHeartbeatInterval)
			{
				m_HeartbeatTimer = 0.0f;
				BroadcastPacket(MessageType_Heartbeat, false, [](ByteWriter& bw) {
					// Empty heartbeat — just the message type is enough
				});
			}
			m_Session.CheckPeerTimeouts(dt, kClientTimeoutSeconds);
		}
		else if (m_Session.IsClient() && m_Session.IsConnected())
		{
			m_HeartbeatTimer += dt;
			if (m_HeartbeatTimer >= kHeartbeatInterval)
			{
				m_HeartbeatTimer = 0.0f;
				SendToServer(MessageType_Heartbeat, nullptr, 0, false);
			}
		}

		// Detect client disconnect and trigger reconnect
		bool isClientNowConnected = m_Session.IsClient() && m_Session.IsFullyConnected();
		if (m_WasClientConnected && !isClientNowConnected && !m_ReconnectPending)
		{
			// Client was connected but now disconnected — start reconnect flow
			if (!m_ReconnectIP.empty() && m_ReconnectAttempts < kMaxReconnectAttempts)
			{
				m_ReconnectPending = true;
				m_ReconnectTimer = 0.0f;
				CH_CORE_WARN("Network: Connection lost. Will attempt reconnect ({}/{}).", m_ReconnectAttempts + 1,
							 kMaxReconnectAttempts);
			}
		}
		m_WasClientConnected = isClientNowConnected;

		// Client reconnect logic: if we were connected but now disconnected,
		// attempt automatic reconnect with exponential backoff.
		if (m_ReconnectPending && m_ReconnectAttempts < kMaxReconnectAttempts)
		{
			m_ReconnectTimer += dt;
			float backoff = 1.0f * static_cast<float>(1 << m_ReconnectAttempts); // 1s, 2s, 4s
			if (m_ReconnectTimer >= backoff)
			{
				m_ReconnectTimer = 0.0f;
				m_ReconnectAttempts++;
				CH_CORE_INFO("Network: Reconnect attempt {}/{} to {}:{}...", m_ReconnectAttempts, kMaxReconnectAttempts,
							 m_ReconnectIP, m_ReconnectPort);

				m_ReconnectPending = false;
				ConnectTo(m_ReconnectIP, m_ReconnectPort);
			}
		}
	}

	// ── Sending ──────────────────────────────────────────────────────────

	void Network::SendPacket(int clientIndex, MessageType type, const void* data, size_t len, bool reliable)
	{
		m_Transport.SendPacket(clientIndex, type, data, len, reliable);
	}

	void Network::SendPacket(int clientIndex, ePacketChannel channel, MessageType type, const void* data, size_t len,
							 bool reliable)
	{
		m_Transport.SendPacket(clientIndex, channel, type, data, len, reliable);
	}

	void Network::BroadcastPacket(MessageType type, bool reliable, const std::function<void(ByteWriter&)>& populate)
	{
		m_Transport.BroadcastPacket(type, reliable, populate);
	}

	void Network::BroadcastPacket(ePacketChannel channel, MessageType type, bool reliable,
								  const std::function<void(ByteWriter&)>& populate)
	{
		m_Transport.BroadcastPacket(channel, type, reliable, populate);
	}

	void Network::SendToServer(MessageType type, const void* data, size_t len, bool reliable)
	{
		m_Transport.SendToServer(type, data, len, reliable);
	}

	void Network::SendToServer(ePacketChannel channel, MessageType type, const void* data, size_t len, bool reliable)
	{
		m_Transport.SendToServer(channel, type, data, len, reliable);
	}

	void Network::BroadcastSceneChange(const char* scenePath)
	{
		m_Transport.BroadcastSceneChange(scenePath);
	}

	// ── Packet callback ──────────────────────────────────────────────────

	void Network::SetPacketCallback(PacketCallback callback)
	{
		m_Transport.SetPacketCallback(std::move(callback));
	}

	void Network::ClearPacketCallback()
	{
		m_Transport.ClearPacketCallback();
	}

	// ── Address ──────────────────────────────────────────────────────────

	std::string Network::GetListenAddress()
	{
		if (m_UpnpMapper.IsAvailable() && m_UpnpMapper.GetLanIP()[0] != '\0')
		{
			return std::string(m_UpnpMapper.GetLanIP()) + ":" + std::to_string(m_Session.GetPort());
		}
		return m_Session.GetListenAddress();
	}

	std::string Network::GetPublicAddress()
	{
		std::lock_guard<std::mutex> lock(m_PublicIPMutex);
		if (!m_CachedPublicIP.empty())
		{
			return m_CachedPublicIP;
		}
		return "Fetching...";
	}

	// ── STUN / NAT Traversal ─────────────────────────────────────────────

	void Network::QueryStunPublicEndpoint(uint16_t localPort)
	{
		CH_CORE_INFO("Network: Querying STUN servers for public endpoint (local port {})...", localPort);
		m_StunClient.QueryPublicEndpoint(localPort, [this, localPort](const StunClient::StunResult& result) {
			std::lock_guard<std::mutex> lock(m_PublicIPMutex);
			if (result.Success)
			{
				uint16_t effectivePort = localPort;
				if (!m_UpnpMapper.IsPortMapped() && result.BoundToRequestedPort && result.PublicPort != 0)
				{
					effectivePort = result.PublicPort;
				}

				m_CachedPublicIP = result.PublicIP + ":" + std::to_string(effectivePort);
				CH_CORE_INFO("Network: Public endpoint: {} (IP: {}, Port: {}, UPnP mapped: {}, STUN bound: {})",
							 m_CachedPublicIP, result.PublicIP, effectivePort,
							 m_UpnpMapper.IsPortMapped() ? "yes" : "no", result.BoundToRequestedPort ? "yes" : "no");
			}
			else
			{
				CH_CORE_WARN("Network: STUN query failed — {}", result.Error);
			}
		});
	}

	void Network::QueryStunPublicEndpointSync(uint16_t localPort)
	{
		CH_CORE_INFO("Network: Querying STUN servers for public endpoint (local port {}, sync)...", localPort);
		auto result = m_StunClient.QueryPublicEndpointSync(localPort, 3000);
		std::lock_guard<std::mutex> lock(m_PublicIPMutex);
		if (result.Success)
		{
			uint16_t effectivePort = localPort;
			if (!m_UpnpMapper.IsPortMapped() && result.BoundToRequestedPort && result.PublicPort != 0)
			{
				effectivePort = result.PublicPort;
			}

			m_CachedPublicIP = result.PublicIP + ":" + std::to_string(effectivePort);
			CH_CORE_INFO("Network: Public endpoint: {} (IP: {}, Port: {}, UPnP mapped: {}, STUN bound: {})",
						 m_CachedPublicIP, result.PublicIP, effectivePort, m_UpnpMapper.IsPortMapped() ? "yes" : "no",
						 result.BoundToRequestedPort ? "yes" : "no");
		}
		else
		{
			CH_CORE_WARN("Network: STUN query failed — {}", result.Error);
		}
	}

	void Network::StartHolePunch(const std::string& remoteIP, uint16_t remotePort)
	{
		if (m_HolePunchActive)
		{
			CH_CORE_WARN("Network: Hole punch already in progress.");
			return;
		}

		m_HolePunchActive = true;
		m_HolePunchTimer = 0.0f;
		m_HolePunchCount = 0;
		m_HolePunchRemoteIP = remoteIP;
		m_HolePunchRemotePort = remotePort;

		// Create UDP socket and bind to the same port ENet uses,
		// so the hole punch comes from the port peers will reach us on.
		m_HolePunchSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (m_HolePunchSocket < 0
#if CH_PLATFORM_WINDOWS
			|| m_HolePunchSocket == INVALID_SOCKET
#endif
		)
		{
			CH_CORE_ERROR("Network: Failed to create hole punch socket.");
			m_HolePunchActive = false;
			return;
		}

		uint16_t localPort = m_Session.IsClient() ? 0 : m_Session.GetPort();
		sockaddr_in bindAddr = {};
		bindAddr.sin_family = AF_INET;
		bindAddr.sin_addr.s_addr = INADDR_ANY;
		bindAddr.sin_port = htons(localPort);

		if (localPort != 0 && bind(m_HolePunchSocket, (sockaddr*)&bindAddr, sizeof(bindAddr)) < 0)
		{
			CH_CORE_WARN("Network: Failed to bind hole punch socket to port {} (ENet port may be in use).", localPort);
			// Continue anyway — OS-assigned port may still work for NAT traversal
		}

		CH_CORE_INFO("Network: Starting hole punch to {}:{} (local port {}, max {} attempts)", remoteIP, remotePort,
					 localPort, m_HolePunchMaxAttempts);
	}

	void Network::PerformHolePunch(uint16_t localPort, const std::string& remoteIP, uint16_t remotePort)
	{
		if (m_HolePunchSocket < 0)
		{
			return;
		}

		sockaddr_in dest = {};
		dest.sin_family = AF_INET;
		dest.sin_port = htons(remotePort);
		if (inet_pton(AF_INET, remoteIP.c_str(), &dest.sin_addr) <= 0)
		{
			struct addrinfo hints = {}, *res = nullptr;
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_DGRAM;
			hints.ai_protocol = IPPROTO_UDP;

			if (getaddrinfo(remoteIP.c_str(), nullptr, &hints, &res) != 0 || !res)
			{
				CH_CORE_ERROR("Network: Hole punch DNS failed for {}", remoteIP);
				return;
			}
			memcpy(&dest, res->ai_addr, sizeof(sockaddr_in));
			dest.sin_port = htons(remotePort);
			freeaddrinfo(res);
		}

		// Send a small UDP packet to punch through NAT
		const char* punch = "CH_PUNCH";
		sendto(m_HolePunchSocket, punch, (int)strlen(punch), 0, (sockaddr*)&dest, sizeof(dest));
		m_HolePunchCount++;
		CH_CORE_INFO("Network: Hole punch packet #{} sent to {}:{}", m_HolePunchCount, remoteIP, remotePort);
	}

	void Network::UpdateHolePunch(float dt)
	{
		if (!m_HolePunchActive)
		{
			return;
		}

		m_HolePunchTimer += dt;

		// Send hole punch packets at regular intervals
		if (m_HolePunchTimer >= m_HolePunchInterval)
		{
			m_HolePunchTimer = 0.0f;
			PerformHolePunch(m_Session.GetPort(), m_HolePunchRemoteIP, m_HolePunchRemotePort);
		}

		// Check timeout
		if (m_HolePunchCount >= m_HolePunchMaxAttempts)
		{
			CH_CORE_INFO("Network: Hole punch complete ({} packets sent).", m_HolePunchCount);
			m_HolePunchActive = false;
			if (m_HolePunchSocket >= 0)
			{
#if CH_PLATFORM_WINDOWS
				closesocket(m_HolePunchSocket);
#else
				close(m_HolePunchSocket);
#endif
				m_HolePunchSocket = -1;
			}
			if (m_HolePunchCallback)
			{
				m_HolePunchCallback(true);
			}
		}
	}

	// ── Player management ────────────────────────────────────────────────

	void Network::SetLocalPlayerInfo(const char* name, uint8_t skinIndex)
	{
		m_PlayerManager.SetLocalPlayerInfo(name, skinIndex);

		if (!m_Session.IsHost())
		{
			return;
		}

		m_PlayerManager.UpdatePlayerInfo(kHostNetworkID, name ? name : "Host", skinIndex);
		BroadcastPlayerList();
	}

	void Network::SendPlayerInfoToHost(const char* name, uint8_t skinIndex)
	{
		m_Transport.SendPlayerInfoToHost(name, skinIndex, m_PlayerManager.GetLocalNetworkID());
	}

	void Network::BroadcastPlayerList()
	{
		m_Transport.BroadcastPlayerList(m_PlayerManager.GetPlayerList());
	}

	void Network::SendChatMessage(const char* message)
	{
		m_Transport.SendChatMessage(message, m_PlayerManager.GetLocalNetworkID(), m_PlayerManager.GetLocalPlayerName());
	}

	void Network::StorePendingChatMessage(const ChatMessagePacket& pkt)
	{
		m_Transport.StorePendingChatMessage(pkt);
	}

	// ── Client info ──────────────────────────────────────────────────────

	size_t Network::GetClientCount() const
	{
		return m_PlayerManager.GetClientCount();
	}

	std::vector<int> Network::GetClients() const
	{
		return m_PlayerManager.GetClients();
	}

	bool Network::IsClientConnected(int clientIndex) const
	{
		return m_Session.IsClientConnected(clientIndex);
	}

	// ── Connection callbacks ─────────────────────────────────────────────

	void Network::OnClientConnectedInternal(int clientIndex, uint64_t /*networkID*/)
	{
		m_PlayerManager.OnClientConnected(clientIndex);
		const uint64_t assignedID = m_PlayerManager.GetNetworkIDForConnection(clientIndex);

		// Initialize heartbeat tracking for this peer
		m_Session.RecordPeerActivity(clientIndex);

		PlayerAssignMessage assignMsg;
		assignMsg.NetworkID = assignedID;
		ByteWriter w;
		assignMsg.Encode(w);
		m_Transport.SendPacket(clientIndex, MessageType_PlayerAssign, w.Data().data(), w.Data().size(), true);

		BroadcastPlayerList();
		CH_CORE_INFO("[Network][Peer] Client connected: ClientIndex={}, Assigned NetworkID={}", clientIndex,
					 assignedID);
	}

	void Network::OnClientDisconnectedInternal(int clientIndex, uint64_t /*networkID*/)
	{
		const uint64_t discID = m_PlayerManager.GetNetworkIDForConnection(clientIndex);
		m_PlayerManager.OnClientDisconnected(clientIndex);
		BroadcastPlayerList();
		CH_CORE_WARN("[Network][Peer] Client disconnected: ClientIndex={}, NetworkID={}", clientIndex, discID);
	}

} // namespace Chained
