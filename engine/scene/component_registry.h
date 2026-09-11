#ifndef CH_COMPONENT_REGISTRY_H
#define CH_COMPONENT_REGISTRY_H

#include <algorithm> // std::remove used in RegisterReflective

#include "engine/scene/entity.h"
#include "engine/scene/scene.h"
#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

#include "engine/reflection/reflection.h"
#include "engine/reflection/reflection_rfl.h"

namespace Chained
{
	/**
	 * @brief Metadata required for a component to be integrated into Editor and Serialization.
	 */
	struct ComponentMetadata
	{
		std::string Name;
		std::string SerializationKey;
		const char* Icon = nullptr;

		// UI Drawing callback (Inspector)
		std::function<void(Entity)> DrawUI;

		// Serialization callbacks
		std::function<void(YAML::Emitter&, Entity)> Serialize;
		std::function<void(Entity, YAML::Node)> Deserialize;

		// Lifecycle callbacks
		std::function<bool(Entity)> Has;
		std::function<std::vector<uint64_t>(Scene*)> GetAll;
		std::function<void(Entity)> Add;
		std::function<void(Entity)> Remove;
		std::function<void(Entity, Entity)> Copy;

		// Visibility and filters
		std::string Category = "Other";
		bool Visible = true;
		bool AllowAdd = true;
		bool IsWidget = false;
		bool IsReflective = false;

		// Type-erased reflection caller.
		// Used by the Editor (UI mode) and Serializer (Serialize/Deserialize mode).
		std::function<void(Entity, IPropertyArchiveBase&, ReflectionMode)> ReflectInternal;

		// Notifies EnTT that the component changed (triggers on_update observers).
		// Called by the inspector after a reflected field is edited.
		std::function<void(Entity)> NotifyUpdate;
	};

	/**
	 * @brief Centralized registry for all components (Engine & Game).
	 */
	class ComponentRegistry
	{
	public:
		static void Register(entt::id_type typeId, const ComponentMetadata& metadata);
		static void RegisterEngineComponents();

		static const std::unordered_map<entt::id_type, ComponentMetadata>& GetRegistry()
		{
			return s_Registry;
		}
		static bool Exists(entt::id_type typeId)
		{
			return s_Registry.contains(typeId);
		}
		static const ComponentMetadata& GetMetadata(::entt::id_type typeId)
		{
			auto it = s_Registry.find(typeId);
			CH_CORE_ASSERT(it != s_Registry.end(), "ComponentRegistry::GetMetadata — type not registered!");
			return it->second;
		}
		static ComponentMetadata& GetMetadataMutable(::entt::id_type typeId)
		{
			auto it = s_Registry.find(typeId);
			CH_CORE_ASSERT(it != s_Registry.end(), "ComponentRegistry::GetMetadataMutable — type not registered!");
			return it->second;
		}

		static void SetAllowAdd(::entt::id_type typeId, bool allow)
		{
			s_Registry.at(typeId).AllowAdd = allow;
		}
		static void SetVisible(::entt::id_type typeId, bool visible)
		{
			s_Registry.at(typeId).Visible = visible;
		}
		static void SetIsWidget(::entt::id_type typeId, bool isWidget)
		{
			s_Registry.at(typeId).IsWidget = isWidget;
		}

		// Editor-specific API: allows overriding metadata for editor-specific behavior
		// (undo/redo, custom DrawUI, etc.) without exposing mutable access to the full struct.
		static void OverrideMetadata(::entt::id_type typeId, const ComponentMetadata& override)
		{
			auto it = s_Registry.find(typeId);
			CH_CORE_ASSERT(it != s_Registry.end(), "ComponentRegistry::OverrideMetadata — type not registered!");
			auto& existing = it->second;
			if (!override.Name.empty())
			{
				existing.Name = override.Name;
			}
			if (override.Icon)
			{
				existing.Icon = override.Icon;
			}
			if (override.DrawUI)
			{
				existing.DrawUI = override.DrawUI;
			}
			if (override.Add)
			{
				existing.Add = override.Add;
			}
			if (override.Remove)
			{
				existing.Remove = override.Remove;
			}
			if (override.Serialize)
			{
				existing.Serialize = override.Serialize;
			}
			if (override.Deserialize)
			{
				existing.Deserialize = override.Deserialize;
			}
			if (override.Copy)
			{
				existing.Copy = override.Copy;
			}
			if (override.NotifyUpdate)
			{
				existing.NotifyUpdate = override.NotifyUpdate;
			}
			existing.IsWidget = override.IsWidget;
			existing.Category = override.Category;
			existing.AllowAdd = override.AllowAdd;
		}

