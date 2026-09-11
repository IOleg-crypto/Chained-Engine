#ifndef CH_ENTITY_COMMANDS_H
#define CH_ENTITY_COMMANDS_H

#include "command.h"
#include "editor/layer.h"
#include "engine/scene/components.h"
#include "engine/scene/scene.h"
#include "engine/scene/component_serializer.h"
#include <yaml-cpp/yaml.h>

namespace Chained
{
	class DestroyEntityCommand : public IEditorCommand
	{
	public:
		DestroyEntityCommand(Entity entity)
			: m_Entity(entity),
			  m_Scene(entity.GetRegistry().ctx().get<Scene*>())
		{
		}

		void Execute() override
		{
			CH_CORE_INFO("Destroying entity via command: {}", m_Entity.GetComponent<TagComponent>().Tag);

			m_UUID = m_Entity.GetUUID();

			// Serialize the entity before destroying
			YAML::Emitter out;
			out << YAML::BeginMap;
			ComponentSerializer::SerializeID(out, m_Entity);
			ComponentSerializer::SerializeAll(out, m_Entity);
			out << YAML::EndMap;
			m_SerializedData = out.c_str();

			if (EditorLayer::Get().GetSelectedEntity() == m_Entity)
			{
				EditorLayer::Get().SetSelectedEntity({});
			}

			m_Scene->DestroyEntity(m_Entity);
		}

		void Undo() override
		{
			CH_CORE_INFO("Undoing DestroyEntity, restoring UUID: {}", m_UUID);
			YAML::Node node = YAML::Load(m_SerializedData);

			std::string name = "Restored Entity";
			auto tagComponent = node["TagComponent"];
			if (tagComponent && tagComponent["Tag"] && tagComponent["Tag"].IsScalar())
			{
				name = tagComponent["Tag"].as<std::string>();
			}

			m_Entity = m_Scene->CreateEntityWithUUID(m_UUID, name);
			ComponentSerializer::DeserializeAll(m_Entity, node);
		}

		std::string GetName() const override
		{
			return "Destroy Entity";
		}

	private:
		Entity m_Entity;
		Scene* m_Scene;
		uint64_t m_UUID;
		std::string m_SerializedData;
	};

	class CreateEntityCommand : public IEditorCommand
	{
	public:
		CreateEntityCommand(Scene* scene, const std::string& name, const std::string& modelPath = "")
			: m_Scene(scene),
			  m_Name(name),
			  m_ModelPath(modelPath)
		{
		}

		void Execute() override
		{
			m_Entity = m_Scene->CreateEntity(m_Name);
			if (!m_ModelPath.empty())
			{
				auto& mc = m_Entity.AddComponent<ModelComponent>();
				mc.ModelPath = m_ModelPath;

				// Attach default collider matching the primitive type
				auto& col = m_Entity.AddComponent<ColliderComponent>();
				if (m_ModelPath.find("Cube") != std::string::npos)
				{
					col.Type = ColliderType::Box;
					col.Size = {1.0f, 1.0f, 1.0f};
				}
				else if (m_ModelPath.find("Sphere") != std::string::npos ||
						 m_ModelPath.find("Hemisphere") != std::string::npos)
				{
					col.Type = ColliderType::Sphere;
					col.Radius = 0.5f;
				}
				else if (m_ModelPath.find("Capsule") != std::string::npos)
				{
					col.Type = ColliderType::Capsule;
					col.Radius = 0.5f;
					col.Height = 1.0f;
				}
				else
				{
					col.Type = ColliderType::Mesh;
					col.ModelPath = m_ModelPath;
				}
			}
		}

		void Undo() override
		{
			if (m_Entity)
			{
				m_Scene->DestroyEntity(m_Entity);
			}
		}

		std::string GetName() const override
		{
			return "Create Entity";
		}

	private:
		Scene* m_Scene;
		std::string m_Name;
		std::string m_ModelPath;
		Entity m_Entity;
	};

	class DuplicateEntityCommand : public IEditorCommand
	{
	public:
		DuplicateEntityCommand(Entity entity)
			: m_SourceEntity(entity),
			  m_Scene(entity.GetRegistry().ctx().get<Scene*>())
		{
		}

		void Execute() override
		{
			m_DuplicateEntity = Entity(m_Scene->CopyEntity(m_SourceEntity), m_Scene->GetRegistryPtr());
		}

		void Undo() override
		{
			if (m_DuplicateEntity)
			{
				m_Scene->DestroyEntity(m_DuplicateEntity);
			}
		}

		std::string GetName() const override
		{
			return "Duplicate Entity";
		}

	private:
		Entity m_SourceEntity;
		Entity m_DuplicateEntity;
		Scene* m_Scene;
	};

	class ParentEntityCommand : public IEditorCommand
	{
	public:
		ParentEntityCommand(Entity entity, Entity newParent, Scene* scene)
			: m_Entity(entity),
			  m_NewParent(newParent),
			  m_Scene(scene)
		{
			if (m_Entity && m_Entity.HasComponent<HierarchyComponent>())
			{
				auto parentID = m_Entity.GetComponent<HierarchyComponent>().Parent;
				if (parentID != entt::null)
				{
					m_OldParent = Entity(parentID, m_Entity.GetRegistryPtr());
				}
			}
		}

		void Execute() override
		{
			SetParent(m_Entity, m_NewParent);
		}

		void Undo() override
		{
			SetParent(m_Entity, m_OldParent);
		}

		std::string GetName() const override
		{
			return "Parent Entity";
		}

	private:
		void SetParent(Entity child, Entity parent)
		{
			if (!child)
			{
				return;
			}

			if (!child.HasComponent<HierarchyComponent>())
			{
				child.AddComponent<HierarchyComponent>();
			}

			auto& hc = child.GetComponent<HierarchyComponent>();

			// Remove from old parent
			if (hc.Parent != entt::null && m_Scene->GetRegistryPtr()->valid(hc.Parent))
			{
				Entity oldParent(hc.Parent, m_Scene->GetRegistryPtr());
				if (oldParent && oldParent.HasComponent<HierarchyComponent>())
				{
					auto& oldPhc = oldParent.GetComponent<HierarchyComponent>();
					auto it = std::find(oldPhc.Children.begin(), oldPhc.Children.end(), (entt::entity)child);
					if (it != oldPhc.Children.end())
					{
						oldPhc.Children.erase(it);
					}
				}
			}

			// Set new parent
			if (parent)
			{
				hc.Parent = (entt::entity)parent;
				if (!parent.HasComponent<HierarchyComponent>())
				{
					parent.AddComponent<HierarchyComponent>();
				}
				parent.GetComponent<HierarchyComponent>().Children.push_back((entt::entity)child);
			}
			else
			{
				hc.Parent = entt::null;
			}
		}

		Entity m_Entity;
		Entity m_NewParent;
		Entity m_OldParent;
		Scene* m_Scene;
	};

} // namespace Chained

#endif // CH_ENTITY_COMMANDS_H
