#ifndef CH_RENDERER_H
#define CH_RENDERER_H

#include "engine/core/service.h"
#include "engine/common/timestep.h"
#include "engine/graphics/api/renderer_types.h"
#include "engine/graphics/camera_types.h"
#include "engine/graphics/pipeline/renderer_data.h"
#include "engine/graphics/pipeline/lighting_manager.h"
#include "engine/graphics/pipeline/geometry_factory.h"
#include "engine/graphics/pipeline/shader_storage.h"
#include "engine/assets/types/environment_asset.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace Chained
{

	class Shader;
	class ShaderAsset;

	/// @brief Singleton renderer facade that owns GPU resources, frame state, and low-level draw calls.
	///
	/// Manages the OpenGL rendering pipeline: shader compilation, uniform buffer management,
	/// draw call batching, skybox rendering, and post-processing. Accessed globally via ServiceLocator.
	class CH_API Renderer : public Service
	{
	public:
		Renderer();
		virtual ~Renderer() override;

		/// @brief Load engine-level shaders and static resources (unit meshes, fullscreen quad).
		void LoadEngineResources();
		void InitializeResources();

		/// @brief Begin a new render frame with the given camera.
		void BeginScene(const Camera3D& camera);

		/// @brief Flush pending draw calls and finalize the frame.
		void EndScene();

		void Clear(const glm::vec4& color);
		void SetViewport(int x, int y, int width, int height);

		// Low-level Draw calls
		void DrawMesh(const Mesh& mesh, const Material& material, const glm::mat4& transform);
		void DrawMeshInstanced(const Mesh& mesh, const Material& material, const std::vector<glm::mat4>& transforms);
		void DrawSkybox(uint32_t textureId, int skyboxMode, bool isHDR, float exposure, float brightness,
						float contrast, const Camera3D& camera, bool flipped = false);
		void DrawBillboard(const Camera3D& camera, uint32_t textureId, const glm::vec3& position, float size,
						   const glm::vec4& tint);
		void DrawSprite(uint32_t textureId, const glm::mat4& transform, const glm::vec4& tint, bool flipX = false,
						bool flipY = false);
		void ApplyPostProcessing(uint32_t screenTextureId, uint32_t depthTextureId, const Camera3D& camera,
								 ShaderAsset* overrideShader = nullptr,
								 const std::vector<ShaderUniform>& uniforms = {});

		// Lighting (delegated to LightingManager)
		LightingManager& GetLightingManager()
		{
			return m_LightingManager;
		}
		const LightingManager& GetLightingManager() const
		{
			return m_LightingManager;
		}

		// Frame
		void SetDiagnosticMode(float mode);
		void UpdateTime(Timestep time);

		ShaderStorage& GetShaderLibrary()
		{
			return *m_Data->Shaders;
		}
		const FrameState& GetFrame() const
		{
			return m_Data->Frame;
		}

		void SetHeadless(bool headless)
		{
			m_Headless = headless;
		}
		void SetViewportSize(uint32_t width, uint32_t height)
		{
			m_ViewportWidth = width;
			m_ViewportHeight = height;
		}
		uint32_t GetViewportWidth() const
		{
			return m_ViewportWidth;
		}
		uint32_t GetViewportHeight() const
		{
			return m_ViewportHeight;
		}
		bool IsHeadless() const
		{
			return m_Headless;
		}

	protected:
		virtual void Initialize() override;
		virtual void Shutdown() override;
		void Update(Timestep ts);

	private:
		void InitializeSkybox();
		void CleanupSkybox();
		ShaderAsset* BindShader(const std::string& name);

	private:
		std::unique_ptr<RendererData> m_Data;
		LightingManager m_LightingManager;
		GeometryFactory m_GeometryFactory;
		bool m_Headless = false;
		bool m_ResourcesLoaded = false;
		uint32_t m_ViewportWidth = 1280;
		uint32_t m_ViewportHeight = 720;
	};

} // namespace Chained

#endif // CH_RENDERER_H