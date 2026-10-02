// byte_codec_tests.cpp
// Unit tests for ByteWriter / ByteReader and all Message encode/decode round-trips.
// Zero engine dependencies — only includes net_packet.h.

#include "engine/networking/net_packet.h"
#include "gtest/gtest.h"

#include <cstring>
#include <limits>

using namespace Chained;

// ── ByteWriter / ByteReader primitives ──────────────────────────────────────

TEST(ByteCodecTest, U8RoundTrip)
{
	ByteWriter w;
	w.WriteU8(0x00);
	w.WriteU8(0xAB);
	w.WriteU8(0xFF);

	ASSERT_EQ(w.Data().size(), 3u);

	ByteReader r(w.Data().data(), w.Data().size());
	uint8_t a, b, c;
	EXPECT_TRUE(r.ReadU8(a));
	EXPECT_TRUE(r.ReadU8(b));
	EXPECT_TRUE(r.ReadU8(c));
	EXPECT_EQ(a, 0x00);
	EXPECT_EQ(b, 0xAB);
	EXPECT_EQ(c, 0xFF);
	EXPECT_TRUE(r.Eof());
}

TEST(ByteCodecTest, U16RoundTrip)
{
	const uint16_t values[] = {0, 1, 255, 256, 0x1234, 0xFFFF};
	for (uint16_t v : values)
	{
		ByteWriter w;
		w.WriteU16(v);
		ASSERT_EQ(w.Data().size(), 2u);

		ByteReader r(w.Data().data(), w.Data().size());
		uint16_t got;
		EXPECT_TRUE(r.ReadU16(got));
		EXPECT_EQ(got, v) << "Failed for value " << v;
	}
}

TEST(ByteCodecTest, U16LittleEndian)
{
	ByteWriter w;
	w.WriteU16(0x1234);
	// Little-endian: low byte first
	EXPECT_EQ(w.Data()[0], 0x34);
	EXPECT_EQ(w.Data()[1], 0x12);
}

TEST(ByteCodecTest, U32RoundTrip)
{
	const uint32_t values[] = {0, 1, 0x12345678u, 0xDEADBEEFu, 0xFFFFFFFFu};
	for (uint32_t v : values)
	{
		ByteWriter w;
		w.WriteU32(v);
		ASSERT_EQ(w.Data().size(), 4u);

		ByteReader r(w.Data().data(), w.Data().size());
		uint32_t got;
		EXPECT_TRUE(r.ReadU32(got));
		EXPECT_EQ(got, v) << "Failed for value " << v;
	}
}

TEST(ByteCodecTest, U64RoundTrip)
{
	const uint64_t values[] = {
		0ull, 1ull, 0x00000000DEADBEEFull, 0xCAFEBABEDEADBEEFull, std::numeric_limits<uint64_t>::max(),
	};
	for (uint64_t v : values)
	{
		ByteWriter w;
		w.WriteU64(v);
		ASSERT_EQ(w.Data().size(), 8u);

		ByteReader r(w.Data().data(), w.Data().size());
		uint64_t got;
		EXPECT_TRUE(r.ReadU64(got));
		EXPECT_EQ(got, v) << "Failed for value " << v;
	}
}

TEST(ByteCodecTest, FloatRoundTrip)
{
	const float values[] = {0.0f, 1.0f, -1.0f, 3.14159f, -999.5f, 1.5f, -0.0f};
	for (float v : values)
	{
		ByteWriter w;
		w.WriteFloat(v);
		ASSERT_EQ(w.Data().size(), 4u);

		ByteReader r(w.Data().data(), w.Data().size());
		float got;
		EXPECT_TRUE(r.ReadFloat(got));
		EXPECT_FLOAT_EQ(got, v) << "Failed for value " << v;
	}
}

TEST(ByteCodecTest, FloatSpecialValues)
{
	ByteWriter w;
	w.WriteFloat(std::numeric_limits<float>::infinity());
	w.WriteFloat(-std::numeric_limits<float>::infinity());

	ByteReader r(w.Data().data(), w.Data().size());
	float a, b;
	EXPECT_TRUE(r.ReadFloat(a));
	EXPECT_TRUE(r.ReadFloat(b));
	EXPECT_TRUE(std::isinf(a) && a > 0.0f);
	EXPECT_TRUE(std::isinf(b) && b < 0.0f);
}

