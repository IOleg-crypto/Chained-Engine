#ifndef CH_ENTITY_COLLECTOR_H
#define CH_ENTITY_COLLECTOR_H

#include "engine/graphics/pipeline/scene_renderer_types.h"
#include "engine/graphics/pipeline/frustum.h"
#include "engine/graphics/pipeline/material_manager.h"
#include <entt/entt.hpp>
#include <vector>
#include <glm/glm.hpp>

namespace Chained
{

	class Renderer;

	/// @brief Collects visible entities from the scene, frustum-culls them,
	/// resolves materials, and splits into opaque/transparent render queues.
	/// Extracted from SceneRenderer to separate entity gathering from draw dispatch.
	class CH_API EntityCollector
	{
	public:
		explicit EntityCollector(MaterialManager& materialManager);
		~EntityCollector() = default;

		void Collect(entt::registry& registry, const Frustum& frustum, const glm::vec3& cameraPos);
		void Clear();

		std::vector<RenderItem>& GetOpaqueQueue()
		{
			return m_OpaqueQueue;
		}
		std::vector<RenderItem>& GetTransparentQueue()
		{
			return m_TransparentQueue;
		}

	private:
		bool EnqueueModelAsset(entt::registry& registry, entt::entity entity, ModelAsset* modelAsset,
							   const glm::mat4& worldTransform, const Frustum& frustum, const glm::vec3& cameraPos);

		MaterialManager& m_MaterialManager;
		std::vector<RenderItem> m_OpaqueQueue;
		std::vector<RenderItem> m_TransparentQueue;
	};

} // namespace Chained

#endif // CH_ENTITY_COLLECTOR_H
