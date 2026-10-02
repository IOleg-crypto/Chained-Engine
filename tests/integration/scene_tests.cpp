// scene_tests.cpp
// Consolidated scenario-based tests for Scene, Entity, Hierarchy, and SceneSerializer.
// Follows Arrange-Act-Assert (AAA) pattern without fixtures.

#include "engine/scene/scene.h"
#include "engine/scene/components.h"
#include "engine/scene/scene_serializer.h"
#include "gtest/gtest.h"

#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <vector>

using namespace Chained;

// ============================================================================
// 1. POSITIVE SCENARIO (Happy Path: ECS, Hierarchy, Cloning, State Machine)
// ============================================================================
TEST(SceneModule, Positive_EntityHierarchyAndCloning)
{
	// Arrange
	Scene scene;

	// Act (Step 1: Construct Root -> Child -> SubChild entity hierarchy)
	Entity root = scene.CreateEntity("RootEntity");
	Entity child = scene.CreateEntity("ChildEntity");
	Entity subChild = scene.CreateEntity("SubChildEntity");

	root.AddComponent<CameraComponent>().Primary = true;
	auto& rootTransform = root.GetComponent<TransformComponent>();
	rootTransform.Translation = {0.0f, 10.0f, 0.0f};

	auto& childTransform = child.GetComponent<TransformComponent>();
	childTransform.Translation = {5.0f, 0.0f, 0.0f};

	auto& rootHc = root.AddComponent<HierarchyComponent>();
	auto& childHc = child.AddComponent<HierarchyComponent>();
	auto& subChildHc = subChild.AddComponent<HierarchyComponent>();

	childHc.Parent = (entt::entity)root;
	rootHc.Children.push_back((entt::entity)child);

	subChildHc.Parent = (entt::entity)child;
	childHc.Children.push_back((entt::entity)subChild);

	// Assert
	ASSERT_TRUE(root.IsValid());
	ASSERT_TRUE(child.IsValid());
	ASSERT_TRUE(subChild.IsValid());
	EXPECT_EQ(childHc.Parent, (entt::entity)root);
	EXPECT_EQ(subChildHc.Parent, (entt::entity)child);
	ASSERT_EQ(rootHc.Children.size(), 1u);
	EXPECT_EQ(rootHc.Children[0], (entt::entity)child);

	// Act (Step 2: Lookup by Tag and UUID)
	Entity foundByTag = scene.FindEntityByTag("ChildEntity");
	Entity foundByUUID = scene.GetEntityByUUID(root.GetUUID());

	// Assert
	EXPECT_TRUE(foundByTag.IsValid());
	EXPECT_EQ(foundByTag.GetUUID(), child.GetUUID());
	EXPECT_TRUE(foundByUUID.IsValid());
	EXPECT_EQ(foundByUUID.GetName(), "RootEntity");

	// Act (Step 3: Deep clone entity subtree)
	Entity cloned = {scene.CopyEntity(root), scene.GetRegistryPtr()};

	// Assert
	ASSERT_TRUE(cloned.IsValid());
	EXPECT_EQ(cloned.GetName(), "RootEntity_copy");
	EXPECT_NE(cloned.GetUUID(), root.GetUUID()); // Unique UUID generated
	EXPECT_TRUE(cloned.HasComponent<CameraComponent>());
	EXPECT_TRUE(cloned.GetComponent<CameraComponent>().Primary);
	EXPECT_FLOAT_EQ(cloned.GetComponent<TransformComponent>().Translation.y, 10.0f);

	// Act & Assert (Step 4: Scene state machine transitions)
	EXPECT_EQ(scene.GetSceneState(), SceneState::Edit);
	scene.TransitionToState(SceneState::Play);
	EXPECT_EQ(scene.GetSceneState(), SceneState::Play);
	scene.TransitionToState(SceneState::Simulate);
	EXPECT_EQ(scene.GetSceneState(), SceneState::Simulate);
	scene.TransitionToState(SceneState::Edit);
	EXPECT_EQ(scene.GetSceneState(), SceneState::Edit);
}