TEST(ByteCodecTest, StringRoundTrip)
{
	const char* str = "hello_world";
	ByteWriter w;
	w.WriteString(str, 32);

	ByteReader r(w.Data().data(), w.Data().size());
	char buf[64] = {};
	EXPECT_TRUE(r.ReadString(buf, sizeof(buf)));
	EXPECT_STREQ(buf, str);
	EXPECT_TRUE(r.Eof());
}

TEST(ByteCodecTest, EmptyStringRoundTrip)
{
	ByteWriter w;
	w.WriteString("", 32);

	ByteReader r(w.Data().data(), w.Data().size());
	char buf[64] = {'X'}; // pre-fill to detect no-overwrite
	EXPECT_TRUE(r.ReadString(buf, sizeof(buf)));
	EXPECT_EQ(buf[0], '\0');
	EXPECT_TRUE(r.Eof());
}

TEST(ByteCodecTest, NullStringWritesEmptyLength)
{
	ByteWriter w;
	w.WriteString(nullptr, 32);

	ByteReader r(w.Data().data(), w.Data().size());
	char buf[64] = {};
	EXPECT_TRUE(r.ReadString(buf, sizeof(buf)));
	EXPECT_EQ(buf[0], '\0');
}

TEST(ByteCodecTest, StringTruncatedAtMaxLen)
{
	// WriteString(str, maxLen) should cap at maxLen chars
	const char* longStr = "abcdefghijklmnop"; // 16 chars
	ByteWriter w;
	w.WriteString(longStr, 8); // maxLen=8 → writes only 8 chars

	ByteReader r(w.Data().data(), w.Data().size());
	char buf[64] = {};
	EXPECT_TRUE(r.ReadString(buf, sizeof(buf)));
	EXPECT_EQ(std::strlen(buf), 8u);
}

TEST(ByteCodecTest, WriteBytesRoundTrip)
{
	const uint8_t src[] = {0x01, 0x02, 0xAA, 0xFF};
	ByteWriter w;
	w.WriteBytes(src, sizeof(src));
	ASSERT_EQ(w.Data().size(), 4u);

	ByteReader r(w.Data().data(), w.Data().size());
	uint8_t dst[4] = {};
	EXPECT_TRUE(r.ReadBytes(dst, sizeof(dst)));
	EXPECT_EQ(std::memcmp(src, dst, sizeof(src)), 0);
}

// ── ByteReader underflow / truncation ───────────────────────────────────────

TEST(ByteCodecTest, ReadU8UnderflowReturnsFalse)
{
	ByteWriter w;
	w.WriteU8(42);

	ByteReader r(w.Data().data(), w.Data().size());
	uint8_t v;
	r.ReadU8(v);			   // consume the only byte
	EXPECT_FALSE(r.ReadU8(v)); // underflow
}

TEST(ByteCodecTest, ReadU16UnderflowReturnsFalse)
{
	uint8_t oneByte = 0x01;
	ByteReader r(&oneByte, 1);
	uint16_t v;
	EXPECT_FALSE(r.ReadU16(v));
}

TEST(ByteCodecTest, ReadU32UnderflowReturnsFalse)
{
	uint8_t threeBytes[3] = {1, 2, 3};
	ByteReader r(threeBytes, sizeof(threeBytes));
	uint32_t v;
	EXPECT_FALSE(r.ReadU32(v));
}

TEST(ByteCodecTest, ReadU64UnderflowReturnsFalse)
{
	uint8_t sevenBytes[7] = {};
	ByteReader r(sevenBytes, sizeof(sevenBytes));
	uint64_t v;
	EXPECT_FALSE(r.ReadU64(v));
}

TEST(ByteCodecTest, ReadFloatUnderflowReturnsFalse)
{
	uint8_t threeBytes[3] = {};
	ByteReader r(threeBytes, sizeof(threeBytes));
	float v;
	EXPECT_FALSE(r.ReadFloat(v));
}

TEST(ByteCodecTest, ReadStringUnderflowReturnsFalse)
{
	// Write length byte = 10 but only 3 content bytes follow
	uint8_t data[] = {10, 'a', 'b', 'c'};
	ByteReader r(data, sizeof(data));
	char buf[64] = {};
	EXPECT_FALSE(r.ReadString(buf, sizeof(buf)));
}

