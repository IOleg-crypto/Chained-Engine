#include "hierarchy_system.h"
#include "engine/core/profiler.h"
#include "engine/scene/components.h"
#include "engine/scene/systems/transform_system.h"

namespace Chained::Hierarchy
{

	void UpdateWorldTransforms(entt::registry& reg, const std::vector<entt::entity>& roots)
	{
		CH_PROFILE_FUNCTION();

		std::vector<UpdateTask> stack;
		stack.reserve(roots.size());

		// 1. Push root entities to stack
		for (auto entity : roots)
		{
			if (reg.valid(entity) && reg.all_of<TransformComponent>(entity))
			{
				stack.push_back({entity, glm::mat4(1.0f), true});
			}
		}

		// 2. Iterative DFS update with dirty flag propagation
		while (!stack.empty())
		{
			UpdateTask task = stack.back();
			stack.pop_back();

			auto& tc = reg.get<TransformComponent>(task.Entity);

			// A node needs update if it is explicitly dirty OR its parent's world transform changed
			bool needsUpdate = task.ParentChanged || tc.TransformChanged;

			if (needsUpdate)
			{
				tc.WorldTransform = task.ParentTransform * TransformSystem::ComputeLocalMatrix(tc);
				tc.InverseWorldTransform = glm::inverse(tc.WorldTransform);
				tc.TransformChanged = false;
			}

			if (reg.all_of<HierarchyComponent>(task.Entity))
			{
				auto& hc = reg.get<HierarchyComponent>(task.Entity);
				for (auto child : hc.Children)
				{
					if (reg.valid(child) && reg.all_of<TransformComponent>(child))
					{
						stack.push_back({child, tc.WorldTransform, needsUpdate});
					}
				}
			}
		}
	}

} // namespace Chained::Hierarchy
