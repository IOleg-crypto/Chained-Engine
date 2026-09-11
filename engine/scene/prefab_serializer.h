#ifndef CH_PREFAB_SERIALIZER_H
#define CH_PREFAB_SERIALIZER_H

#include "engine/scene/scene.h"
#include <string>
#include <future>

namespace Chained
{
	namespace PrefabSerializer
	{
		bool Serialize(Entity entity, const std::string& filepath);
		Entity Deserialize(Scene* scene, const std::string& filepath);
		std::future<Entity> LoadAsync(Scene* scene, const std::string& filepath);
	} // namespace PrefabSerializer

} // namespace Chained

#endif // CH_PREFAB_SERIALIZER_H
