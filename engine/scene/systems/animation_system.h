#ifndef CH_ANIMATION_SYSTEM_H
#define CH_ANIMATION_SYSTEM_H

#include "engine/common/timestep.h"
#include <entt/entt.hpp>

namespace Chained
{
	class AssetManager;

	namespace AnimationSystem
	{
		void Update(entt::registry& reg, Timestep ts, AssetManager* assets = nullptr);
	}
} // namespace Chained

#endif // CH_ANIMATION_SYSTEM_H
