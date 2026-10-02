// network_tests.cpp
// Consolidated scenario-based tests for Networking (NetPacket Codecs, Channel Mapping, and Loopback Session).
// Follows Arrange-Act-Assert (AAA) pattern without fixtures.

#include "engine/networking/net_packet.h"
#include "engine/networking/network_service.h"
#include "gtest/gtest.h"

#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

using namespace Chained;

// ============================================================================
// 1. POSITIVE SCENARIO (Happy Path: Channel Mapping and Codec Round-Trips)
// ============================================================================
TEST(NetworkModule, Positive_ChannelMappingAndMessageRoundTrips)
{
	// Arrange & Assert (Step 1: Verify channel mappings and reliability flags)
	EXPECT_TRUE(IsChannelReliable(ePacketChannel::SYSTEM));
	EXPECT_FALSE(IsChannelReliable(ePacketChannel::SYNC));
	EXPECT_TRUE(IsChannelReliable(ePacketChannel::EVENT));
	EXPECT_TRUE(IsChannelReliable(ePacketChannel::SCRIPT));

	EXPECT_EQ(GetChannelForMessageType(MessageType_WorldState), ePacketChannel::SYNC);
	EXPECT_EQ(GetChannelForMessageType(MessageType_InputState), ePacketChannel::SYNC);
	EXPECT_EQ(GetChannelForMessageType(MessageType_ChatMessage), ePacketChannel::EVENT);
	EXPECT_EQ(GetChannelForMessageType(MessageType_EntitySpawn), ePacketChannel::SYSTEM);
	EXPECT_EQ(GetChannelForMessageType(MessageType_SceneChange), ePacketChannel::SYSTEM);

	// Act & Assert (Step 2: WorldState round-trip)
	{
		WorldStateMessage sent;
		sent.Tick = 1000;
		sent.NetworkID = 0xCAFEBABEull;
		sent.Position[0] = 12.5f;
		sent.Position[1] = 1.0f;
		sent.Position[2] = -4.5f;
		sent.Rotation[0] = 1.0f;
		sent.Rotation[1] = 0.0f;
		sent.Rotation[2] = 0.0f;
		sent.Rotation[3] = 0.0f;
		sent.Velocity[0] = 5.0f;
		sent.Velocity[1] = 0.0f;
		sent.Velocity[2] = -1.0f;
		sent.IsGrounded = 1;
		sent.ActionFlags = InputAction_Jump | InputAction_Sprint;

		ByteWriter w;
		sent.Encode(w);

		WorldStateMessage received;
		ByteReader r(w.Data().data(), w.Data().size());
		ASSERT_TRUE(received.Decode(r));
		EXPECT_TRUE(r.Eof());

		EXPECT_EQ(received.Tick, 1000u);
		EXPECT_EQ(received.NetworkID, 0xCAFEBABEull);
		EXPECT_FLOAT_EQ(received.Position[0], 12.5f);
		EXPECT_FLOAT_EQ(received.Position[2], -4.5f);
		EXPECT_EQ(received.IsGrounded, 1);
		EXPECT_EQ(received.ActionFlags, InputAction_Jump | InputAction_Sprint);
	}

	// Act & Assert (Step 3: EntitySpawn and EntityDestroy round-trip)
	{
		EntitySpawnMessage spawn;
		spawn.NetworkID = 42;
		std::strncpy(spawn.PrefabPath, "prefabs/hero.chprefab", sizeof(spawn.PrefabPath) - 1);

		ByteWriter w;
		spawn.Encode(w);

		EntitySpawnMessage decodedSpawn;
		ByteReader r(w.Data().data(), w.Data().size());
		ASSERT_TRUE(decodedSpawn.Decode(r));
		EXPECT_EQ(decodedSpawn.NetworkID, 42u);
		EXPECT_STREQ(decodedSpawn.PrefabPath, "prefabs/hero.chprefab");

		EntityDestroyMessage destroy;
		destroy.NetworkID = 42;
		ByteWriter w2;
		destroy.Encode(w2);

		EntityDestroyMessage decodedDestroy;
		ByteReader r2(w2.Data().data(), w2.Data().size());
		ASSERT_TRUE(decodedDestroy.Decode(r2));
		EXPECT_EQ(decodedDestroy.NetworkID, 42u);
	}

	// Act & Assert (Step 4: PlayerList round-trip)
	{
		PlayerListMessage list;
		list.Count = 2;
		list.Entries[0] = {1, "HostPlayer", 0, 1, 0};
		list.Entries[1] = {2, "ClientPlayer", 3, 0, 35};
		std::strncpy(list.Entries[0].Name, "HostPlayer", sizeof(list.Entries[0].Name) - 1);
		std::strncpy(list.Entries[1].Name, "ClientPlayer", sizeof(list.Entries[1].Name) - 1);

		ByteWriter w;
		list.Encode(w);

		PlayerListMessage decodedList;
		ByteReader r(w.Data().data(), w.Data().size());
		ASSERT_TRUE(decodedList.Decode(r));
		ASSERT_EQ(decodedList.Count, 2);
		EXPECT_EQ(decodedList.Entries[0].NetworkID, 1u);
		EXPECT_STREQ(decodedList.Entries[0].Name, "HostPlayer");
		EXPECT_EQ(decodedList.Entries[1].Ping, 35u);
	}
}

