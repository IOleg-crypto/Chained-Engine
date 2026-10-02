
#include "engine/core/service_locator.h"
#include "engine/physics/iphysics_world.h"
#include "engine/physics/physics.h"
#include "engine/scene/components.h"
#include "engine/scene/scene.h"
#include "gtest/gtest.h"

using namespace Chained;

TEST(PhysicsTest, Raycast)
{
	auto scene = std::make_shared<Scene>();
	auto entity = scene->CreateEntity("Test Entity");
	auto& transform = entity.GetComponent<TransformComponent>();
	transform.Translation = {0.0f, 0.0f, 5.0f};

	auto& collider = entity.AddComponent<ColliderComponent>();
	collider.Size = {1.0f, 1.0f, 1.0f};
	collider.Offset = {-0.5f, -0.5f, -0.5f};

	auto& rb = entity.AddComponent<RigidBodyComponent>();
	rb.Type = RigidBodyComponent::BodyType::Static;

	Ray ray;
	ray.position = {0.0f, 0.0f, 0.0f};
	ray.direction = {0.0f, 0.0f, 1.0f};

	scene->OnRuntimeStart();
	scene->OnUpdateRuntime(Timestep(0.016f));

	RaycastResult result = ServiceLocator::Get<Physics>()->Raycast(ray);
	EXPECT_TRUE(result.Hit);
	EXPECT_NEAR(result.Distance, 4.0f, 0.001f);
	EXPECT_EQ(result.Entity, (entt::entity)entity);

	ray.direction = {0.0f, 0.0f, -1.0f};
	result = ServiceLocator::Get<Physics>()->Raycast(ray);
	EXPECT_FALSE(result.Hit);
}

TEST(PhysicsTest, RaycastMissingCollider)
{
	auto scene = std::make_shared<Scene>();
	auto entity = scene->CreateEntity("No Collider");
	auto& transform = entity.GetComponent<TransformComponent>();
	transform.Translation = {0.0f, 0.0f, 5.0f};

	auto& rb = entity.AddComponent<RigidBodyComponent>();
	rb.Type = RigidBodyComponent::BodyType::Static;

	Ray ray;
	ray.position = {0.0f, 0.0f, 0.0f};
	ray.direction = {0.0f, 0.0f, 1.0f};

	scene->OnRuntimeStart();
	scene->OnUpdateRuntime(Timestep(0.016f));

	RaycastResult result = ServiceLocator::Get<Physics>()->Raycast(ray);
	EXPECT_FALSE(result.Hit);
}

TEST(PhysicsTest, ColliderEnabledFlag)
{
	auto scene = std::make_shared<Scene>();
	auto entity = scene->CreateEntity("Collider Entity");
	auto& collider = entity.AddComponent<ColliderComponent>();

	// Default is true
	EXPECT_TRUE(collider.Enabled);

	// Set to false
	collider.Enabled = false;
	EXPECT_FALSE(collider.Enabled);
}

TEST(PhysicsTest, ContextLifecycleResetAndClear)
{
	auto scene = std::make_shared<Scene>();

	// ResetAccumulator should be safe to call even if no accumulator exists yet
	ServiceLocator::Get<Physics>()->ResetAccumulator(scene.get());

	// ClearContext should clean up without issues
	ServiceLocator::Get<Physics>()->ClearContext(scene.get());

	// Calling ResetAccumulator after ClearContext should be safe
	ServiceLocator::Get<Physics>()->ResetAccumulator(scene.get());
}

TEST(PhysicsTest, RuntimeStartInitializesRigidBodyHandles)
{
	auto scene = std::make_shared<Scene>();
	auto entity = scene->CreateEntity("Physics Entity");
	entity.AddComponent<ColliderComponent>();
	auto& rigidBody = entity.AddComponent<RigidBodyComponent>();

	EXPECT_EQ(rigidBody.Handle, kInvalidPhysicsBody);

	scene->OnRuntimeStart();
	scene->OnUpdateRuntime(Timestep(0.016f));

	EXPECT_NE(rigidBody.Handle, kInvalidPhysicsBody);
}

namespace
{
	// Unit cube centered at the origin, half-extent 0.5, in unscaled model-local space.
	// Winding is irrelevant for mesh-shape raycasting.
	std::vector<PhysicsTriangle> MakeUnitCubeTriangles()
	{
		const glm::vec3 c[8] = {
			{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
			{-0.5f, -0.5f, 0.5f},  {0.5f, -0.5f, 0.5f},	 {0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, 0.5f},
		};
		const int idx[12][3] = {
			{0, 1, 2}, {0, 2, 3}, {4, 6, 5}, {4, 7, 6}, {0, 3, 7}, {0, 7, 4},
			{1, 5, 6}, {1, 6, 2}, {0, 4, 5}, {0, 5, 1}, {3, 2, 6}, {3, 6, 7},
		};
		std::vector<PhysicsTriangle> tris;
		tris.reserve(12);
		for (const auto& t : idx)
		{
			tris.push_back({c[t[0]], c[t[1]], c[t[2]]});
		}
		return tris;
	}
} // namespace

// Two bodies share the same cached mesh BVH (same CacheKey) but different MeshScale.
// If scale were baked into the cached shape, the second body would inherit the first's
// scale; instead each gets its own ScaledShape over one shared bare BVH, so the raycast
// hit distance to each near face reflects its own scale.
TEST(PhysicsTest, MeshShapeCacheIsScaleIndependent)
{
	auto* physics = ServiceLocator::Get<Physics>();
	physics->ResetWorld(); // fresh world → clean shape cache
	auto* world = physics->GetWorld();
	ASSERT_NE(world, nullptr);

	auto tris = MakeUnitCubeTriangles();

	// Body A: scale 1 at x=0, z=10 → near (-Z) face at z = 10 - 0.5 = 9.5
	PhysicsBodyDesc a;
	a.Shape = ColliderType::Mesh;
	a.IsStatic = true;
	a.Position = {0.0f, 0.0f, 10.0f};
	a.Triangles = tris;
	a.MeshScale = {1.0f, 1.0f, 1.0f};
	a.CacheKey = "unit_cube_test";
	PhysicsBodyHandle hA = world->CreateBody(a);
	ASSERT_NE(hA, kInvalidPhysicsBody);

	// Body B: same CacheKey, scale 2 at x=100, z=10 → near face at z = 10 - 1.0 = 9.0
	PhysicsBodyDesc b;
	b.Shape = ColliderType::Mesh;
	b.IsStatic = true;
	b.Position = {100.0f, 0.0f, 10.0f};
	b.Triangles = tris;
	b.MeshScale = {2.0f, 2.0f, 2.0f};
	b.CacheKey = "unit_cube_test";
	PhysicsBodyHandle hB = world->CreateBody(b);
	ASSERT_NE(hB, kInvalidPhysicsBody);

	RaycastResult hitA = world->Raycast({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 1000.0f);
	ASSERT_TRUE(hitA.Hit);
	EXPECT_NEAR(hitA.Distance, 9.5f, 0.01f);

	RaycastResult hitB = world->Raycast({100.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 1000.0f);
	ASSERT_TRUE(hitB.Hit);
	EXPECT_NEAR(hitB.Distance, 9.0f, 0.01f);

	world->DestroyBody(hA);
	world->DestroyBody(hB);
	physics->ResetWorld();
}
