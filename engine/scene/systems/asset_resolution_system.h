#ifndef CH_ASSET_RESOLUTION_SYSTEM_H
#define CH_ASSET_RESOLUTION_SYSTEM_H

#include <entt/entt.hpp>

namespace Chained
{
	class AssetManager;

	namespace AssetResolutionSystem
	{
		void RegisterObservers(entt::registry& reg);
		void Update(entt::registry& reg, AssetManager* assets = nullptr);
	} // namespace AssetResolutionSystem
} // namespace Chained

#endif // CH_ASSET_RESOLUTION_SYSTEM_H
