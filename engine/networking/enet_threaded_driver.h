#ifndef CH_ENET_THREADED_DRIVER_H
#define CH_ENET_THREADED_DRIVER_H

#include "network_driver.h"
#include <enet.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Chained
{
	/// High-performance multi-threaded ENet implementation of INetworkDriver.
	/// Isolates ENet socket servicing and packet sending on a dedicated worker thread.
	/// Separates traffic into 4 dedicated channels: SYSTEM, SYNC, EVENT, and SCRIPT.
	class ENetThreadedDriver : public INetworkDriver
	{
	public:
		ENetThreadedDriver();
		~ENetThreadedDriver() override;

		NetworkError Initialize() override;
		void Shutdown() override;

		NetworkError Host(uint16_t port, int maxClients) override;
		NetworkError Connect(const std::string& ip, uint16_t port) override;
		void Disconnect() override;
		void DisconnectPeer(int peerIndex) override;

		void SendPacket(int peerIndex, ePacketChannel channel, const void* data, size_t len, bool reliable) override;
		void BroadcastPacket(ePacketChannel channel, const void* data, size_t len, bool reliable) override;

		void PollEvents(std::vector<NetworkDriverEvent>& outEvents) override;

		Role GetRole() const override
		{
			return m_Role.load(std::memory_order_relaxed);
		}
		bool IsConnected() const override
		{
			Role r = m_Role.load(std::memory_order_relaxed);
			if (r == Role::Client)
			{
				return m_FullyConnected.load(std::memory_order_relaxed);
			}
			return r != Role::Offline;
		}
		bool IsFullyConnected() const override
		{
			return m_FullyConnected.load(std::memory_order_relaxed);
		}
		uint16_t GetPort() const override
		{
			return m_Port.load(std::memory_order_relaxed);
		}
		int GetMaxClients() const override
		{
			return m_MaxClients.load(std::memory_order_relaxed);
		}
		std::string GetListenAddress() const override;
		uint32_t GetPeerRtt(int peerIndex) const override;
		bool IsPeerConnected(int peerIndex) const override;
		void PunchHole(const std::string& targetIP, uint16_t targetPort, int count = 10) override;

	private:
		struct OutboundPacket
		{
			enum class Type
			{
				Send,
				DisconnectPeer,
				PunchHole
			};
			Type CmdType = Type::Send;
			int PeerIndex = kInvalidPeerHandle;
			ePacketChannel Channel = ePacketChannel::SYSTEM;
			bool Reliable = true;
			std::string PunchIP;
			uint16_t PunchPort = 0;
			int PunchCount = 10;
			std::vector<uint8_t> Data;
		};

		void StartWorker();
		void StopWorker();
		void WorkerLoop();

		std::atomic<Role> m_Role{Role::Offline};
		std::atomic<bool> m_FullyConnected{false};
		std::atomic<uint16_t> m_Port{0};
		std::atomic<int> m_MaxClients{0};
		std::atomic<bool> m_WorkerRunning{false};
		std::atomic<bool> m_Initialized{false};

		mutable std::mutex m_StateMutex;
		std::string m_ListenAddress;

		// Worker thread and queues
		std::thread m_WorkerThread;
		std::mutex m_OutboundMutex;
		std::condition_variable m_OutboundCV;
		std::vector<OutboundPacket> m_OutboundQueue;

		std::mutex m_InboundMutex;
		std::vector<NetworkDriverEvent> m_InboundQueue;

		// Low-level ENet objects (solely accessed by worker thread during runtime)
		ENetHost* m_Host = nullptr;
		ENetPeer* m_ServerPeer = nullptr;
		mutable std::mutex m_PeerMutex;
		std::unordered_map<int, ENetPeer*> m_PeerMap;

		// RTT tracking
		mutable std::mutex m_RttMutex;
		std::unordered_map<int, uint32_t> m_PeerRtt;
		uint32_t m_ServerRtt = 0;
	};

} // namespace Chained

#endif // CH_ENET_THREADED_DRIVER_H