TEST(ByteCodecTest, ReadStringExceedsMaxLenReturnsFalse)
{
	// length byte = 200, maxLen passed to ReadString = 32 → reject
	uint8_t data[202];
	data[0] = 200;
	std::memset(data + 1, 'x', 200);
	data[201] = '\0';

	ByteReader r(data, sizeof(data));
	char buf[32] = {};
	EXPECT_FALSE(r.ReadString(buf, 32));
}

TEST(ByteCodecTest, EofAndRemainingTracking)
{
	ByteWriter w;
	w.WriteU8(1);
	w.WriteU8(2);

	ByteReader r(w.Data().data(), w.Data().size());
	EXPECT_FALSE(r.Eof());
	EXPECT_EQ(r.Remaining(), 2u);

	uint8_t v;
	r.ReadU8(v);
	EXPECT_FALSE(r.Eof());
	EXPECT_EQ(r.Remaining(), 1u);

	r.ReadU8(v);
	EXPECT_TRUE(r.Eof());
	EXPECT_EQ(r.Remaining(), 0u);
}

TEST(ByteCodecTest, EmptyBufferIsEofImmediately)
{
	ByteReader r(nullptr, 0);
	EXPECT_TRUE(r.Eof());
	EXPECT_EQ(r.Remaining(), 0u);
	uint8_t v;
	EXPECT_FALSE(r.ReadU8(v));
}

TEST(ByteCodecTest, ClearResetsBuffer)
{
	ByteWriter w;
	w.WriteU32(0xDEADBEEF);
	EXPECT_EQ(w.Data().size(), 4u);
	w.Clear();
	EXPECT_TRUE(w.Data().empty());
}

// ── Message encode / decode round-trips ─────────────────────────────────────

TEST(MessageCodecTest, InputStateRoundTrip)
{
	InputStateMessage sent;
	sent.Tick = 77777;
	sent.MoveX = 1.5f;
	sent.MoveZ = -2.25f;
	sent.ActionFlags = InputAction_Jump | InputAction_Sprint;
	sent.MouseX = 0.123f;
	sent.MouseY = -0.456f;
	sent.DeltaTime = 0.016f;

	ByteWriter w;
	sent.Encode(w);

	InputStateMessage got;
	ByteReader r(w.Data().data(), w.Data().size());
	ASSERT_TRUE(got.Decode(r));
	EXPECT_TRUE(r.Eof());

	EXPECT_EQ(got.Tick, sent.Tick);
	EXPECT_FLOAT_EQ(got.MoveX, sent.MoveX);
	EXPECT_FLOAT_EQ(got.MoveZ, sent.MoveZ);
	EXPECT_EQ(got.ActionFlags, sent.ActionFlags);
	EXPECT_FLOAT_EQ(got.MouseX, sent.MouseX);
	EXPECT_FLOAT_EQ(got.MouseY, sent.MouseY);
	EXPECT_FLOAT_EQ(got.DeltaTime, sent.DeltaTime);
}

TEST(MessageCodecTest, InputStateDecodeFailsOnTruncated)
{
	InputStateMessage sent;
	sent.Tick = 1;
	ByteWriter w;
	sent.Encode(w);

	// Give reader 3 bytes less than needed
	size_t truncated = w.Data().size() - 3;
	ByteReader r(w.Data().data(), truncated);
	InputStateMessage got;
	EXPECT_FALSE(got.Decode(r));
}

TEST(MessageCodecTest, WorldStateRoundTrip)
{
	WorldStateMessage sent;
	sent.Tick = 9999;
	sent.NetworkID = 0xCAFEBABEull;
	sent.Position[0] = 10.5f;
	sent.Position[1] = 0.0f;
	sent.Position[2] = -3.2f;
	sent.Rotation[0] = 1.0f;
	sent.Rotation[1] = 0.0f;
	sent.Rotation[2] = 0.0f;
	sent.Rotation[3] = 0.0f;
	sent.Velocity[0] = 2.0f;
	sent.Velocity[1] = 0.0f;
	sent.Velocity[2] = 0.5f;
	sent.IsGrounded = 1;
	sent.ActionFlags = InputAction_Jump;

	ByteWriter w;
	sent.Encode(w);

	WorldStateMessage got;
	ByteReader r(w.Data().data(), w.Data().size());
	ASSERT_TRUE(got.Decode(r));
	EXPECT_TRUE(r.Eof());

	EXPECT_EQ(got.Tick, sent.Tick);
	EXPECT_EQ(got.NetworkID, sent.NetworkID);
	EXPECT_FLOAT_EQ(got.Position[0], sent.Position[0]);
	EXPECT_FLOAT_EQ(got.Position[2], sent.Position[2]);
	EXPECT_FLOAT_EQ(got.Velocity[0], sent.Velocity[0]);
	EXPECT_EQ(got.IsGrounded, sent.IsGrounded);
	EXPECT_EQ(got.ActionFlags, sent.ActionFlags);
}

