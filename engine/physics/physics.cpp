#include "physics.h"

#include "engine/core/service_locator.h"
#include "engine/scene/components.h"
#include "engine/scene/scene.h"
#include "engine/scene/systems/physics_body_system.h"
#include "engine/scene/systems/transform_system.h"
#include "iphysics_world.h"
#include "jolt_physics_world.h"

#include "engine/project/project.h"

namespace Chained
{

	// Named constants replacing magic numbers
	static constexpr float kFixedDtDefault = 1.0f / 60.0f;
	static constexpr int kMaxStepsPerFrame = 8;
	static constexpr float kVelocityYThreshold = 0.5f;
	static constexpr float kDefaultRaycastDistance = 1000.0f;

	Physics::Physics() = default;
	Physics::~Physics() = default;

	void Physics::Initialize()
	{
		CH_CORE_INFO("Physics initialized (Jolt backend).");
	}

	void Physics::Shutdown()
	{
		m_World.reset();
		CH_CORE_INFO("Physics shutdown.");
	}

	IPhysicsWorld* Physics::GetWorld()
	{
		if (!m_World)
		{
			m_World = std::make_unique<JoltPhysicsWorld>();
		}
		return m_World.get();
	}

	void Physics::ResetWorld(Scene* scene)
	{
		auto oldJoltWorld = dynamic_cast<JoltPhysicsWorld*>(m_World.get());

		// Invalidate all rigid body handles before destroying the world
		if (scene)
		{
			auto& registry = scene->GetRegistry();
			auto view = registry.view<RigidBodyComponent>();
			for (auto entity : view)
			{
				auto& rb = view.get<RigidBodyComponent>(entity);
				rb.Handle = kInvalidPhysicsBody;
			}
			registry.ctx().erase<Physics*>();
		}

		std::unordered_map<std::string, JPH::RefConst<JPH::Shape>> cachedMeshShapes;
		std::unordered_map<std::string, JPH::RefConst<JPH::Shape>> cachedConvexHulls;
		if (oldJoltWorld)
		{
			cachedMeshShapes = oldJoltWorld->GetMeshShapeCache();
			cachedConvexHulls = oldJoltWorld->GetConvexHullCache();
		}

		// Destroy old world cleanly before instantiating the new world
		m_World.reset();

		auto newWorld = std::make_unique<JoltPhysicsWorld>();
		newWorld->RestoreShapeCache(std::move(cachedMeshShapes), std::move(cachedConvexHulls));
		m_World = std::move(newWorld);

		if (auto project = Project::GetActive())
		{
			float gravity = project->GetConfig().Physics.Gravity;
			m_World->SetGravity(gravity);
		}

		CH_CORE_INFO("Physics: World reset — fresh Jolt world created (shape cache preserved).");
	}

	void Physics::InitializeBodies(Scene* scene)
	{
		// Use m_World directly — ResetWorld() already created the world.
		// GetWorld() would lazily create a new one without gravity/contact listener.
		auto world = m_World.get();
		if (!world)
		{
			return;
		}

		auto& registry = scene->GetRegistry();
		if (!registry.ctx().contains<Physics*>())
		{
			registry.ctx().emplace<Physics*>(this);
		}

		PhysicsBodySystem::BatchInitializeBodies(registry, world);

		CH_CORE_INFO("Physics::InitializeBodies — bodies initialized for scene '{}'.", scene->GetSettings().Name);
	}

