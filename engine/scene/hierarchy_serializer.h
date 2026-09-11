#ifndef CH_HIERARCHY_SERIALIZER_H
#define CH_HIERARCHY_SERIALIZER_H

#include "engine/scene/scene.h"
#include <yaml-cpp/yaml.h>

namespace Chained
{
	struct HierarchyTask
	{
		Entity entity;
		uint64_t parent = 0;
		std::vector<uint64_t> children;
	};

	namespace HierarchySerializer
	{
		void Serialize(YAML::Emitter& out, Entity entity);
		void DeserializeTask(Entity entity, YAML::Node node, HierarchyTask& outTask);
	} // namespace HierarchySerializer

} // namespace Chained

#endif // CH_HIERARCHY_SERIALIZER_H