// ============================================================================
// 2. NEGATIVE SCENARIO (Buffer Underflow, Corrupted Packets, Overflow Safety)
// ============================================================================
TEST(NetworkModule, Negative_BufferUnderflowAndCorruptedPackets)
{
	// Act & Assert (Step 1: Reading from empty buffer returns false)
	ByteReader emptyReader(nullptr, 0);
	uint8_t u8Val = 0;
	uint32_t u32Val = 0;
	float floatVal = 0.0f;
	char strBuf[32] = {};

	EXPECT_FALSE(emptyReader.ReadU8(u8Val));
	EXPECT_FALSE(emptyReader.ReadU32(u32Val));
	EXPECT_FALSE(emptyReader.ReadFloat(floatVal));
	EXPECT_FALSE(emptyReader.ReadString(strBuf, sizeof(strBuf)));
	EXPECT_TRUE(emptyReader.Eof());

	// Act & Assert (Step 2: Buffer underflow returns false during decode)
	InputStateMessage inputMsg;
	inputMsg.Tick = 999;
	ByteWriter w;
	inputMsg.Encode(w);

	ByteReader truncatedReader(w.Data().data(), w.Data().size() - 4);
	InputStateMessage decodedInput;
	EXPECT_FALSE(decodedInput.Decode(truncatedReader));

	// Act & Assert (Step 3: String claims 100 bytes but only 2 follow)
	uint8_t corruptStringData[] = {100, 'a', 'b'};
	ByteReader corruptReader(corruptStringData, sizeof(corruptStringData));
	EXPECT_FALSE(corruptReader.ReadString(strBuf, sizeof(strBuf)));

	// Act & Assert (Step 4: String length exceeds destination buffer)
	uint8_t bigStringData[50];
	bigStringData[0] = 40;
	std::memset(bigStringData + 1, 'X', 40);
	ByteReader overflowReader(bigStringData, sizeof(bigStringData));
	char smallDst[16] = {};
	EXPECT_FALSE(overflowReader.ReadString(smallDst, sizeof(smallDst)));
}

// ============================================================================
// 3. LOCAL SESSION SCENARIO (Loopback Host-Client Session — skipped in CI)
// ============================================================================
#ifndef CH_CI
TEST(NetworkModule, Positive_LoopbackHostClientSession)
{
	// Arrange
	static uint16_t s_Port = 27650;
	uint16_t testPort = s_Port++;

	Network host;
	Network client;
	host.SetTestMode(true);
	client.SetTestMode(true);
	host.Initialize();
	client.Initialize();

	ASSERT_TRUE(host.IsEnabled());
	ASSERT_TRUE(client.IsEnabled());

	// Act (Step 1: Host opens server, client connects)
	host.HostGame(testPort, 4);
	EXPECT_EQ(host.GetRole(), Role::Host);

	client.ConnectTo("127.0.0.1", testPort);
	EXPECT_EQ(client.GetRole(), Role::Client);

	auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	bool connected = false;
	while (std::chrono::steady_clock::now() < deadline)
	{
		host.Update(1.0f / 60.0f);
		client.Update(1.0f / 60.0f);
		if (host.GetClientCount() == 1)
		{
			connected = true;
			break;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	// Assert
	EXPECT_TRUE(connected) << "Host never accepted loopback client connection";

	// Act (Step 2: Client sends packet to host)
	bool receivedMsg = false;
	host.SetPacketCallback([&](int clientIndex, MessageType type, const uint8_t* data, size_t len) {
		if (type == MessageType_ChatMessage)
		{
			receivedMsg = true;
		}
	});

	const char chat[] = "Hello from client";
	client.SendToServer(MessageType_ChatMessage, reinterpret_cast<const uint8_t*>(chat), sizeof(chat), true);

	deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
	while (std::chrono::steady_clock::now() < deadline && !receivedMsg)
	{
		host.Update(1.0f / 60.0f);
		client.Update(1.0f / 60.0f);
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}

	// Assert
	EXPECT_TRUE(receivedMsg);

	host.ClearPacketCallback();
	client.Shutdown();
	host.Shutdown();
}
#endif // CH_CI
