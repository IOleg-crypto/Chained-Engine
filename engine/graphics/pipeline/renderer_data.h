#ifndef CH_RENDERER_DATA_H
#define CH_RENDERER_DATA_H

#include "engine/assets/types/environment_asset.h"
#include "engine/common/timestep.h"
#include "engine/graphics/api/buffer.h"
#include "engine/graphics/api/renderer_types.h"
#include "engine/graphics/api/storage_buffer.h"
#include "engine/graphics/api/vertex_array.h"
#include "engine/graphics/camera_types.h"
#include "engine/graphics/pipeline/shader_storage.h"
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

namespace Chained
{

	// ---------------------------------------------------------------------------
	// RenderLight — packed light data uploaded per-frame to the SSBO (binding 0)
	// ---------------------------------------------------------------------------
	struct RenderLight
	{
		glm::vec4 color = {1.0f, 1.0f, 1.0f, 1.0f}; // 16 bytes
		glm::vec3 position = {0, 0, 0};				// 12 bytes
		float intensity = 1.0f;						// 4 bytes
		glm::vec3 direction = {0, -1, 0};			// 12 bytes
		float radius = 10.0f;						// 4 bytes
		float innerCutoff = 15.0f;					// 4 bytes
		float outerCutoff = 20.0f;					// 4 bytes
		int lightType = 0;							// 4 bytes
		int enabled = 0;							// 4 bytes
	};

	// ---------------------------------------------------------------------------
	// CameraData — uploaded once per frame to UBO (binding 0)
	// Must match layout in resources/shaders/include/camera.glsl
	// ---------------------------------------------------------------------------
	struct CameraData
	{
		glm::mat4 ViewProjection;
		glm::mat4 Projection;
		glm::mat4 View;
	};

	// ---------------------------------------------------------------------------
	// LightingData — owns the SSBO and all per-frame lighting state
	// ---------------------------------------------------------------------------
	struct LightingData
	{
		static constexpr int MaxLights = 256;

		RenderLight Lights[MaxLights];
		std::shared_ptr<StorageBuffer> LightSSBO;
		bool LightsDirty = true;
		int LightCount = 0;
		LightingSettings CurrentLighting;
		FogSettings CurrentFog;
	};

	// ---------------------------------------------------------------------------
	// SkyboxData — cached skybox geometry and texture handle
	// ---------------------------------------------------------------------------
	struct SkyboxData
	{
		std::unique_ptr<Model> SkyboxCubeModel;
		std::unique_ptr<Model> SkyboxSphereModel;
		std::shared_ptr<Texture> CachedCubemap;
		std::string CachedCubemapPath;
		unsigned int SourceTextureId = 0;
	};

	struct FrameState
	{
		glm::vec3 CameraPosition = {0.0f, 0.0f, 0.0f};
		Timestep Time = 0.0f;
		float DiagnosticMode = 0.0f;
		unsigned int CurrentShaderId = 0;
		glm::mat4 View = glm::mat4(1.0f);
		glm::mat4 Proj = glm::mat4(1.0f);
	};

	// Shadow state — propagated from ShadowPass after Execute
	struct ShadowState
	{
		bool Enabled = false;
		uint32_t MapTextureID = 0;
		glm::mat4 LightSpaceMatrix = glm::mat4(1.0f);
		float Bias = 0.005f;
	};

	// GPU instancing cache
	struct InstancingState
	{
		std::shared_ptr<class StorageBuffer> SSBO;
		uint32_t Capacity = 0;
	};

	// RendererData — composes all sub-states; owned by the Renderer singleton
	// ---------------------------------------------------------------------------
	struct RendererData
	{
		SkyboxData Skybox;
		FrameState Frame;
		InstancingState Instancing;

		std::unique_ptr<ShaderStorage> Shaders;
		std::shared_ptr<UniformBuffer> CameraUBO;

		EnvironmentSettings CurrentEnv;
	};

} // namespace Chained

#endif // CH_RENDERER_DATA_H
