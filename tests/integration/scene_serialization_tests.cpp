#include "engine/scene/components.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_serializer.h"
#include "gtest/gtest.h"

#include <filesystem>
#include <fstream>

using namespace Chained;

// Destructive / malformed-input tests for SceneSerializer. These exercise the parser against
// inputs it must survive without throwing, even when it can't produce a usable scene.
class SceneSerializationDestructiveTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		std::filesystem::create_directories("test_assets");
	}
	void TearDown() override
	{
		std::filesystem::remove_all("test_assets");
	}

	static void WriteFile(const std::string& path, const std::string& content)
	{
		std::ofstream f(path);
		f << content;
	}
};

// Empty file — must return false, must not throw.
TEST_F(SceneSerializationDestructiveTest, EmptyFile)
{
	std::string path = "test_assets/empty.chscene";
	WriteFile(path, "");

	Scene scene;
	SceneSerializer serializer(&scene);
	EXPECT_NO_THROW({
		bool ok = serializer.Deserialize(path);
		EXPECT_FALSE(ok);
	});
}

// Binary garbage — YAML parser must not throw unhandled exception.
TEST_F(SceneSerializationDestructiveTest, BinaryGarbage)
{
	std::string path = "test_assets/garbage.chscene";
	{
		std::ofstream f(path, std::ios::binary);
		static const unsigned char kGarbage[] = {0xFF, 0x00, 0xDE, 0xAD, 0xBE, 0xEF, 0x13, 0x37};
		f.write(reinterpret_cast<const char*>(kGarbage), sizeof(kGarbage));
	}

	Scene scene;
	SceneSerializer serializer(&scene);
	EXPECT_NO_THROW(serializer.Deserialize(path));
}

// Valid YAML missing the "Scene:" root key.
TEST_F(SceneSerializationDestructiveTest, MissingSceneKey)
{
	std::string path = "test_assets/no_scene.chscene";
	WriteFile(path, "Entities:\n  - tag: Foo\n");

	Scene scene;
	SceneSerializer serializer(&scene);
	EXPECT_NO_THROW(serializer.Deserialize(path));
}

// Entity block missing UUID entirely — must not crash; either skip the entity or assign a new UUID.
TEST_F(SceneSerializationDestructiveTest, EntityMissingUUID)
{
	std::string path = "test_assets/no_uuid.chscene";
	WriteFile(path, "Scene: Test\n"
					"Entities:\n"
					"  - Entity:\n"
					"      TagComponent:\n"
					"        Tag: NoUUID\n");

	Scene scene;
	SceneSerializer serializer(&scene);
	EXPECT_NO_THROW(serializer.Deserialize(path));
}

// Truncated mid-token YAML.
TEST_F(SceneSerializationDestructiveTest, TruncatedYAML)
{
	std::string path = "test_assets/truncated.chscene";
	WriteFile(path,
			  "Scene: Test\n"
			  "Entities:\n"
			  "  - Entity:\n"
			  "      TagCompon"); // intentionally cut

	Scene scene;
	SceneSerializer serializer(&scene);
	EXPECT_NO_THROW(serializer.Deserialize(path));
}

// Path that does not exist.
TEST_F(SceneSerializationDestructiveTest, NonExistentFile)
{
	Scene scene;
	SceneSerializer serializer(&scene);
	EXPECT_NO_THROW({
		bool ok = serializer.Deserialize("test_assets/ghost.chscene");
		EXPECT_FALSE(ok);
	});
}

// Malformed string input via DeserializeFromString — parser must report failure, not throw.
TEST_F(SceneSerializationDestructiveTest, DeserializeFromStringInvalidYAML)
{
	Scene scene;
	SceneSerializer serializer(&scene);
	EXPECT_NO_THROW({
		bool ok = serializer.DeserializeFromString("not: [valid, yaml");
		EXPECT_FALSE(ok);
	});
}

// Deeply nested / duplicate keys shouldn't cause infinite loops or crashes.
TEST_F(SceneSerializationDestructiveTest, DuplicateEntityUUIDs)
{
	std::string path = "test_assets/dup_uuid.chscene";
	WriteFile(path, "Scene: Test\n"
					"Entities:\n"
					"  - Entity: 1234567890\n"
					"    TagComponent:\n"
					"      Tag: First\n"
					"  - Entity: 1234567890\n"
					"    TagComponent:\n"
					"      Tag: Second\n");

	Scene scene;
	SceneSerializer serializer(&scene);
	EXPECT_NO_THROW(serializer.Deserialize(path));
}

// Round-trip with many entities — verify count and tags survive serialize/deserialize.
TEST_F(SceneSerializationDestructiveTest, LargeSceneRoundTrip)
{
	constexpr int kCount = 200;
	std::string path = "test_assets/large.chscene";

	std::vector<UUID> originalUUIDs;
	{
		Scene scene;
		for (int i = 0; i < kCount; ++i)
		{
			Entity e = scene.CreateEntity("E_" + std::to_string(i));
			originalUUIDs.push_back(e.GetUUID());
		}

		SceneSerializer serializer(&scene);
		ASSERT_TRUE(serializer.Serialize(path));
	}

	{
		Scene scene;
		SceneSerializer serializer(&scene);
		ASSERT_TRUE(serializer.Deserialize(path));

		int count = 0;
		auto view = scene.GetRegistry().view<TagComponent>();
		for (auto e : view)
		{
			(void)e;
			++count;
		}
		EXPECT_EQ(count, kCount);

		// Spot-check a few entities preserved their UUID and tag.
		Entity first = scene.GetEntityByUUID(originalUUIDs.front());
		ASSERT_TRUE(first);
		EXPECT_EQ(first.GetComponent<TagComponent>().Tag, "E_0");

		Entity last = scene.GetEntityByUUID(originalUUIDs.back());
		ASSERT_TRUE(last);
		EXPECT_EQ(last.GetComponent<TagComponent>().Tag, "E_" + std::to_string(kCount - 1));
	}
}

// Round-trip via the in-memory string API — no filesystem involved.
TEST_F(SceneSerializationDestructiveTest, StringRoundTrip)
{
	std::string yaml;
	UUID uuid;
	{
		Scene scene;
		Entity e = scene.CreateEntity("StringEntity");
		uuid = e.GetUUID();

		SceneSerializer serializer(&scene);
		yaml = serializer.SerializeToString();
		EXPECT_FALSE(yaml.empty());
	}

	{
		Scene scene;
		SceneSerializer serializer(&scene);
		ASSERT_TRUE(serializer.DeserializeFromString(yaml));

		Entity found = scene.GetEntityByUUID(uuid);
		ASSERT_TRUE(found);
		EXPECT_EQ(found.GetComponent<TagComponent>().Tag, "StringEntity");
	}
}

// Re-deserializing into a non-empty scene shouldn't leak/duplicate previously-existing entities
// beyond what's expected, and must not crash.
TEST_F(SceneSerializationDestructiveTest, DeserializeIntoNonEmptyScene)
{
	std::string path = "test_assets/second.chscene";
	{
		Scene source;
		source.CreateEntity("FromFile");
		SceneSerializer serializer(&source);
		ASSERT_TRUE(serializer.Serialize(path));
	}

	Scene scene;
	scene.CreateEntity("PreExisting");

	SceneSerializer serializer(&scene);
	EXPECT_NO_THROW(serializer.Deserialize(path));
}
