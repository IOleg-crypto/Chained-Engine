#include <gtest/gtest.h>

#include "engine/networking/net_packet.h"
#include "engine/networking/network_service.h"

#include <chrono>
#include <cstring>
#include <thread>
#include <atomic>

using namespace Chained;

// Network loopback tests are flaky in CI (timing-sensitive, UPnP/firewall side-effects).
// They still run locally when CH_CI is not defined.
#ifndef CH_CI
namespace
{
	constexpr uint16_t kTestPortBase = 27599;
	std::atomic<uint16_t> s_PortCounter{0};

	class NetworkLoopbackTest : public ::testing::Test
	{
	protected:
		void SetUp() override
		{
			m_Port = kTestPortBase + (s_PortCounter.fetch_add(1) % 50);
			m_Host.SetTestMode(true);
			m_Client.SetTestMode(true);
			m_Host.Initialize();
			m_Client.Initialize();
			ASSERT_TRUE(m_Host.IsEnabled()) << "ENet failed to initialize";
			ASSERT_TRUE(m_Client.IsEnabled()) << "ENet failed to initialize";
		}

		void TearDown() override
		{
			m_Client.Shutdown();
			m_Host.Shutdown();
		}

		template <typename Predicate> bool PumpUntil(Predicate pred, std::chrono::milliseconds timeout)
		{
			const auto deadline = std::chrono::steady_clock::now() + timeout;
			while (std::chrono::steady_clock::now() < deadline)
			{
				m_Host.Update(1.0f / 60.0f);
				m_Client.Update(1.0f / 60.0f);
				if (pred())
				{
					return true;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(5));
			}
			return false;
		}

		uint16_t m_Port = 0;
		Network m_Host;
		Network m_Client;
	};
} // namespace

TEST_F(NetworkLoopbackTest, InitializeSucceeds)
{
	EXPECT_EQ(m_Host.GetRole(), Role::Offline);
	EXPECT_FALSE(m_Host.IsConnected());
}

TEST_F(NetworkLoopbackTest, HostGameOpensListenSocket)
{
	m_Host.HostGame(m_Port, 4);

	EXPECT_TRUE(m_Host.IsEnabled()) << "HostGame disabled the service — listen socket failed";
	EXPECT_EQ(m_Host.GetRole(), Role::Host);
	EXPECT_EQ(m_Host.GetPort(), m_Port);
}

TEST_F(NetworkLoopbackTest, ClientConnectsToHost)
{
	m_Host.HostGame(m_Port, 4);
	ASSERT_TRUE(m_Host.IsEnabled());

	m_Client.ConnectTo("127.0.0.1", m_Port);
	ASSERT_TRUE(m_Client.IsEnabled());
	EXPECT_EQ(m_Client.GetRole(), Role::Client);

	const bool connected = PumpUntil([this] { return m_Host.GetClientCount() == 1; }, std::chrono::seconds(10));
	EXPECT_TRUE(connected) << "Host never accepted the client connection";
}

TEST_F(NetworkLoopbackTest, ClientToHostPacketRoundTrip)
{
	m_Host.HostGame(m_Port, 4);
	ASSERT_TRUE(m_Host.IsEnabled());

	m_Client.ConnectTo("127.0.0.1", m_Port);
	ASSERT_TRUE(m_Client.IsEnabled());
	ASSERT_TRUE(PumpUntil([this] { return m_Host.GetClientCount() == 1; }, std::chrono::seconds(10)));

	InputStateMessage received{};
	bool gotPacket = false;
	m_Host.SetPacketCallback([&](int clientIndex, MessageType type, const uint8_t* data, size_t len) {
		if (type == MessageType_InputState && clientIndex >= 0)
		{
			ByteReader r(data, len);
			if (received.Decode(r))
			{
				gotPacket = true;
			}
		}
	});

	InputStateMessage sent;
	sent.Tick = 4242;
	sent.MoveX = 1.5f;
	sent.MoveZ = -2.5f;
	sent.ActionFlags = InputAction_Jump | InputAction_Sprint;

	ByteWriter w;
	sent.Encode(w);
	m_Client.SendToServer(MessageType_InputState, w.Data().data(), w.Data().size(), true);

	ASSERT_TRUE(PumpUntil([&] { return gotPacket; }, std::chrono::seconds(10)))
		<< "Host never received the client's input message";

	EXPECT_EQ(received.Tick, 4242u);
	EXPECT_FLOAT_EQ(received.MoveX, 1.5f);
	EXPECT_FLOAT_EQ(received.MoveZ, -2.5f);
	EXPECT_EQ(received.ActionFlags, InputAction_Jump | InputAction_Sprint);

	m_Host.ClearPacketCallback();
}

