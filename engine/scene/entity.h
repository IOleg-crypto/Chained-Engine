#ifndef CH_ENTITY_H
#define CH_ENTITY_H

#include "engine/common/engine_assert.h"
#include "engine/common/base.h"
#include "engine/common/uuid.h"
#include "engine/scene/components/core/id_component.h"
#include "engine/scene/components/core/tag_component.h"
#include "entt/entt.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace Chained
{

	// Lightweight handle over an entt registry. Entity objects do not own the registry;
	// validity depends on the backing registry still being alive and the handle still existing.
	struct EntityUUIDMap
	{
		std::unordered_map<UUID, entt::entity> Map;
	};

	class Entity
	{
	public:
		Entity() = default;
		// Wraps an existing registry reference.
		Entity(entt::entity handle, entt::registry* registry);
		Entity(const Entity& other) = default;

		// Adds a component if it is not already present and returns the inserted component.
		template <typename T, typename... Args> T& AddOrReplaceComponent(Args&&... args)
		{
			return m_Registry->emplace_or_replace<T>(m_EntityHandle, std::forward<Args>(args)...);
		}

		// Adds a component and asserts that the entity does not already own it.
		template <typename T, typename... Args> T& AddComponent(Args&&... args)
		{
			CH_CORE_ASSERT(!HasComponent<T>(), "Entity already has component!");
			return m_Registry->emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
		}

		// Returns the requested component and asserts if it is missing.
		template <typename T> T& GetComponent()
		{
			CH_CORE_ASSERT(HasComponent<T>(), "Entity does not have component!");
			return m_Registry->get<T>(m_EntityHandle);
		}

		// Returns true when the entity currently owns the requested component.
		template <typename T> bool HasComponent()
		{
			return m_Registry && m_Registry->all_of<T>(m_EntityHandle);
		}

		// Removes the requested component and asserts if it is missing.
		template <typename T> void RemoveComponent()
		{
			CH_CORE_ASSERT(HasComponent<T>(), "Entity does not have component!");
			m_Registry->remove<T>(m_EntityHandle);
		}

		// Applies a patch operation to the component stored on this entity.
		template <typename T, typename... Func> void Patch(Func&&... func)
		{
			m_Registry->patch<T>(m_EntityHandle, std::forward<Func>(func)...);
		}

		// Fast null check. Use IsValid() to confirm the handle still exists in the registry.
		operator bool() const
		{
			return m_EntityHandle != entt::null && m_Registry != nullptr;
		}
		// Returns false if the registry is missing or the entity handle has been destroyed.
		bool IsValid() const;

		// Destroys the entity and any descendants attached through the hierarchy.
		void Destroy();

		operator entt::entity() const
		{
			return m_EntityHandle;
		}
		explicit operator uint32_t() const
		{
			return static_cast<uint32_t>(m_EntityHandle);
		}

		bool operator==(const Entity& other) const
		{
			return m_EntityHandle == other.m_EntityHandle && m_Registry == other.m_Registry;
		}
		bool operator!=(const Entity& other) const
		{
			return !(*this == other);
		}

		/// Returns the underlying EnTT registry for direct access.
		/// @warning Returns mutable reference — prefer component-specific Entity methods.
		/// Direct registry manipulation bypasses Entity lifecycle hooks.
		entt::registry& GetRegistry()
		{
			return *m_Registry;
		}
		const entt::registry& GetRegistry() const
		{
			return *m_Registry;
		}

		entt::registry* GetRegistryPtr()
		{
			return m_Registry;
		}

		UUID GetUUID()
		{
			return GetComponent<IDComponent>().ID;
		}

		const std::string& GetName()
		{
			return GetComponent<TagComponent>().Tag;
		}

		glm::mat4 GetWorldTransform();
		glm::vec3 GetWorldPosition();
		glm::vec3 GetForward();
		glm::vec3 GetUp();
		glm::vec3 GetRight();

	private:
		entt::entity m_EntityHandle{entt::null};
		entt::registry* m_Registry = nullptr;
	};
} // namespace Chained

#endif // CH_ENTITY_H
