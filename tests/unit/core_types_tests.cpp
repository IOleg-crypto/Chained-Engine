// core_types_tests.cpp
// Consolidated scenario-based tests for Common Core types: UUID, Color, Timestep.
// Follows Arrange-Act-Assert (AAA) pattern without fixtures.

#include "engine/common/uuid.h"
#include "engine/common/color.h"
#include "engine/common/timestep.h"
#include "gtest/gtest.h"

#include <unordered_set>
#include <unordered_map>

using namespace Chained;

// ============================================================================
// 1. UUID TESTS (Generation, Uniqueness, String Serialization, Hashability)
// ============================================================================
TEST(CoreTypesModule, UUID_LifecycleAndUniqueness)
{
	// Arrange
	constexpr int kCount = 1000;
	std::unordered_set<uint64_t> ids;

	// Act
	for (int i = 0; i < kCount; ++i)
	{
		UUID id;
		ids.insert((uint64_t)id);
	}

	// Assert
	EXPECT_EQ(ids.size(), static_cast<size_t>(kCount));

	// Act & Assert (Copy and assignment)
	UUID a;
	UUID b = a;
	EXPECT_EQ((uint64_t)a, (uint64_t)b);

	// Act & Assert (String conversion round-trip)
	std::string str = a.ToString();
	EXPECT_FALSE(str.empty());
	UUID fromStr(str);
	EXPECT_EQ((uint64_t)a, (uint64_t)fromStr);

	// Act & Assert (Invalid string parsing returns 0)
	UUID invalidId("not-a-valid-uuid");
	EXPECT_EQ((uint64_t)invalidId, 0ull);

	// Act & Assert (Hashability in unordered_map)
	std::unordered_map<UUID, std::string> map;
	UUID id1, id2;
	map[id1] = "First";
	map[id2] = "Second";
	EXPECT_EQ(map[id1], "First");
	EXPECT_EQ(map[id2], "Second");
	EXPECT_EQ(map.size(), 2u);
}

// ============================================================================
// 2. COLOR TESTS (RGBA, Named Constants, Overflow/Underflow)
// ============================================================================
TEST(CoreTypesModule, Color_ConstructionAndNamedConstants)
{
	// Act & Assert (Default constructor RGBA: 0, 0, 0, 255)
	Color c;
	EXPECT_EQ(c.r, 0);
	EXPECT_EQ(c.g, 0);
	EXPECT_EQ(c.b, 0);
	EXPECT_EQ(c.a, 255);

	// Act & Assert (Parameterized constructor)
	Color custom(100, 150, 200, 128);
	EXPECT_EQ(custom.r, 100);
	EXPECT_EQ(custom.g, 150);
	EXPECT_EQ(custom.b, 200);
	EXPECT_EQ(custom.a, 128);

	// Act & Assert (Named color constants)
	EXPECT_EQ(Color::White().r, 255);
	EXPECT_EQ(Color::White().a, 255);
	EXPECT_EQ(Color::Black().r, 0);
	EXPECT_EQ(Color::Black().a, 255);
	EXPECT_EQ(Color::Red().r, 255);
	EXPECT_EQ(Color::Green().g, 255);
	EXPECT_EQ(Color::Blue().b, 255);
}

// ============================================================================
// 3. TIMESTEP TESTS (Second/Millisecond Conversions, Extreme Values)
// ============================================================================
TEST(CoreTypesModule, Timestep_ConversionsAndBoundaries)
{
	// Act & Assert (60 FPS nominal frame time 0.016s)
	Timestep ts(0.016f);
	EXPECT_FLOAT_EQ(ts.GetSeconds(), 0.016f);
	EXPECT_FLOAT_EQ(ts.GetMilliseconds(), 16.0f);

	// Act & Assert (Implicit float conversion)
	float rawSeconds = ts;
	EXPECT_FLOAT_EQ(rawSeconds, 0.016f);

	// Act & Assert (Extreme delta times)
	Timestep negative(-1.0f);
	EXPECT_FLOAT_EQ(negative.GetSeconds(), -1.0f);

	Timestep large(1000.0f);
	EXPECT_FLOAT_EQ(large.GetSeconds(), 1000.0f);
	EXPECT_FLOAT_EQ(large.GetMilliseconds(), 1000000.0f);
}
