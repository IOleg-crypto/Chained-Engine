#ifndef CH_YAML_HELPERS_H
#define CH_YAML_HELPERS_H

#include <glm/glm.hpp>
#include <yaml-cpp/yaml.h>

namespace Chained
{

	inline YAML::Node Vec4ToYAML(const glm::vec4& v)
	{
		YAML::Node node;
		node.push_back(v.x);
		node.push_back(v.y);
		node.push_back(v.z);
		node.push_back(v.w);
		return node;
	}

	inline glm::vec4 Vec4FromYAML(const YAML::Node& node)
	{
		if (!node || !node.IsSequence() || node.size() < 4)
		{
			return {0, 0, 0, 1};
		}
		return {node[0].as<float>(), node[1].as<float>(), node[2].as<float>(), node[3].as<float>()};
	}

} // namespace Chained

#endif // CH_YAML_HELPERS_H
