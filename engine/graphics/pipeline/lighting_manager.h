#ifndef CH_LIGHTING_MANAGER_H
#define CH_LIGHTING_MANAGER_H

#include "engine/common/base.h"
#include "engine/common/timestep.h"
#include "engine/graphics/pipeline/renderer_data.h"
#include "engine/assets/types/environment_asset.h"
#include <glm/glm.hpp>
#include <cstdint>

namespace Chained
{

	class Shader;

	/// @brief Owns all per-frame lighting and shadow state.
	/// Centralizes light array management, SSBO upload, and uniform binding
	/// so that Renderer and SceneRenderer don't need to know the details.
	class CH_API LightingManager
	{
	public:
		LightingManager() = default;
		~LightingManager() = default;

		/// @brief Initialize GPU resources (SSBO). Call after GraphicsDevice is ready.
		void Initialize();

		/// @brief Release GPU resources.
		void Shutdown();

		// --- Light array management ---

		void Clear();
		void SetLight(int index, const RenderLight& light);
		void SetLightCount(int count);

		/// @brief Apply environment settings (lighting + fog) from a scene/level.
		void ApplyEnvironment(const EnvironmentSettings& settings);
		void SetMainLight(const LightingSettings& settings);

		// --- Shadow state ---

		void SetShadowState(bool enabled, uint32_t mapTextureID, const glm::mat4& lightSpaceMatrix, float bias);

		/// @brief Upload the light SSBO to the GPU. Call once per frame.
		void Upload();

		/// @brief Bind all lighting + shadow + fog uniforms to the given shader.
		/// @param frame  Read-only frame state (camera position, time, diagnostic mode).
		void ApplyUniforms(Shader* shader, const FrameState& frame);

		// --- Accessors ---

		LightingData& GetLighting()
		{
			return m_Lighting;
		}
		const LightingData& GetLighting() const
		{
			return m_Lighting;
		}

		ShadowState& GetShadow()
		{
			return m_Shadow;
		}
		const ShadowState& GetShadow() const
		{
			return m_Shadow;
		}

	private:
		LightingData m_Lighting;
		ShadowState m_Shadow;
	};

} // namespace Chained

#endif // CH_LIGHTING_MANAGER_H