// ============================================================================
// 2. NEGATIVE SCENARIO (Cascade Destruction, Stale Handles, Corrupted YAML)
// ============================================================================
TEST(SceneModule, Negative_CorruptedDataAndCascadeDestruction)
{
	// Arrange
	Scene scene;

	// Act (Step 1: Parent destruction cascades to children)
	Entity parent = scene.CreateEntity("Parent");
	Entity child = scene.CreateEntity("Child");
	entt::entity childHandle = (entt::entity)child;

	parent.AddComponent<HierarchyComponent>().Children.push_back((entt::entity)child);
	child.AddComponent<HierarchyComponent>().Parent = (entt::entity)parent;

	scene.DestroyEntity(parent);

	// Assert
	EXPECT_FALSE(scene.GetRegistry().valid((entt::entity)parent));
	EXPECT_FALSE(scene.GetRegistry().valid(childHandle));

	// Act (Step 2: Destroy child before parent — parent stays safe)
	Entity p2 = scene.CreateEntity("P2");
	Entity c2 = scene.CreateEntity("C2");
	p2.AddComponent<HierarchyComponent>().Children.push_back((entt::entity)c2);
	c2.AddComponent<HierarchyComponent>().Parent = (entt::entity)p2;

	// Assert
	EXPECT_NO_THROW(scene.DestroyEntity(c2));
	EXPECT_TRUE(scene.GetRegistry().valid((entt::entity)p2));
	EXPECT_NO_THROW(scene.DestroyEntity(p2));

	// Act & Assert (Step 3: Invalid entity operations return false without throwing)
	Entity invalidEntity;
	EXPECT_FALSE(invalidEntity.IsValid());
	EXPECT_NO_THROW(scene.DestroyEntity(invalidEntity));
	EXPECT_FALSE(scene.GetEntityByUUID(UUID(0xDEADBEEFCAFEBABEull)).IsValid());
	EXPECT_FALSE(scene.FindEntityByTag("DefinitelyNonExistentTag").IsValid());

	// Act & Assert (Step 4: SceneSerializer handles malformed/corrupted data safely)
	SceneSerializer serializer(&scene);
	EXPECT_FALSE(serializer.DeserializeFromString(""));
	EXPECT_FALSE(serializer.DeserializeFromString("invalid: [yaml, syntax"));
	EXPECT_NO_THROW(serializer.DeserializeFromString("SomeOtherKey: 12345\n"));
	EXPECT_FALSE(serializer.Deserialize("non_existent_folder/missing_scene.chscene"));
}

// ============================================================================
// 3. STRESS SCENARIO (Mass Entities and String Serialization Round-Trip)
// ============================================================================
TEST(SceneModule, Stress_MassEntitiesAndSerializationRoundTrip)
{
	// Arrange
	constexpr int kEntityCount = 300;
	std::string serializedYaml;
	std::vector<UUID> originalUUIDs;

	// Act (Phase 1: Create 300 entities and serialize to string)
	{
		Scene sourceScene;
		std::unordered_set<uint64_t> uniqueCheck;

		for (int i = 0; i < kEntityCount; ++i)
		{
			Entity e = sourceScene.CreateEntity("Entity_" + std::to_string(i));
			auto& tc = e.GetComponent<TransformComponent>();
			tc.Translation = {static_cast<float>(i), 0.0f, -static_cast<float>(i)};

			UUID id = e.GetUUID();
			originalUUIDs.push_back(id);
			uniqueCheck.insert((uint64_t)id);
		}

		EXPECT_EQ(uniqueCheck.size(), static_cast<size_t>(kEntityCount));

		SceneSerializer serializer(&sourceScene);
		serializedYaml = serializer.SerializeToString();
	}

	// Assert (Phase 1: Serialized string is valid)
	ASSERT_FALSE(serializedYaml.empty());

	// Act & Assert (Phase 2: Deserialize into fresh scene and spot check)
	{
		Scene targetScene;
		SceneSerializer serializer(&targetScene);
		ASSERT_TRUE(serializer.DeserializeFromString(serializedYaml));

		int count = 0;
		for (auto entity : targetScene.GetRegistry().view<TagComponent>())
		{
			(void)entity;
			++count;
		}
		EXPECT_EQ(count, kEntityCount);

		Entity first = targetScene.GetEntityByUUID(originalUUIDs.front());
		ASSERT_TRUE(first.IsValid());
		EXPECT_EQ(first.GetName(), "Entity_0");
		EXPECT_FLOAT_EQ(first.GetComponent<TransformComponent>().Translation.x, 0.0f);

		Entity last = targetScene.GetEntityByUUID(originalUUIDs.back());
		ASSERT_TRUE(last.IsValid());
		EXPECT_EQ(last.GetName(), "Entity_" + std::to_string(kEntityCount - 1));
	}

	// Act & Assert (Phase 3: Wide tree with 300 children destroyed in O(n))
	{
		Scene treeScene;
		Entity root = treeScene.CreateEntity("TreeRoot");
		auto& rootHc = root.AddComponent<HierarchyComponent>();

		for (int i = 0; i < 300; ++i)
		{
			Entity child = treeScene.CreateEntity("Leaf_" + std::to_string(i));
			child.AddComponent<HierarchyComponent>().Parent = (entt::entity)root;
			rootHc.Children.push_back((entt::entity)child);
		}

		EXPECT_EQ(rootHc.Children.size(), 300u);
		EXPECT_NO_THROW(treeScene.DestroyEntity(root));

		int remaining = 0;
		for (auto entity : treeScene.GetRegistry().view<TagComponent>())
		{
			(void)entity;
			++remaining;
		}
		EXPECT_EQ(remaining, 0);
	}
}
