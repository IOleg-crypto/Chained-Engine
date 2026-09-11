#include "prefab_serializer.h"
#include "component_serializer.h"
#include "components.h"
#include "engine/scene/yaml.h"
#include "engine/scene/hierarchy_serializer.h"
#include "engine/scene/scene.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/service_locator.h"

namespace Chained
{
	static void SerializeEntityRecursive(YAML::Emitter& out, Entity entity)
	{
		out << YAML::BeginMap;
		ComponentSerializer::SerializeID(out, entity);
		ComponentSerializer::SerializeAll(out, entity);
		out << YAML::EndMap;

		if (entity.HasComponent<HierarchyComponent>())
		{
			auto& hc = entity.GetComponent<HierarchyComponent>();
			for (auto childHandle : hc.Children)
			{
				Entity child = {childHandle, &entity.GetRegistry()};
				if (child)
				{
					SerializeEntityRecursive(out, child);
				}
			}
		}
	}

	bool PrefabSerializer::Serialize(Entity entity, const std::string& filepath)
	{
		if (!entity)
		{
			return false;
		}

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Prefab" << YAML::Value << entity.GetComponent<TagComponent>().Tag;
		out << YAML::Key << "RootEntity" << YAML::Value << (uint64_t)entity.GetUUID();
		out << YAML::Key << "Entities" << YAML::Value << YAML::BeginSeq;

		SerializeEntityRecursive(out, entity);

		out << YAML::EndSeq;
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		if (!fout.is_open())
		{
			CH_CORE_ERROR("PrefabSerializer: Failed to open file for writing '{}'", filepath);
			return false;
		}
		fout << out.c_str();
		if (!fout.good())
		{
			CH_CORE_ERROR("PrefabSerializer: Failed to write to file '{}'", filepath);
			return false;
		}
		return true;
	}

