#ifndef CH_ANIMATION_SYSTEM_H
#define CH_ANIMATION_SYSTEM_H

#include "engine/common/timestep.h"
#include <entt/entt.hpp>

namespace Chained
{
	namespace AnimationSystem
	{
		void Update(entt::registry& reg, Timestep ts);
	}
} // namespace Chained

#endif // CH_ANIMATION_SYSTEM_H