	void Physics::Update(Scene* scene, Timestep deltaTime, bool runtime)
	{
		if (!runtime)
		{
			return;
		}

		float& accumulator = m_Accumulators[scene];
		accumulator += deltaTime;

		float fixedDt = kFixedDtDefault;
		if (auto project = Project::GetActive())
		{
			fixedDt = project->GetConfig().Physics.FixedTimestep;
		}
		bool stepped = false;

		auto world = GetWorld();
		if (!world)
		{
			return;
		}

		auto& registry = scene->GetRegistry();
		auto view = registry.view<TransformComponent, RigidBodyComponent>();

		for (auto entity : view)
		{
			auto& rb = view.get<RigidBodyComponent>(entity);
			auto& transform = view.get<TransformComponent>(entity);

			if (rb.Handle == kInvalidPhysicsBody)
			{
				continue;
			}

			// Network-driven bodies are controlled by NetworkSystem::InterpolateEntities.
			// Do not let Jolt overwrite their transform or apply simulated velocity,
			// but synchronize the physics body transform so other dynamic bodies collide with it.
			if (rb.IsNetworkDriven)
			{
				if (rb.Handle != kInvalidPhysicsBody)
				{
					world->SetTransform(rb.Handle, transform.Translation, transform.RotationQuat);
				}
				continue;
			}

			if (rb.Type == RigidBodyComponent::BodyType::Dynamic)
			{
				// 1. Apply transform update FIRST (if script changed it)
				if (transform.TransformChanged)
				{
					glm::vec3 currentPos;
					glm::quat currentRot;
					world->GetTransform(rb.Handle, currentPos, currentRot);

					bool posChanged = glm::distance2(transform.Translation, currentPos) > 0.0001f;
					glm::quat targetRot = rb.IsFixedRotation ? currentRot : transform.RotationQuat;

					bool rotChanged = false;
					if (!rb.IsFixedRotation)
					{
						float dot = glm::abs(glm::dot(currentRot, targetRot));
						rotChanged = dot < 0.9999f;
					}

					if (posChanged || rotChanged)
					{
						glm::vec3 preVel = world->GetVelocity(rb.Handle);
						world->SetTransform(rb.Handle, transform.Translation, targetRot);
						world->SetVelocity(rb.Handle, preVel);
					}
					transform.TransformChanged = false;
				}

				// 2. Apply script-requested velocity
				glm::vec3 currentJoltVelocity = world->GetVelocity(rb.Handle);
				glm::vec3 finalVelocity = rb.Velocity;
				if (!rb.VelocityForced && rb.Velocity.y <= kVelocityYThreshold)
				{
					finalVelocity.y = currentJoltVelocity.y;
				}
				rb.VelocityForced = false;
				world->SetVelocity(rb.Handle, finalVelocity);
			}
			else if (rb.Type == RigidBodyComponent::BodyType::Kinematic)
			{
				world->SetTransform(rb.Handle, transform.Translation, transform.RotationQuat);
				world->SetVelocity(rb.Handle, rb.Velocity);
				// NOTE: Do NOT clear TransformChanged here.
				// The script set it via Transform_SetTranslation; the HierarchySystem
				// PostUpdate pass needs it to propagate Translation → WorldTransform.
				continue;
			}
			else if (transform.TransformChanged)
			{
				// Static bodies — just update transform
				world->SetTransform(rb.Handle, transform.Translation, transform.RotationQuat);
				transform.TransformChanged = false;
			}
		}

		int steps = 0;
		while (accumulator >= fixedDt && steps < kMaxStepsPerFrame)
		{
			world->ClearGroundedState();
			world->Step(fixedDt);
			accumulator -= fixedDt;
			stepped = true;
			steps++;
		}

		if (accumulator >= fixedDt)
		{
			accumulator = 0.0f;
		}

		if (stepped)
		{
			UpdateColliders(scene);
		}
	}

	void Physics::UpdateColliders(Scene* scene)
	{
		auto world = GetWorld();
		auto& registry = scene->GetRegistry();
		auto view = registry.view<TransformComponent, RigidBodyComponent>();

		for (auto entity : view)
		{
			auto& transform = view.get<TransformComponent>(entity);
			auto& rb = view.get<RigidBodyComponent>(entity);

			if (rb.Handle == kInvalidPhysicsBody)
			{
				continue;
			}
			if (rb.IsNetworkDriven)
			{
				continue;
			}
			if (rb.Type == RigidBodyComponent::BodyType::Static)
			{
				continue;
			}

			bool isActive = world->IsBodyActive(rb.Handle);

			if (rb.Type == RigidBodyComponent::BodyType::Kinematic)
			{
				// Kinematic: position is controlled by script, but IsGrounded is still needed.
				// Only update IsGrounded for active bodies; sleeping bodies do not move,
				// so their grounded state remains correct from the previous frame.
				if (isActive)
				{
					rb.IsGrounded = world->IsBodyGrounded(rb.Handle);
				}
				continue;
			}

			// Dynamic: read position, velocity and grounded state from Jolt
			// For sleeping bodies - position hasn't changed, IsGrounded is kept from previous frame.
			if (!isActive)
			{
				continue;
			}

			glm::vec3 pos;
			glm::quat rot;
			world->GetTransform(rb.Handle, pos, rot);

			TransformSystem::SetTranslation(transform, pos);
			if (!rb.IsFixedRotation)
			{
				TransformSystem::SetRotationQuat(transform, rot);
			}

			rb.Velocity = world->GetVelocity(rb.Handle);
			rb.IsGrounded = world->IsBodyGrounded(rb.Handle);
		}
	}

	void Physics::ForceSetVelocity(PhysicsBodyHandle handle, const glm::vec3& velocity)
	{
		if (auto world = GetWorld())
		{
			world->SetVelocity(handle, velocity);
		}
	}

	RaycastResult Physics::Raycast(Ray ray)
	{
		if (auto world = GetWorld())
		{
			return world->Raycast(ray.position, ray.direction, kDefaultRaycastDistance);
		}
		return {};
	}

	void Physics::ResetAccumulator(Scene* scene)
	{
		m_Accumulators[scene] = 0.0f;
	}

	void Physics::ClearContext(Scene* scene)
	{
		auto& registry = scene->GetRegistry();

		if (m_World)
		{
			auto view = registry.view<RigidBodyComponent>();
			for (auto entity : view)
			{
				auto& rb = view.get<RigidBodyComponent>(entity);
				if (rb.Handle != kInvalidPhysicsBody)
				{
					m_World->DestroyBody(rb.Handle);
					rb.Handle = kInvalidPhysicsBody;
				}
			}
		}
		m_Accumulators.erase(scene);
		registry.ctx().erase<Physics*>();
	}

} // namespace Chained