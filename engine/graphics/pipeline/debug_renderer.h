#ifndef CH_DEBUG_RENDERER_H
#define CH_DEBUG_RENDERER_H

#include "engine/graphics/camera_types.h"
#include "engine/graphics/api/renderer_types.h"
#include "engine/scene/components/render/model_component.h"
#include "engine/scene/scene_settings.h"
#include "engine/graphics/pipeline/scene_renderer_types.h"
#include "engine/core/service.h"
#include "engine/graphics/api/buffer.h"
#include "engine/graphics/api/vertex_array.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

namespace Chained
{

	class Renderer;

	struct LineVertex
	{
		glm::vec3 Position;
		glm::vec4 Color;
	};

	struct LineState
	{
		std::shared_ptr<class VertexBuffer> VBO;
		std::shared_ptr<VertexArray> VAO;
		std::vector<LineVertex> Vertices;
		uint32_t VBOSize = 0;
	};

	struct StaticResources
	{
		std::unique_ptr<Model> UnitCubeModel;
		std::unique_ptr<Model> UnitSphereModel;
		std::unique_ptr<Model> UnitCapsuleModel;
		std::unique_ptr<Model> WireCubeModel;
	};

	class CH_API DebugRenderer : public Service
	{
	public:
		DebugRenderer() = default;
		virtual ~DebugRenderer() override = default;

		void DrawLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color);
		void Flush(Renderer& renderer);
		void DrawMeshWire(const Mesh& mesh, const glm::vec4& color, const glm::mat4& transform, Renderer& renderer,
						  bool useWireframe = true);
		void DrawCubeWires(const glm::mat4& transform, const glm::vec3& size, const glm::vec4& color,
						   Renderer& renderer, bool useWireframe = true);
		void DrawCapsuleWires(const glm::mat4& transform, float radius, float height, const glm::vec4& color,
							  Renderer& renderer, bool useWireframe = true);
		void DrawSphereWires(const glm::mat4& transform, float radius, const glm::vec4& color, Renderer& renderer,
							 bool useWireframe = true);
		void DrawInfiniteGrid(const Camera3D& camera, const GridSettings& grid, Renderer& renderer);

		// High-level debug overlay: sets up pipeline state, draws colliders + grid, flushes lines.
		void RenderDebug(entt::registry& registry, const SceneSettings& settings, const Camera3D& camera,
						 const SceneRenderOptions& options, Renderer& renderer);

		// Draws wireframe/solid collider shapes for all ColliderComponent entities.
		void DrawColliderDebug(entt::registry& registry, const SceneRenderOptions& options, Renderer& renderer);

	protected:
		virtual void Initialize() override;
		virtual void Shutdown() override;

	private:
		LineState m_Lines;
		StaticResources m_Resources;
		std::shared_ptr<VertexArray> m_GridPlaneVAO;
	};

} // namespace Chained

#endif // CH_DEBUG_RENDERER_H