	Entity PrefabSerializer::Deserialize(Scene* scene, const std::string& filepath)
	{
		if (!scene)
		{
			CH_CORE_ERROR("PrefabSerializer::Deserialize: scene is null");
			return {};
		}

		// Direct synchronous deserialization (the actual work)
		YAML::Node prefabRootNode;
		try
		{
			std::string content;

			// Try reading from pack first
			if (auto* am = ServiceLocator::TryGet<AssetManager>())
			{
				if (am->IsPacked())
				{
					auto packData = am->ReadAssetData(filepath);
					if (packData.empty() && filepath.rfind("assets/", 0) != 0)
					{
						packData = am->ReadAssetData("assets/" + filepath);
					}
					if (!packData.empty())
					{
						content.assign(packData.begin(), packData.end());
					}
				}
			}

			// Fallback to disk
			if (content.empty())
			{
				std::ifstream stream;
				std::string resolvedPath = filepath;
				if (auto* am = ServiceLocator::TryGet<AssetManager>())
				{
					resolvedPath = am->ResolvePath(filepath);
				}
				if (!resolvedPath.empty())
				{
					stream.open(resolvedPath);
				}
				if (!stream.is_open())
				{
					stream.open(filepath);
				}
				if (!stream.is_open() && filepath.rfind("assets/", 0) != 0)
				{
					stream.open("assets/" + filepath);
				}
				if (!stream.is_open())
				{
					CH_CORE_ERROR("PrefabSerializer: Failed to load prefab file '{}' (tried: '{}')", filepath,
								  resolvedPath);
					return {};
				}
				std::stringstream ss;
				ss << stream.rdbuf();
				content = ss.str();
			}

			prefabRootNode = YAML::Load(content);
		} catch (...)
		{
			CH_CORE_ERROR("PrefabSerializer: Failed to load prefab file '{}'", filepath);
			return {};
		}

		if (!prefabRootNode["Prefab"] || !prefabRootNode["Entities"])
		{
			CH_CORE_ERROR("PrefabSerializer: Invalid prefab file format '{}'", filepath);
			return {};
		}

		auto entitiesNode = prefabRootNode["Entities"];

		std::unordered_map<uint64_t, Entity> remapTable;
		std::vector<Entity> createdEntities;
		std::vector<HierarchyTask> hierarchyTasks;

		// Step 1: Create all entities first
		for (auto entityNode : entitiesNode)
		{
			if (!entityNode["Entity"])
			{
				continue;
			}

			uint64_t oldUUID = entityNode["Entity"].as<uint64_t>();
			std::string tag = "Entity";
			if (entityNode["TagComponent"] && entityNode["TagComponent"]["Tag"])
			{
				tag = entityNode["TagComponent"]["Tag"].as<std::string>();
			}

			Entity newEntity = scene->CreateEntity(tag);
			remapTable[oldUUID] = newEntity;
			createdEntities.push_back(newEntity);
		}

		// Step 2: Deserialize components and collect hierarchy tasks
		int idx = 0;
		for (auto entityNode : entitiesNode)
		{
			if (idx >= createdEntities.size())
			{
				break;
			}

			Entity entity = createdEntities[idx++];
			ComponentSerializer::DeserializeAll(entity, entityNode);

			if (entity.HasComponent<TransformComponent>())
			{
				auto& tc = entity.GetComponent<TransformComponent>();
				tc.RotationQuat = glm::quat(tc.Rotation);
				tc.PrevRotationQuat = tc.RotationQuat;
				tc.PrevTranslation = tc.Translation;
				tc.PrevScale = tc.Scale;
				tc.TransformChanged = true;
			}

			if (entity.HasComponent<RigidBodyComponent>())
			{
				entity.GetComponent<RigidBodyComponent>().Handle = kInvalidPhysicsBody;
			}

			HierarchyTask task;
			HierarchySerializer::DeserializeTask(entity, entityNode, task);
			if (task.parent != 0 || !task.children.empty())
			{
				hierarchyTasks.push_back(task);
			}
		}

		// Step 3: Reconstruct hierarchy using the remap table
		for (const auto& task : hierarchyTasks)
		{
			Entity entity = task.entity;
			auto& hc = entity.AddOrReplaceComponent<HierarchyComponent>();

			// Fix Parent
			if (task.parent != 0 && remapTable.count(task.parent))
			{
				hc.Parent = (entt::entity)remapTable[task.parent];
			}
			else
			{
				hc.Parent = entt::null;
			}

			// Fix Children
			hc.Children.clear();
			for (uint64_t oldChildID : task.children)
			{
				if (remapTable.count(oldChildID))
				{
					hc.Children.push_back((entt::entity)remapTable[oldChildID]);
				}
			}
		}

		// Step 4: Find and return the root entity
		if (!prefabRootNode["RootEntity"])
		{
			CH_CORE_ERROR("PrefabSerializer: Missing 'RootEntity' in '{}'", filepath);
			return createdEntities.empty() ? Entity{} : createdEntities[0];
		}
		uint64_t rootOldUUID = prefabRootNode["RootEntity"].as<uint64_t>();
		if (remapTable.count(rootOldUUID))
		{
			return remapTable[rootOldUUID];
		}

		return createdEntities.empty() ? Entity{} : createdEntities[0];
	}

	std::future<Entity> PrefabSerializer::LoadAsync(Scene* scene, const std::string& filepath)
	{
		// Launch the deserialization in a background thread using std::thread
		std::promise<Entity> promise;
		std::future<Entity> future = promise.get_future();

		std::thread loader([scene, filepath, &promise]() mutable {
			// Call the synchronous deserialization directly in the background thread
			Entity result = Deserialize(scene, filepath);

			// Set the result via the promise so the main thread can retrieve it
			promise.set_value(result);
		});

		// Detach the thread so it runs independently and delivers the result via the promise
		loader.detach();

		return future;
	}
} // namespace Chained
