#include "hierarchy_serializer.h"
#include "components/core/hierarchy_component.h"
#include "components/core/id_component.h"

namespace Chained
{

	void HierarchySerializer::Serialize(YAML::Emitter& out, Entity entity)
	{
		if (entity.HasComponent<HierarchyComponent>())
		{
			auto& hc = entity.GetComponent<HierarchyComponent>();
			out << YAML::Key << "Hierarchy";
			out << YAML::BeginMap;

			uint64_t parentUUID = 0;
			if (hc.Parent != entt::null)
			{
				Entity parent{hc.Parent, &entity.GetRegistry()};
				if (parent.HasComponent<IDComponent>())
				{
					parentUUID = (uint64_t)parent.GetComponent<IDComponent>().ID;
				}
			}
			out << YAML::Key << "Parent" << YAML::Value << parentUUID;

			out << YAML::Key << "Children" << YAML::BeginSeq;
			for (auto childHandle : hc.Children)
			{
				Entity child{childHandle, &entity.GetRegistry()};
				if (child.HasComponent<IDComponent>())
				{
					out << (uint64_t)child.GetComponent<IDComponent>().ID;
				}
			}
			out << YAML::EndSeq;
			out << YAML::EndMap;
		}
	}

	void HierarchySerializer::DeserializeTask(Entity entity, YAML::Node node, HierarchyTask& outTask)
	{
		if (node["Hierarchy"])
		{
			auto h = node["Hierarchy"];
			outTask.entity = entity;
			if (h["Parent"])
			{
				outTask.parent = h["Parent"].as<uint64_t>();
			}
			else
			{
				outTask.parent = 0;
			}

			if (h["Children"] && h["Children"].IsSequence())
			{
				for (auto child : h["Children"])
				{
					outTask.children.push_back(child.as<uint64_t>());
				}
			}
		}
	}

} // namespace Chained
