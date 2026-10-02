// physics_tests.cpp
// Consolidated scenario-based tests for Physics and Collision (Jolt Physics integration).
// Follows Arrange-Act-Assert (AAA) pattern without fixtures.

#include "engine/core/service_locator.h"
#include "engine/physics/iphysics_world.h"
#include "engine/physics/physics.h"
#include "engine/scene/components.h"
#include "engine/scene/scene.h"
#include "gtest/gtest.h"

#include <vector>

using namespace Chained;

namespace
{
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

// ============================================================================
// 1. POSITIVE SCENARIO (Happy Path: Raycast, Jolt Handles, Mesh Scaling)
// ============================================================================
TEST(PhysicsModule, Positive_LifecycleRaycastAndMeshScaling)
{
	// Arrange
	auto* physics = ServiceLocator::Get<Physics>();
	ASSERT_NE(physics, nullptr);

	auto scene = std::make_shared<Scene>();

	Entity target = scene->CreateEntity("StaticCube");
	auto& transform = target.GetComponent<TransformComponent>();
	transform.Translation = {0.0f, 0.0f, 5.0f};

	auto& collider = target.AddComponent<ColliderComponent>();
	collider.Size = {1.0f, 1.0f, 1.0f};
	collider.Offset = {-0.5f, -0.5f, -0.5f};

	auto& rb = target.AddComponent<RigidBodyComponent>();
	rb.Type = RigidBodyComponent::BodyType::Static;

	// Act (Step 1: Start runtime — initializes Jolt handle)
	scene->OnRuntimeStart();
	scene->OnUpdateRuntime(Timestep(0.016f));

	// Assert
	EXPECT_NE(rb.Handle, kInvalidPhysicsBody);

	// Act (Step 2: Raycast towards cube at +Z)
	Ray forwardRay;
	forwardRay.position = {0.0f, 0.0f, 0.0f};
	forwardRay.direction = {0.0f, 0.0f, 1.0f};

	RaycastResult hitResult = physics->Raycast(forwardRay);

	// Assert
	EXPECT_TRUE(hitResult.Hit);
	EXPECT_NEAR(hitResult.Distance, 4.0f, 0.01f);
	EXPECT_EQ(hitResult.Entity, (entt::entity)target);

	// Arrange (Step 3: Multi-body scale-independent mesh cache)
	physics->ResetWorld();
	auto* world = physics->GetWorld();
	ASSERT_NE(world, nullptr);

	auto cubeTriangles = MakeUnitCubeTriangles();

	// Act (Body A: Scale 1.0 at Z=10 -> near face at 9.5)
	PhysicsBodyDesc descA;
	descA.Shape = ColliderType::Mesh;
	descA.IsStatic = true;
	descA.Position = {0.0f, 0.0f, 10.0f};
	descA.Triangles = cubeTriangles;
	descA.MeshScale = {1.0f, 1.0f, 1.0f};
	descA.CacheKey = "unit_cube_shared";
	PhysicsBodyHandle bodyA = world->CreateBody(descA);

	// Act (Body B: Same CacheKey, Scale 2.0 at (100, 0, 10) -> near face at 9.0)
	PhysicsBodyDesc descB;
	descB.Shape = ColliderType::Mesh;
	descB.IsStatic = true;
	descB.Position = {100.0f, 0.0f, 10.0f};
	descB.Triangles = cubeTriangles;
	descB.MeshScale = {2.0f, 2.0f, 2.0f};
	descB.CacheKey = "unit_cube_shared";
	PhysicsBodyHandle bodyB = world->CreateBody(descB);

	// Assert
	ASSERT_NE(bodyA, kInvalidPhysicsBody);
	ASSERT_NE(bodyB, kInvalidPhysicsBody);

	RaycastResult hitA = world->Raycast({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 1000.0f);
	EXPECT_TRUE(hitA.Hit);
	EXPECT_NEAR(hitA.Distance, 9.5f, 0.01f);

	RaycastResult hitB = world->Raycast({100.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 1000.0f);
	EXPECT_TRUE(hitB.Hit);
	EXPECT_NEAR(hitB.Distance, 9.0f, 0.01f);

	world->DestroyBody(bodyA);
	world->DestroyBody(bodyB);
	physics->ResetWorld();
}

// ============================================================================
// 2. NEGATIVE SCENARIO (Crash-Safety, Misses, Missing Colliders)
// ============================================================================
TEST(PhysicsModule, Negative_EdgeCasesAndMissingColliders)
{
	// Arrange
	auto* physics = ServiceLocator::Get<Physics>();
	ASSERT_NE(physics, nullptr);

	auto scene = std::make_shared<Scene>();

	// Act & Assert (Step 1: Entity with RigidBody but NO ColliderComponent)
	Entity noColliderEntity = scene->CreateEntity("NoCollider");
	noColliderEntity.GetComponent<TransformComponent>().Translation = {0.0f, 0.0f, 5.0f};
	auto& rb = noColliderEntity.AddComponent<RigidBodyComponent>();
	rb.Type = RigidBodyComponent::BodyType::Static;

	scene->OnRuntimeStart();
	scene->OnUpdateRuntime(Timestep(0.016f));

	Ray ray;
	ray.position = {0.0f, 0.0f, 0.0f};
	ray.direction = {0.0f, 0.0f, 1.0f};

	EXPECT_FALSE(physics->Raycast(ray).Hit);

	// Act & Assert (Step 2: Collider with Enabled = false)
	auto& collider = noColliderEntity.AddComponent<ColliderComponent>();
	collider.Enabled = false;
	EXPECT_FALSE(collider.Enabled);

	// Act & Assert (Step 3: Raycast in opposite direction -Z)
	ray.direction = {0.0f, 0.0f, -1.0f};
	EXPECT_FALSE(physics->Raycast(ray).Hit);

	// Act & Assert (Step 4: Context lifecycle safety calls)
	EXPECT_NO_THROW(physics->ResetAccumulator(scene.get()));
	EXPECT_NO_THROW(physics->ClearContext(scene.get()));
	EXPECT_NO_THROW(physics->ResetAccumulator(scene.get()));
	EXPECT_NO_THROW(physics->ClearContext(nullptr));
}