TEST(MessageCodecTest, EntitySpawnRoundTrip)
{
	EntitySpawnMessage sent;
	sent.NetworkID = 0x00000000DEADBEEFull;
	std::strncpy(sent.PrefabPath, "prefab/player.chprefab", sizeof(sent.PrefabPath) - 1);

	ByteWriter w;
	sent.Encode(w);

	EntitySpawnMessage got;
	ByteReader r(w.Data().data(), w.Data().size());
	ASSERT_TRUE(got.Decode(r));

	EXPECT_EQ(got.NetworkID, sent.NetworkID);
	EXPECT_STREQ(got.PrefabPath, sent.PrefabPath);
}

TEST(MessageCodecTest, EntitySpawnEmptyPath)
{
	EntitySpawnMessage sent;
	sent.NetworkID = 42;
	sent.PrefabPath[0] = '\0';

	ByteWriter w;
	sent.Encode(w);

	EntitySpawnMessage got;
	ByteReader r(w.Data().data(), w.Data().size());
	ASSERT_TRUE(got.Decode(r));
	EXPECT_EQ(got.NetworkID, 42u);
	EXPECT_EQ(got.PrefabPath[0], '\0');
}

TEST(MessageCodecTest, EntityDestroyRoundTrip)
{
	EntityDestroyMessage sent;
	sent.NetworkID = 77;

	ByteWriter w;
	sent.Encode(w);

	EntityDestroyMessage got;
	ByteReader r(w.Data().data(), w.Data().size());
	ASSERT_TRUE(got.Decode(r));
	EXPECT_EQ(got.NetworkID, 77u);
}

TEST(MessageCodecTest, SceneChangeRoundTrip)
{
	SceneChangeMessage sent;
	std::strncpy(sent.ScenePath, "assets/scenes/level_01.chscene", sizeof(sent.ScenePath) - 1);

	ByteWriter w;
	sent.Encode(w);

	SceneChangeMessage got;
	ByteReader r(w.Data().data(), w.Data().size());
	ASSERT_TRUE(got.Decode(r));
	EXPECT_STREQ(got.ScenePath, sent.ScenePath);
}

TEST(MessageCodecTest, PlayerAssignRoundTrip)
{
	PlayerAssignMessage sent;
	sent.NetworkID = 5;

	ByteWriter w;
	sent.Encode(w);

	PlayerAssignMessage got;
	ByteReader r(w.Data().data(), w.Data().size());
	ASSERT_TRUE(got.Decode(r));
	EXPECT_EQ(got.NetworkID, 5u);
}

TEST(MessageCodecTest, PlayerInfoRoundTrip)
{
	PlayerInfoMessage sent;
	std::strncpy(sent.Name, "Alice", sizeof(sent.Name) - 1);
	sent.SkinIndex = 3;

	ByteWriter w;
	sent.Encode(w);

	PlayerInfoMessage got;
	ByteReader r(w.Data().data(), w.Data().size());
	ASSERT_TRUE(got.Decode(r));
	EXPECT_STREQ(got.Name, "Alice");
	EXPECT_EQ(got.SkinIndex, 3);
}

