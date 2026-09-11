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
								const glm::vec3& scale);
		bool BuildBodyDesc(entt::registry& reg, entt::entity e, PhysicsBodyDesc& outDesc);
		void BatchInitializeBodies(entt::registry& reg, IPhysicsWorld* world);
		void Update(entt::registry& reg);
		bool IsStartupComplete(entt::registry& reg, IPhysicsWorld* world);
	} // namespace PhysicsBodySystem
} // namespace Chained

#endif // CH_PHYSICS_BODY_SYSTEM_H
