#include "shadow_pass.h"
#include "engine/core/service_locator.h"
#include "engine/graphics/api/graphics_device.h"
#include "engine/graphics/pipeline/renderer.h"
#include "engine/graphics/pipeline/scene_renderer.h"
#include "engine/project/project.h"

namespace Chained
{

	void ShadowPass::Init()
	{
		if (m_Initialized)
		{
			return;
		}

		// Grab the depth-pass shader from the library. Use LoadOrGet so it resolves the
		// path from the config and loads it on demand — LoadEngineResources only eagerly
		// loads the common material shaders (Lighting/Skinned/Unlit/Billboard), not ShadowDepth.
		auto* renderer = ServiceLocator::TryGet<Renderer>();
		if (!renderer)
		{
			return;
		}
		m_DepthShaderAsset = renderer->GetShaderLibrary().LoadOrGet("ShadowDepth");

		// Only mark initialized once the shader is really loaded. If SceneRenderer was
		// constructed before the shader config was parsed, Execute() will retry the load.
		if (m_DepthShaderAsset && m_DepthShaderAsset->GetShader())
		{
			CH_INFO("Shadow depth shader loaded");
			m_Initialized = true;
		}
	}

	void ShadowPass::Execute(const RenderContext& ctx)
	{
		// Always cast shadows from the global environment directional light,
		// unless the project's Render settings have shadows disabled entirely.
		auto project = Project::GetActive();
		if (project && !project->GetConfig().Render.EnableShadows)
		{
			m_HasShadows = false;
			if (!m_DisabledWarningShown)
			{
				CH_INFO("Shadows disabled in project settings");
				m_DisabledWarningShown = true;
			}
			return;
		}
		m_DisabledWarningShown = false;

		const auto& lightingSettings = ctx.Renderer->GetEnvironment().Lighting;
		glm::vec3 lightDir = glm::length(glm::vec3(lightingSettings.Direction)) > 0.0001f
								 ? glm::normalize(glm::vec3(lightingSettings.Direction))
								 : glm::normalize(glm::vec3(0.3f, -0.7f, 0.3f));

		m_HasShadows = true;

		// The shader may not have been available when Init() ran (SceneRenderer can be
		// constructed before the shader config is parsed). Retry the load lazily here.
		if (!m_DepthShaderAsset || !m_DepthShaderAsset->GetShader())
		{
			Init();
		}
		if (!m_DepthShaderAsset || !m_DepthShaderAsset->GetShader())
		{
			return;
		}

		// Read shadow resolution from project settings
		uint32_t shadowRes = 2048;
		if (project)
		{
			shadowRes = (uint32_t)project->GetConfig().Render.ShadowResolution;
			if (shadowRes == 0)
			{
				shadowRes = 2048;
			}
		}
		m_ShadowMapSize = shadowRes;

		// Recreate shadow map FBO if size changed (depth-only)
		if (!m_ShadowMap || m_ShadowMap->GetSpecification().Width != shadowRes)
		{
			FramebufferSpecification shadowFboSpec;
			shadowFboSpec.Width = shadowRes;
			shadowFboSpec.Height = shadowRes;
			shadowFboSpec.Samples = 1;
			shadowFboSpec.DepthOnly = true; // Pure depth texture — no color attachment
			m_ShadowMap = Framebuffer::Create(shadowFboSpec);
		}

		if (!m_ShadowMap || !m_ShadowMap->IsValid())
		{
			return;
		}

		// Build light-space matrix
		constexpr float orthoSize = 80.0f;
		constexpr float nearPlane = 1.0f;
		constexpr float farPlane = 300.0f;

		glm::mat4 lightProjection = glm::ortho(-orthoSize, orthoSize, -orthoSize, orthoSize, nearPlane, farPlane);
		glm::vec3 lightPos = -lightDir * (farPlane * 0.5f);
		glm::mat4 lightView = glm::lookAt(lightPos, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		m_LightSpaceMatrix = lightProjection * lightView;

		// Set shadow bias on renderer
		if (auto* r = ServiceLocator::TryGet<Renderer>())
		{
			r->GetLightingManager().GetShadow().Bias = 0.003f;
		}

		auto* shader = m_DepthShaderAsset->GetShader().get();
		shader->Bind();
		shader->SetMatrix("u_LightSpaceMatrix", m_LightSpaceMatrix);

		// Save current FBO binding and viewport — both must be restored so the
		// subsequent scene passes render at the camera's resolution, not the shadow map's.
		uint32_t previousFBO = GraphicsDevice::Get().GetFramebufferBinding();
		int prevViewport[4] = {0, 0, 0, 0};
		GraphicsDevice::Get().GetViewport(&prevViewport[0], &prevViewport[1], &prevViewport[2], &prevViewport[3]);

		m_ShadowMap->Bind();
		GraphicsDevice::Get().SetViewport(0, 0, shadowRes, shadowRes);
		GraphicsDevice::Get().ClearDepth();

		// Depth bias to prevent shadow acne (surface-shadow self-intersection)
		GraphicsDevice::Get().SetPolygonOffset(true, 2.0f, 1.0f);

		// Render all opaque items into the depth buffer using the depth shader (with GPU instancing).
		const auto& opaqueQueue = ctx.Renderer->GetOpaqueQueue();
		for (size_t i = 0; i < opaqueQueue.size();)
		{
			const auto& firstItem = opaqueQueue[i];

			if (firstItem.Asset && firstItem.BoneMatrices.empty())
			{
				size_t j = i + 1;
				std::vector<glm::mat4> transforms = {firstItem.Transform};
				while (j < opaqueQueue.size() && opaqueQueue[j].Asset == firstItem.Asset &&
					   opaqueQueue[j].BoneMatrices.empty() && opaqueQueue[j].Materials.empty())
				{
					transforms.push_back(opaqueQueue[j].Transform);
					++j;
				}

				if (transforms.size() > 1)
				{
					ctx.Renderer->DrawModelInstanced(firstItem.Asset, transforms, {},
													 m_DepthShaderAsset->GetShader().get(), RenderPassStage::Opaque);
					i = j;
					continue;
				}
			}

			ctx.Renderer->DrawModel(firstItem.Asset, firstItem.Transform, firstItem.BoneMatrices, firstItem.Materials,
									m_DepthShaderAsset->GetShader().get(), {}, RenderPassStage::Opaque);
			++i;
		}

		GraphicsDevice::Get().SetPolygonOffset(false);

		// Restore previous FBO binding and viewport
		m_ShadowMap->Unbind();
		GraphicsDevice::Get().BindFramebuffer(previousFBO);
		GraphicsDevice::Get().SetViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
	}

	void ShadowPass::Shutdown()
	{
		m_ShadowMap.reset();
		m_DepthShaderAsset.reset();
		m_Initialized = false;
	}

} // namespace Chained