TEST_F(NetworkLoopbackTest, HostBroadcastReachesClient)
{
	m_Host.HostGame(m_Port, 4);
	ASSERT_TRUE(m_Host.IsEnabled());

	m_Client.ConnectTo("127.0.0.1", m_Port);
	ASSERT_TRUE(m_Client.IsEnabled());
	ASSERT_TRUE(PumpUntil([this] { return m_Host.GetClientCount() == 1; }, std::chrono::seconds(10)));

	std::string receivedPath;
	m_Client.SetPacketCallback([&](int clientIndex, MessageType type, const uint8_t* data, size_t len) {
		if (type == MessageType_SceneChange)
		{
			SceneChangeMessage msg;
			ByteReader r(data, len);
			if (msg.Decode(r))
			{
				receivedPath = msg.ScenePath;
			}
		}
	});

	m_Host.BroadcastSceneChange("assets/scenes/level_01.chscene");

	ASSERT_TRUE(PumpUntil([&] { return !receivedPath.empty(); }, std::chrono::seconds(10)))
		<< "Client never received the broadcast scene change";
	EXPECT_EQ(receivedPath, "assets/scenes/level_01.chscene");

	m_Client.ClearPacketCallback();
}

TEST_F(NetworkLoopbackTest, HostAppearsInItsOwnPlayerList)
{
	m_Host.SetLocalPlayerInfo("Alice", 3);
	m_Host.HostGame(m_Port, 4);
	ASSERT_TRUE(m_Host.IsEnabled());

	const auto& players = m_Host.GetPlayerList();
	ASSERT_EQ(players.size(), 1u) << "Host is missing from its own lobby";
	EXPECT_EQ(players[0].NetworkID, 1u);
	EXPECT_EQ(players[0].IsHost, 1);
	EXPECT_EQ(players[0].Name, "Alice");
	EXPECT_EQ(players[0].SkinIndex, 3);
	EXPECT_EQ(m_Host.GetLocalNetworkID(), 1u);
}

TEST_F(NetworkLoopbackTest, PeerGetsIdentityDistinctFromHost)
{
	m_Host.HostGame(m_Port, 4);
	ASSERT_TRUE(m_Host.IsEnabled());

	m_Client.ConnectTo("127.0.0.1", m_Port);
	ASSERT_TRUE(m_Client.IsEnabled());
	ASSERT_TRUE(PumpUntil([this] { return m_Host.GetClientCount() == 1; }, std::chrono::seconds(10)));

	const auto clients = m_Host.GetClients();
	ASSERT_EQ(clients.size(), 1u);
	const int peer = clients.front();
	const uint64_t peerID = m_Host.GetNetworkIDForConnection(peer);
	EXPECT_NE(peerID, 0u) << "Peer identity was not bound at accept time";
	EXPECT_NE(peerID, 1u) << "NetworkID 1 is reserved for the host";

	const auto& players = m_Host.GetPlayerList();
	ASSERT_EQ(players.size(), 2u);
	int hostFlags = 0;
	for (const auto& p : players)
	{
		hostFlags += p.IsHost ? 1 : 0;
	}
	EXPECT_EQ(hostFlags, 1);

	EXPECT_EQ(m_Host.GetNetworkIDForConnection(kInvalidPeerHandle), 0u);
}
#endif // CH_CI

TEST(NetworkMessageTest, EntitySpawnFields)
{
	EntitySpawnMessage msg;
	msg.NetworkID = 0x00000000DEADBEEFull;
	std::strncpy(msg.PrefabPath, "prefab/player.chprefab", sizeof(msg.PrefabPath) - 1);
	msg.PrefabPath[sizeof(msg.PrefabPath) - 1] = '\0';

	ByteWriter w;
	msg.Encode(w);

	EntitySpawnMessage decoded;
	ByteReader r(w.Data().data(), w.Data().size());
	ASSERT_TRUE(decoded.Decode(r));

	EXPECT_EQ(decoded.NetworkID, 0x00000000DEADBEEFull);
	EXPECT_STREQ(decoded.PrefabPath, "prefab/player.chprefab");
}

