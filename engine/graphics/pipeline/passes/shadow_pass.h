#ifndef SHADOW_PASS_H
#define SHADOW_PASS_H

#include "engine/graphics/pipeline/render_pass.h"
#include "engine/graphics/api/framebuffer.h"
#include "engine/assets/types/shader_asset.h"
#include <glm/glm.hpp>
#include <memory>

namespace Chained
{

	// Renders a shadow map for the primary directional light.
	// The result (m_ShadowMap framebuffer) is available to later passes via GetShadowMap().
	class ShadowPass : public IRenderPass
	{
	public:
		void Init() override;
		void Execute(const RenderContext& ctx) override;
		void Shutdown() override;

		const std::string& GetName() const override
		{
			static std::string name = "ShadowPass";
			return name;
		}

		// Other passes can call this to bind the shadow depth attachment.
		std::shared_ptr<Framebuffer> GetShadowMap() const
		{
			return m_ShadowMap;
		}
		const glm::mat4& GetLightSpaceMatrix() const
		{
			return m_LightSpaceMatrix;
		}
		uint32_t GetShadowMapSize() const
		{
			return m_ShadowMapSize;
		}
		bool HasShadows() const
		{
			return m_HasShadows;
		}

	private:
		std::shared_ptr<Framebuffer> m_ShadowMap;
		std::shared_ptr<ShaderAsset> m_DepthShaderAsset;
		glm::mat4 m_LightSpaceMatrix{1.0f};
		uint32_t m_ShadowMapSize = 2048;
		bool m_HasShadows = false;
		bool m_DisabledWarningShown = false;

		bool m_Initialized = false;
	};

} // namespace Chained
#endif
