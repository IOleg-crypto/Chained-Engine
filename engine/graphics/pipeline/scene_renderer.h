#ifndef CH_SCENE_RENDERER_H
#define CH_SCENE_RENDERER_H

#include "engine/scene/scene_settings.h"
#include "engine/core/profiler.h"
#include "engine/scene/entity.h"
#include "engine/graphics/pipeline/render_pass.h"
#include "engine/graphics/pipeline/material_manager.h"
#include "engine/graphics/pipeline/entity_collector.h"
#include <entt/entt.hpp>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include "engine/graphics/api/texture.h"

namespace Chained
{
	struct Frustum;
	class AssetManager;
	class Renderer; // forward declaration — full include in .cpp only
	class Shader;	// forward declaration — full include in .cpp only

	// High-level scene render orchestrator that collects visible entities, draws them, and emits debug overlays.
	class SceneRenderer
	{
	public:
		SceneRenderer();
		~SceneRenderer() = default;

		// Renders the scene using the supplied camera and options.
		// Internally manages Renderer::BeginScene/EndScene — callers should NOT call them separately.
		void RenderScene(entt::registry& registry, const SceneSettings& settings, const Camera3D& camera,
						 const SceneRenderOptions& options);

		// Architectural Helper: Retrieves the primary camera from scene entities.
		static std::optional<Camera3D> GetActiveCamera(entt::registry& registry);
		static Entity GetPrimaryCameraEntity(entt::registry& registry, entt::registry* registryPtr);

		void AddPass(std::unique_ptr<IRenderPass> pass);

		void DrawAnimatedEntities(const std::vector<AnimatedEntry>& animatedEntries);
		void DrawModel(ModelAsset* modelAsset, const glm::mat4& transform,
					   const std::vector<glm::mat4>& boneMatrices = {}, const std::vector<Material>& materials = {},
					   Shader* shaderOverride = nullptr, const std::vector<ShaderUniform>& shaderUniformOverrides = {},
					   RenderPassStage pass = RenderPassStage::Both);
		void DrawModelInstanced(ModelAsset* modelAsset, const std::vector<glm::mat4>& transforms,
								const std::vector<Material>& materials = {}, Shader* shaderOverride = nullptr,
								RenderPassStage pass = RenderPassStage::Both);

		void BindShaderUniforms(Shader* shader, const std::vector<glm::mat4>& boneMatrices,
								const std::vector<ShaderUniform>& shaderUniformOverrides);

		// Expose internals for passes
		std::vector<RenderItem>& GetOpaqueQueue()
		{
			return m_Collector.GetOpaqueQueue();
		}
		std::vector<RenderItem>& GetTransparentQueue()
		{
			return m_Collector.GetTransparentQueue();
		}
		EnvironmentSettings& GetEnvironment()
		{
			return m_CurrentEnv;
		}

		void RenderSprites(entt::registry& registry, const Camera3D& camera);

	private:
		std::vector<std::unique_ptr<IRenderPass>> m_RenderPasses;

		ProfilerStats m_CurrentStats;
		MaterialManager m_MaterialManager;
		EntityCollector m_Collector;

		EnvironmentSettings m_CurrentEnv;

		// Skybox cache
		std::shared_ptr<Texture> m_CachedCubemap;
		std::string m_CachedCubemapPath;
		std::shared_ptr<ModelAsset> m_SkyboxCubeModel;
	};

} // namespace Chained

#endif // CH_SCENE_RENDERER_H
