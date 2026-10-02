#include "engine/app/application.h"
#include "engine/core/service_locator.h"
#include "engine/scene/components.h"
#include "engine/scene/scene.h"
#include "engine/scene/component_serializer.h"
#include "gtest/gtest.h"

using namespace Chained;

class SceneTest : public ::testing::Test
{
};

TEST_F(SceneTest, CreateEntity)
{
	Scene scene;
	Entity entity = scene.CreateEntity("Test Entity");

	EXPECT_TRUE(entity);
	EXPECT_TRUE(entity.HasComponent<TagComponent>());
	EXPECT_TRUE(entity.HasComponent<TransformComponent>());
	EXPECT_EQ(entity.GetComponent<TagComponent>().Tag, "Test Entity");
}

TEST_F(SceneTest, DestroyEntity)
{
	Scene scene;
	Entity entity = scene.CreateEntity("To Destroy");
	entt::entity handle = entity;

	scene.DestroyEntity(entity);
	EXPECT_FALSE(scene.GetRegistry().valid(handle));
}

TEST_F(SceneTest, ComponentOperations)
{
	Scene scene;
	Entity entity = scene.CreateEntity();

	struct CustomComponent
	{
		int Value;
	};
	entity.AddComponent<CustomComponent>(42);

	EXPECT_TRUE(entity.HasComponent<CustomComponent>());
	EXPECT_EQ(entity.GetComponent<CustomComponent>().Value, 42);

	entity.RemoveComponent<CustomComponent>();
	EXPECT_FALSE(entity.HasComponent<CustomComponent>());
}

TEST_F(SceneTest, EntityRenaming)
{
	Scene scene;
	Entity entity = scene.CreateEntity("Old Name");

	auto& tag = entity.GetComponent<TagComponent>().Tag;
	EXPECT_EQ(tag, "Old Name");

	tag = "New Name";
	EXPECT_EQ(entity.GetComponent<TagComponent>().Tag, "New Name");
}

TEST_F(SceneTest, FindEntityByTag)
{
	Scene scene;
	scene.CreateEntity("Entity A");
	scene.CreateEntity("Entity B");

	Entity found = scene.FindEntityByTag("Entity B");
	EXPECT_TRUE(found);
	EXPECT_EQ(found.GetName(), "Entity B");

	Entity notFound = scene.FindEntityByTag("Non-existent");
	EXPECT_FALSE(notFound);
}

TEST_F(SceneTest, GetEntityByUUID)
{
	Scene scene;
	Entity entity = scene.CreateEntity("Tracked Entity");
	UUID id = entity.GetUUID();

	Entity found = scene.GetEntityByUUID(id);
	EXPECT_TRUE(found);
	EXPECT_EQ(found.GetName(), "Tracked Entity");

	Entity notFound = scene.GetEntityByUUID(UUID(999999));
	EXPECT_FALSE(notFound);
}

TEST_F(SceneTest, CopyEntity)
{
	Scene scene;
	Entity src = scene.CreateEntity("Source");
	src.AddComponent<CameraComponent>().Primary = true;

	Entity dst = {scene.CopyEntity(src), scene.GetRegistryPtr()};
	EXPECT_TRUE(dst);
	EXPECT_EQ(dst.GetName(), "Source_copy"); // Copy must have "_copy" suffix
	EXPECT_NE(src.GetUUID(), dst.GetUUID()); // Different UUID
	EXPECT_TRUE(dst.HasComponent<CameraComponent>());
	EXPECT_TRUE(dst.GetComponent<CameraComponent>().Primary);
}

TEST_F(SceneTest, CopyEntityResetsManagedScriptRuntimeState)
{
	Scene scene;
	Entity src = scene.CreateEntity("Scripted");

	auto& scripts = src.AddComponent<ManagedScriptComponent>().Scripts;
	scripts.emplace_back("TestScript");
	auto& script = scripts.back();
	// Simulate a live runtime instance
	script.IsInstantiated = true;
	script.NeedsStart = false;

	Entity dst = {scene.CopyEntity(src), scene.GetRegistryPtr()};
	ASSERT_TRUE(dst.HasComponent<ManagedScriptComponent>());

	const auto& copiedScripts = dst.GetComponent<ManagedScriptComponent>().Scripts;
	ASSERT_EQ(copiedScripts.size(), 1u);
	EXPECT_EQ(copiedScripts[0].ClassName, "TestScript");
	EXPECT_EQ(copiedScripts[0].Fields.size(), 0u);
	EXPECT_FALSE(copiedScripts[0].HasInstance());
	EXPECT_TRUE(copiedScripts[0].NeedsStart);
}

