#ifndef CH_ASSET_RESOLUTION_SYSTEM_H
#define CH_ASSET_RESOLUTION_SYSTEM_H

#include <entt/entt.hpp>

namespace Chained
{
	namespace AssetResolutionSystem
	{
		void RegisterObservers(entt::registry& reg);
		void Update(entt::registry& reg);
	} // namespace AssetResolutionSystem
} // namespace Chained

#endif // CH_ASSET_RESOLUTION_SYSTEM_H
