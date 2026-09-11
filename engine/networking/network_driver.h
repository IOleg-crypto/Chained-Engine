#ifndef CH_NETWORK_DRIVER_H
#define CH_NETWORK_DRIVER_H

#include "network_types.h"
#include <string>
#include <vector>
#include <memory>

namespace Chained
{
	/// Pure virtual interface providing an abstraction layer for the low-level network transport.
	/// Decouples engine game loops, ECS, and scripts from transport-specific libraries (such as ENet).
	class INetworkDriver
	{
	public:
		virtual ~INetworkDriver() = default;

		virtual NetworkError Initialize() = 0;
		virtual void Shutdown() = 0;

		virtual NetworkError Host(uint16_t port, int maxClients) = 0;
		virtual NetworkError Connect(const std::string& ip, uint16_t port) = 0;
		virtual void Disconnect() = 0;
		virtual void DisconnectPeer(int peerIndex) = 0;

		virtual void SendPacket(int peerIndex, ePacketChannel channel, const void* data, size_t len, bool reliable) = 0;
		virtual void BroadcastPacket(ePacketChannel channel, const void* data, size_t len, bool reliable) = 0;

		/// Drains received events and incoming packets from the network driver.
		/// Safe to call from the main / simulation thread.
		virtual void PollEvents(std::vector<NetworkDriverEvent>& outEvents) = 0;

		virtual Role GetRole() const = 0;
		virtual bool IsConnected() const = 0;
		virtual bool IsFullyConnected() const = 0;
		virtual uint16_t GetPort() const = 0;
		virtual int GetMaxClients() const = 0;
		virtual std::string GetListenAddress() const = 0;
		virtual uint32_t GetPeerRtt(int peerIndex) const = 0;
		virtual bool IsPeerConnected(int peerIndex) const = 0;
		virtual void PunchHole(const std::string& targetIP, uint16_t targetPort, int count = 10)
		{
		}
	};
} // namespace Chained

#endif // CH_NETWORK_DRIVER_H
