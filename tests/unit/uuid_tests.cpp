#include "engine/common/uuid.h"
#include "gtest/gtest.h"
#include <unordered_set>

using namespace Chained;

TEST(UUIDTest, GenerationUniqueness)
{
	std::unordered_set<uint64_t> uuids;
	const int count = 1000;
	for (int i = 0; i < count; i++)
	{
		UUID id;
		uuids.insert((uint64_t)id);
	}

	EXPECT_EQ(uuids.size(), count);
}

TEST(UUIDTest, CopyAndAssignment)
{
	UUID id1;
	UUID id2 = id1;
	EXPECT_EQ((uint64_t)id1, (uint64_t)id2);

	UUID id3;
	id3 = id1;
	EXPECT_EQ((uint64_t)id1, (uint64_t)id3);
}

TEST(UUIDTest, StringConversion)
{
	UUID id;
	std::string str = id.ToString();
	EXPECT_FALSE(str.empty());

	// Test conversion back from string
	UUID idFromString(str);
	EXPECT_EQ((uint64_t)id, (uint64_t)idFromString);
}

TEST(UUIDTest, InvalidStringConversion)
{
	// Test with invalid string, it should hopefully generate a valid new ID or
	// handle it gracefully depending on implementation. If not implemented
	// safely, it might crash, but let's test it.
	UUID idFromString("invalid-uuid-string");
	// Usually it just hashes the string or returns 0.
	EXPECT_EQ((uint64_t)idFromString, 0);
}

TEST(UUIDTest, Hashability)
{
	std::unordered_map<UUID, std::string> uuidMap;
	UUID id1, id2;

	uuidMap[id1] = "ID1";
	uuidMap[id2] = "ID2";

	EXPECT_EQ(uuidMap[id1], "ID1");
	EXPECT_EQ(uuidMap[id2], "ID2");
	EXPECT_EQ(uuidMap.size(), 2);
}
