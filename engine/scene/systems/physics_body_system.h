#ifndef CH_PHYSICS_BODY_SYSTEM_H
#define CH_PHYSICS_BODY_SYSTEM_H

#include "engine/physics/iphysics_world.h"
#include "engine/scene/components/physics/physics_component.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <unordered_set>

namespace Chained
{
	class IPhysicsWorld;
	class AssetManager;
	class Physics;

	namespace PhysicsBodySystem
	{
		struct WarnState
		{
			std::unordered_set<uint32_t> MissingCollider;
			std::unordered_set<uint32_t> NoModelPath;
			std::unordered_set<uint32_t> RetriedFailedModels;
			std::unordered_set<uint32_t> NoCtx;
			std::unordered_set<uint32_t> BuildFailed;
		};

		void ApplyAutoCalculate(entt::entity entity, entt::registry& registry, ColliderComponent& collider,
								const glm::vec3& scale, AssetManager* am = nullptr);
		bool BuildBodyDesc(entt::registry& reg, entt::entity e, PhysicsBodyDesc& outDesc,
						   AssetManager* assets = nullptr);
		void BatchInitializeBodies(entt::registry& reg, IPhysicsWorld* world, AssetManager* assets = nullptr);
		void Update(entt::registry& reg, Physics* physics = nullptr, AssetManager* assets = nullptr);
		bool IsStartupComplete(entt::registry& reg, IPhysicsWorld* world, AssetManager* assets = nullptr);
	} // namespace PhysicsBodySystem
} // namespace Chained

#endif // CH_PHYSICS_BODY_SYSTEM_H
