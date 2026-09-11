#include "engine/scene/entity.h"
#include "engine/scene/components.h"
#include "engine/physics/physics.h"
#include "engine/core/service_locator.h"

namespace Chained
{

	Entity::Entity(entt::entity handle, entt::registry* registry)
		: m_EntityHandle(handle),
		  m_Registry(registry)
	{
		CH_CORE_ASSERT(m_Registry, "Entity initialized with null registry!");
	}

	bool Entity::IsValid() const
	{
		return m_EntityHandle != entt::null && m_Registry != nullptr && m_Registry->valid(m_EntityHandle);
	}

	// BFS-based recursive destroy: collects all descendants via the hierarchy,
	// then destroys them in reverse order (children before parents) to avoid
	// dangling parent references.
	void Entity::Destroy()
	{
		if (!IsValid())
		{
			return;
		}

		// BFS to collect all entities in the subtree
		std::vector<entt::entity> entitiesToDestroy;
		entitiesToDestroy.push_back(m_EntityHandle);

		size_t current = 0;
		while (current < entitiesToDestroy.size())
		{
			Entity e(entitiesToDestroy[current++], m_Registry);
			if (e.HasComponent<HierarchyComponent>())
			{
				auto& hc = e.GetComponent<HierarchyComponent>();
				for (auto child : hc.Children)
				{
					if (std::find(entitiesToDestroy.begin(), entitiesToDestroy.end(), child) == entitiesToDestroy.end())
					{
						entitiesToDestroy.push_back(child);
					}
				}
			}
		}

		// Destroy in reverse order: children first, then parents
		for (auto it = entitiesToDestroy.rbegin(); it != entitiesToDestroy.rend(); ++it)
		{
			if (m_Registry->valid(*it))
			{
				if (m_Registry->all_of<RigidBodyComponent>(*it))
				{
					auto& rb = m_Registry->get<RigidBodyComponent>(*it);
					if (rb.Handle != kInvalidPhysicsBody)
					{
						Physics* physics = nullptr;
						if (auto* physicsPtr = m_Registry->ctx().find<Physics*>())
						{
							physics = *physicsPtr;
						}
						if (!physics)
						{
							physics = ServiceLocator::TryGet<Physics>();
						}
						if (physics && physics->GetWorld())
						{
							physics->GetWorld()->DestroyBody(rb.Handle);
						}
						rb.Handle = kInvalidPhysicsBody;
					}
				}
				m_Registry->destroy(*it);
			}
		}
	}

	glm::mat4 Entity::GetWorldTransform()
	{
		if (HasComponent<TransformComponent>())
		{
			return GetComponent<TransformComponent>().WorldTransform;
		}
		return glm::mat4(1.0f);
	}

	glm::vec3 Entity::GetWorldPosition()
	{
		return glm::vec3(GetWorldTransform()[3]);
	}

	glm::vec3 Entity::GetForward()
	{
		return glm::normalize(glm::vec3(GetWorldTransform() * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f)));
	}

	glm::vec3 Entity::GetUp()
	{
		return glm::normalize(glm::vec3(GetWorldTransform() * glm::vec4(0.0f, 1.0f, 0.0f, 0.0f)));
	}

	glm::vec3 Entity::GetRight()
	{
		return glm::normalize(glm::vec3(GetWorldTransform() * glm::vec4(1.0f, 0.0f, 0.0f, 0.0f)));
	}

} // namespace Chained