		/**
		 * @brief Registers a component that implements 'void Reflect(Properties& props)'.
		 * Automatically generates UI and Serialization handlers.
		 *
		 * SerializationKey strips spaces from the display name before appending "Component"
		 * so that multi-word names like "Rigid Body" produce the correct YAML key
		 * "RigidBodyComponent" rather than "Rigid BodyComponent".
		 */
		template <typename T>
		static void RegisterReflective(const std::string& name, const char* icon = nullptr,
									   const std::string& category = "Game")
		{
			ComponentMetadata metadata;
			metadata.Name = name;
			// Strip spaces: "Rigid Body" -> "RigidBodyComponent"
			std::string keyBase = name;
			keyBase.erase(std::remove(keyBase.begin(), keyBase.end(), ' '), keyBase.end());
			metadata.SerializationKey = keyBase + "Component";
			metadata.Icon = icon;
			metadata.Category = category;

			metadata.Has = [](Entity e) { return e.HasComponent<T>(); };
			metadata.NotifyUpdate = [](Entity e) {
				if (e.HasComponent<T>())
				{
					e.GetRegistry().template patch<T>(e, [](T&) {});
				}
			};
			metadata.GetAll = [](class Scene* s) {
				std::vector<uint64_t> ids;
				for (auto ent : s->GetRegistry().view<T>())
				{
					ids.push_back(static_cast<uint64_t>(ent));
				}
				return ids;
			};
			metadata.Add = [](Entity e) {
				if (!e.HasComponent<T>())
				{
					e.AddComponent<T>();
				}
			};
			metadata.Remove = [](Entity e) {
				if (e.HasComponent<T>())
				{
					e.RemoveComponent<T>();
				}
			};
			metadata.Copy = [](Entity src, Entity dst) {
				if (src.HasComponent<T>())
				{
					dst.AddOrReplaceComponent<T>(src.GetComponent<T>());
				}
			};

			metadata.IsReflective = true;
			metadata.ReflectInternal = [](Entity e, IPropertyArchiveBase& archive, ReflectionMode mode) {
				if (mode == ReflectionMode::Deserialize)
				{
					auto& comp = e.AddOrReplaceComponent<T>();
					GenericProperties props(archive);
					if constexpr (is_rfl_component<T>::value)
					{
						ReflectFromRfl(comp, props);
					}
					else
					{
						comp.Reflect(props);
					}
					// Fire on_update so observer-driven systems (e.g. PrimitiveSystem)
					// can react to the fully-populated component after deserialization.
					// on_construct fired above with default data; on_update carries the real data.
					e.GetRegistry().template patch<T>(e, [](T&) {});
				}
				else if (e.HasComponent<T>())
				{
					GenericProperties props(archive);
					auto& comp = e.GetComponent<T>();
					if constexpr (is_rfl_component<T>::value)
					{
						ReflectFromRfl(comp, props);
					}
					else
					{
						comp.Reflect(props);
					}
				}
			};

			Register(entt::type_hash<T>::value(), metadata);
		}

	private:
		static std::unordered_map<entt::id_type, ComponentMetadata> s_Registry;
	};

/**
 * @brief Macro for automatic static registration of components.
 * Use this in your component's header file, after the closing namespace brace.
 */
#define CH_CONCAT_(a, b) a##b
#define CH_CONCAT(a, b) CH_CONCAT_(a, b)
#define CH_REGISTER_COMPONENT(type, name, icon, category)                                                              \
	static bool CH_CONCAT(s_CompReg_, __LINE__) = []() {                                                               \
		::Chained::ComponentRegistry::RegisterReflective<type>(name, icon, category);                                  \
		return true;                                                                                                   \
	}()

} // namespace Chained

#endif // CH_COMPONENT_REGISTRY_H