TEST(MessageCodecTest, PlayerListRoundTrip)
{
	PlayerListMessage sent;
	sent.Count = 3;
	sent.Entries[0] = {1, "Alice", 0, 1, 10};
	sent.Entries[1] = {2, "Bob", 2, 0, 50};
	sent.Entries[2] = {3, "Carol", 1, 0, 120};
	// Manually fill Name fields (struct init doesn't null-fill char arrays beyond init)
	std::strncpy(sent.Entries[0].Name, "Alice", sizeof(sent.Entries[0].Name) - 1);
	std::strncpy(sent.Entries[1].Name, "Bob", sizeof(sent.Entries[1].Name) - 1);
	std::strncpy(sent.Entries[2].Name, "Carol", sizeof(sent.Entries[2].Name) - 1);

	ByteWriter w;
	sent.Encode(w);

	PlayerListMessage got;
	ByteReader r(w.Data().data(), w.Data().size());
	ASSERT_TRUE(got.Decode(r));

	ASSERT_EQ(got.Count, 3);
	EXPECT_EQ(got.Entries[0].NetworkID, 1u);
	EXPECT_STREQ(got.Entries[0].Name, "Alice");
	EXPECT_EQ(got.Entries[0].IsHost, 1);
	EXPECT_EQ(got.Entries[0].Ping, 10u);

	EXPECT_EQ(got.Entries[1].NetworkID, 2u);
	EXPECT_STREQ(got.Entries[1].Name, "Bob");
	EXPECT_EQ(got.Entries[1].SkinIndex, 2);

	EXPECT_EQ(got.Entries[2].NetworkID, 3u);
	EXPECT_STREQ(got.Entries[2].Name, "Carol");
	EXPECT_EQ(got.Entries[2].Ping, 120u);
}

TEST(MessageCodecTest, PlayerListEmptyRoundTrip)
{
	PlayerListMessage sent;
	sent.Count = 0;

	ByteWriter w;
	sent.Encode(w);

	PlayerListMessage got;
	ByteReader r(w.Data().data(), w.Data().size());
	ASSERT_TRUE(got.Decode(r));
	EXPECT_EQ(got.Count, 0);
}

TEST(MessageCodecTest, ChatMessageRoundTrip)
{
	ChatMessageMessage sent;
	sent.SenderNetworkID = 0xBEEF;
	std::strncpy(sent.SenderName, "Bob", sizeof(sent.SenderName) - 1);
	std::strncpy(sent.Message, "Hello, world!", sizeof(sent.Message) - 1);

	ByteWriter w;
	sent.Encode(w);

	ChatMessageMessage got;
	ByteReader r(w.Data().data(), w.Data().size());
	ASSERT_TRUE(got.Decode(r));
	EXPECT_EQ(got.SenderNetworkID, 0xBEEFull);
	EXPECT_STREQ(got.SenderName, "Bob");
	EXPECT_STREQ(got.Message, "Hello, world!");
}

TEST(MessageCodecTest, SceneLoadedRoundTrip)
{
	SceneLoadedMessage sent;
	std::strncpy(sent.ScenePath, "assets/scenes/lobby.chscene", sizeof(sent.ScenePath) - 1);

	ByteWriter w;
	sent.Encode(w);

	SceneLoadedMessage got;
	ByteReader r(w.Data().data(), w.Data().size());
	ASSERT_TRUE(got.Decode(r));
	EXPECT_STREQ(got.ScenePath, sent.ScenePath);
}

// ── InputAction flag composition ─────────────────────────────────────────────

TEST(InputActionTest, FlagBitComposition)
{
	EXPECT_EQ(InputAction_None, 0);
	EXPECT_EQ(InputAction_Jump, 1 << 0);
	EXPECT_EQ(InputAction_Sprint, 1 << 1);
	EXPECT_EQ(InputAction_Interact, 1 << 2);

	uint8_t combo = InputAction_Jump | InputAction_Sprint;
	EXPECT_NE(combo & InputAction_Jump, 0);
	EXPECT_NE(combo & InputAction_Sprint, 0);
	EXPECT_EQ(combo & InputAction_Interact, 0);
}

TEST(InputActionTest, ActionFlagsPreservedInRoundTrip)
{
	InputStateMessage sent;
	sent.ActionFlags = InputAction_Jump | InputAction_Interact;

	ByteWriter w;
	sent.Encode(w);

	InputStateMessage got;
	ByteReader r(w.Data().data(), w.Data().size());
	ASSERT_TRUE(got.Decode(r));
	EXPECT_NE(got.ActionFlags & InputAction_Jump, 0);
	EXPECT_EQ(got.ActionFlags & InputAction_Sprint, 0);
	EXPECT_NE(got.ActionFlags & InputAction_Interact, 0);
}
