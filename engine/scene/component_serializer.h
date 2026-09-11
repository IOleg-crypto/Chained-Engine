#ifndef CH_COMPONENT_SERIALIZER_H
#define CH_COMPONENT_SERIALIZER_H

#include "engine/core/service.h"
#include "engine/scene/component_registry.h"
#include <vector>
#include <yaml-cpp/yaml.h>

namespace Chained
{
	namespace ComponentSerializer
	{
		// Serializes all registered components owned by an entity.
		void SerializeAll(YAML::Emitter& out, Entity entity);

		// Deserializes all registered components from YAML.
		void DeserializeAll(Entity entity, YAML::Node node);

		// Copies all registered components from source to destination.
		void CopyAll(Entity source, Entity destination);

		// Serializes the ID component separately.
		void SerializeID(YAML::Emitter& out, Entity entity);
	} // namespace ComponentSerializer

} // namespace Chained

#endif // CH_COMPONENT_SERIALIZER_H
