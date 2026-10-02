#ifndef CH_AUDIO_SYSTEM_H
#define CH_AUDIO_SYSTEM_H

#include "engine/common/timestep.h"
#include <entt/entt.hpp>

namespace Chained
{
	namespace AudioSystem
	{
		void Update(entt::registry& reg);
		void OnRuntimeStart(entt::registry& reg);
		void OnRuntimeStop(entt::registry& reg);
	} // namespace AudioSystem
} // namespace Chained

#endif // CH_AUDIO_SYSTEM_H