TEST_F(SceneTest, DestructiveOperations)
{
	Scene scene;
	Entity entity1 = scene.CreateEntity("Entity 1");
	Entity entity2 = scene.CreateEntity("Entity 2");

	entt::entity handle1 = entity1;
	entt::entity handle2 = entity2;
	UUID uuid1 = entity1.GetUUID();

	// 1. Destroy one entity, check if other is intact
	scene.DestroyEntity(entity1);
	EXPECT_FALSE(scene.GetRegistry().valid(handle1));
	EXPECT_TRUE(scene.GetRegistry().valid(handle2));

	// 2. Fetch by UUID should fail for destroyed entity
	EXPECT_FALSE(scene.GetEntityByUUID(uuid1));

	// 3. Component removal stress test
	entity2.AddComponent<CameraComponent>();
	entity2.AddComponent<LightComponent>();
	entity2.AddComponent<SpriteComponent>();

	EXPECT_TRUE(entity2.HasComponent<CameraComponent>());
	EXPECT_TRUE(entity2.HasComponent<LightComponent>());
	EXPECT_TRUE(entity2.HasComponent<SpriteComponent>());

	entity2.RemoveComponent<LightComponent>();
	EXPECT_FALSE(entity2.HasComponent<LightComponent>());
	EXPECT_TRUE(entity2.HasComponent<CameraComponent>()); // Others should still exist

	// 4. Repeated add/remove
	entity2.AddComponent<LightComponent>();
	EXPECT_TRUE(entity2.HasComponent<LightComponent>());
	entity2.RemoveComponent<LightComponent>();
	EXPECT_FALSE(entity2.HasComponent<LightComponent>());

	// 5. Destroying scene should clear remaining entities
	// (We simulate this by verifying the registry count drops, though destruction of the Scene obj handles actual
	// cleanup)
	scene.DestroyEntity(entity2);
	EXPECT_FALSE(scene.GetRegistry().valid(handle2));

	// 6. Test trying to destroy an invalid entity (should not crash)
	Entity invalidEntity; // Default constructor creates null entity
	EXPECT_NO_THROW({ scene.DestroyEntity(invalidEntity); });
}

TEST_F(SceneTest, EntityTagUniqueness)
{
	Scene scene;
	scene.CreateEntity("SameTag");
	scene.CreateEntity("SameTag");

	Entity found = scene.FindEntityByTag("SameTag");
	EXPECT_TRUE(found);

	int count = 0;
	auto& reg = scene.GetRegistry();
	for (auto entity : reg.view<TagComponent>())
	{
		Entity e(entity, &reg);
		if (e.GetName() == "SameTag")
		{
			++count;
		}
	}
	EXPECT_EQ(count, 2);
}

TEST_F(SceneTest, CopyEntityDeepCopy)
{
	Scene scene;
	Entity src = scene.CreateEntity("Original");
	src.GetComponent<TransformComponent>().Translation = {1.0f, 2.0f, 3.0f};
	src.AddComponent<LightComponent>();
	src.AddComponent<SpriteComponent>();

	Entity dst = {scene.CopyEntity(src), scene.GetRegistryPtr()};

	auto& srcTransform = src.GetComponent<TransformComponent>();
	auto& dstTransform = dst.GetComponent<TransformComponent>();
	EXPECT_FLOAT_EQ(dstTransform.Translation.x, 1.0f);
	EXPECT_FLOAT_EQ(dstTransform.Translation.y, 2.0f);
	EXPECT_FLOAT_EQ(dstTransform.Translation.z, 3.0f);

	srcTransform.Translation = {10.0f, 20.0f, 30.0f};
	EXPECT_FLOAT_EQ(dstTransform.Translation.x, 1.0f);

	EXPECT_TRUE(dst.HasComponent<LightComponent>());
	EXPECT_TRUE(dst.HasComponent<SpriteComponent>());
}

TEST_F(SceneTest, AddMultipleComponentTypes)
{
	Scene scene;
	Entity entity = scene.CreateEntity("MultiComponent");

	entity.AddComponent<CameraComponent>();
	entity.AddComponent<LightComponent>();
	entity.AddComponent<SpriteComponent>();

	EXPECT_TRUE(entity.HasComponent<CameraComponent>());
	EXPECT_TRUE(entity.HasComponent<LightComponent>());
	EXPECT_TRUE(entity.HasComponent<SpriteComponent>());
}

TEST_F(SceneTest, DestroyEntityInIteration)
{
	Scene scene;
	for (int i = 0; i < 5; ++i)
	{
		scene.CreateEntity("E" + std::to_string(i));
	}

	std::vector<entt::entity> toDestroy;
	auto& reg = scene.GetRegistry();
	for (auto entity : reg.view<TagComponent>())
	{
		Entity e(entity, &reg);
		if (e.GetName() == "E2")
		{
			toDestroy.push_back(entity);
		}
	}

	for (auto handle : toDestroy)
	{
		scene.DestroyEntity(Entity(handle, &reg));
	}

	EXPECT_FALSE(scene.FindEntityByTag("E2"));
	EXPECT_TRUE(scene.FindEntityByTag("E0"));
	EXPECT_TRUE(scene.FindEntityByTag("E4"));
}