TEST(NetworkMessageTest, EntityDestroyAndPlayerAssignFields)
{
	EntityDestroyMessage destroy;
	destroy.NetworkID = 77;

	ByteWriter w1;
	destroy.Encode(w1);
	EntityDestroyMessage dec1;
	ByteReader r1(w1.Data().data(), w1.Data().size());
	ASSERT_TRUE(dec1.Decode(r1));
	EXPECT_EQ(dec1.NetworkID, 77u);

	PlayerAssignMessage assign;
	assign.NetworkID = 5;

	ByteWriter w2;
	assign.Encode(w2);
	PlayerAssignMessage dec2;
	ByteReader r2(w2.Data().data(), w2.Data().size());
	ASSERT_TRUE(dec2.Decode(r2));
	EXPECT_EQ(dec2.NetworkID, 5u);
}

TEST(NetworkChannelTest, ChannelEnumValuesAndReliability)
{
	EXPECT_EQ(static_cast<int>(ePacketChannel::SYSTEM), 0);
	EXPECT_EQ(static_cast<int>(ePacketChannel::SYNC), 1);
	EXPECT_EQ(static_cast<int>(ePacketChannel::EVENT), 2);
	EXPECT_EQ(static_cast<int>(ePacketChannel::SCRIPT), 3);
	EXPECT_EQ(static_cast<int>(ePacketChannel::COUNT), 4);

	EXPECT_TRUE(IsChannelReliable(ePacketChannel::SYSTEM));
	EXPECT_FALSE(IsChannelReliable(ePacketChannel::SYNC));
	EXPECT_TRUE(IsChannelReliable(ePacketChannel::EVENT));
	EXPECT_TRUE(IsChannelReliable(ePacketChannel::SCRIPT));

	EXPECT_EQ(GetChannelForMessageType(MessageType_WorldState), ePacketChannel::SYNC);
	EXPECT_EQ(GetChannelForMessageType(MessageType_InputState), ePacketChannel::SYNC);
	EXPECT_EQ(GetChannelForMessageType(MessageType_ChatMessage), ePacketChannel::EVENT);
	EXPECT_EQ(GetChannelForMessageType(MessageType_EntitySpawn), ePacketChannel::SYSTEM);
	EXPECT_EQ(GetChannelForMessageType(MessageType_EntityDestroy), ePacketChannel::SYSTEM);
	EXPECT_EQ(GetChannelForMessageType(MessageType_SceneLoaded), ePacketChannel::SYSTEM);
	EXPECT_EQ(GetChannelForMessageType(MessageType_SceneChange), ePacketChannel::SYSTEM);
	EXPECT_EQ(GetChannelForMessageType(MessageType_PlayerAssign), ePacketChannel::SYSTEM);
	EXPECT_EQ(GetChannelForMessageType(MessageType_PlayerInfo), ePacketChannel::SYSTEM);
	EXPECT_EQ(GetChannelForMessageType(MessageType_PlayerList), ePacketChannel::SYSTEM);
	EXPECT_EQ(GetChannelForMessageType(MessageType_Heartbeat), ePacketChannel::SYSTEM);
}

#ifndef CH_CI
TEST_F(NetworkLoopbackTest, VirtualDriverMultiChannelTransmission)
{
	m_Host.HostGame(m_Port, 4);
	ASSERT_TRUE(m_Host.IsEnabled());

	m_Client.ConnectTo("127.0.0.1", m_Port);
	ASSERT_TRUE(m_Client.IsEnabled());
	ASSERT_TRUE(PumpUntil([this] { return m_Host.GetClientCount() == 1; }, std::chrono::seconds(10)));

	INetworkDriver* hostDriver = m_Host.GetSession().GetDriver();
	INetworkDriver* clientDriver = m_Client.GetSession().GetDriver();
	ASSERT_NE(hostDriver, nullptr);
	ASSERT_NE(clientDriver, nullptr);

	EXPECT_TRUE(hostDriver->IsConnected());
	EXPECT_TRUE(clientDriver->IsConnected());
	EXPECT_EQ(hostDriver->GetRole(), Role::Host);
	EXPECT_EQ(clientDriver->GetRole(), Role::Client);

	// Test sending on SCRIPT channel
	bool scriptMsgReceived = false;
	m_Host.SetPacketCallback([&scriptMsgReceived](int clientIndex, MessageType type, const uint8_t* data, size_t len) {
		if (type == MessageType_ChatMessage)
		{
			scriptMsgReceived = true;
		}
	});

	const char testPayload[] = "ScriptRPCData";
	m_Client.SendToServer(ePacketChannel::SCRIPT, MessageType_ChatMessage, testPayload, sizeof(testPayload), true);

	const bool received = PumpUntil([&scriptMsgReceived] { return scriptMsgReceived; }, std::chrono::seconds(5));
	EXPECT_TRUE(received) << "Host never received packet sent over SCRIPT channel";
}
#endif // CH_CI
