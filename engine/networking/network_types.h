#ifndef CH_NETWORK_TYPES_H
#define CH_NETWORK_TYPES_H

#include <cstdint>
#include <string>
#include <vector>

namespace Chained
{
	using NetworkPeerHandle = int;
	constexpr NetworkPeerHandle kInvalidPeerHandle = -1;

	/// Network ID reserved for the host player in multiplayer sessions.
	static constexpr uint64_t kHostNetworkID = 1;

	/// Default UDP port for game networking (ENet).
	static constexpr uint16_t kDefaultPort = 7777;

	enum ePacketChannel : uint8_t
	{
		SYSTEM = 0, // reliable
		SYNC,		// unreliable
		EVENT,		// reliable
		SCRIPT,		// reliable
		COUNT
	};

	enum class Role : uint8_t
	{
		Offline = 0,
		Host,
		Client,
	};

	enum class DriverType : uint8_t
	{
		ENet = 0,
	};

	enum class NetworkError : uint8_t
	{
		None = 0,
		ENetInitFailed,
		AlreadyConnected,
		CreateHostFailed,
		ConnectFailed,
		BindFailed,
	};

	enum class NetworkDriverEventType : uint8_t
	{
		None = 0,
		Connected,
		Disconnected,
		PacketReceived
	};

	struct NetworkDriverEvent
	{
		NetworkDriverEventType Type = NetworkDriverEventType::None;
		int PeerIndex = kInvalidPeerHandle;
		uint8_t Channel = 0;
		std::vector<uint8_t> Data;
	};

	struct ChatMessagePacket
	{
		uint64_t SenderNetworkID = 0;
		std::string SenderName;
		std::string Message;
	};

} // namespace Chained

#endif // CH_NETWORK_TYPES_H
