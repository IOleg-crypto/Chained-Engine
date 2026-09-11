#include "scene_renderer.h"
#include "engine/assets/asset.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/types/model_asset.h"
#include "engine/assets/types/shader_asset.h"
#include "engine/assets/types/texture_asset.h"
#include "engine/core/service_locator.h"
#include "engine/graphics/api/graphics_device.h"
#include "engine/graphics/pipeline/debug_renderer.h"
#include "engine/graphics/pipeline/frustum.h"
#include "engine/graphics/pipeline/shader_uniform_utils.h"
#include "engine/graphics/pipeline/renderer.h"
#include "engine/graphics/pipeline/lighting_manager.h"
#include "engine/graphics/pipeline/material_manager.h"
#include "engine/scene/components.h"
#include "engine/scene/entity.h"
#include "engine/graphics/nametag_system.h"
#include "engine/core/platform.h"

#include "engine/graphics/pipeline/passes/composite_pass.h"
#include "engine/graphics/pipeline/passes/geometry_pass.h"
#include "engine/graphics/pipeline/passes/shadow_pass.h"
#include "engine/graphics/pipeline/passes/skybox_pass.h"

namespace Chained
{

	static glm::vec4 ColorToVec4(const Color& c)
	{
		return {c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f};
	}

	SceneRenderer::SceneRenderer()
		: m_Collector(m_MaterialManager)
	{
		AddPass(std::make_unique<ShadowPass>());
		AddPass(std::make_unique<SkyboxPass>());
		AddPass(std::make_unique<GeometryPass>());
		AddPass(std::make_unique<CompositePass>());
	}

	void SceneRenderer::AddPass(std::unique_ptr<IRenderPass> pass)
	{
		pass->Init();
		m_RenderPasses.push_back(std::move(pass));
	}

	std::optional<Camera3D> SceneRenderer::GetActiveCamera(entt::registry& reg)
	{
		auto view = reg.view<CameraComponent, TransformComponent>();
		for (auto entity : view)
		{
			auto [camera, transform] = view.get<CameraComponent, TransformComponent>(entity);
			if (camera.Primary)
			{
				return camera.Camera.GetCamera3D(transform.WorldTransform);
			}
		}
		return std::nullopt;
	}

	Entity SceneRenderer::GetPrimaryCameraEntity(entt::registry& reg, entt::registry* registryPtr)
	{
		auto view = reg.view<CameraComponent>();
		for (auto entity : view)
		{
			if (view.get<CameraComponent>(entity).Primary)
			{
				return {entity, registryPtr};
			}
		}
		return {};
	}

	void SceneRenderer::RenderScene(entt::registry& registry, const SceneSettings& settings, const Camera3D& camera,
									const SceneRenderOptions& options)
	{
		CH_PROFILE_FUNCTION();

		auto* renderer = ServiceLocator::TryGet<Renderer>();
		if (!renderer)
		{
			return;
		}

		GraphicsDevice::Get().EnableDepthTest();

		auto environment = options.EnvironmentOverride ? options.EnvironmentOverride : settings.Environment;

		if (environment)
		{
			const auto& envSettings = environment->GetSettings();
			renderer->GetLightingManager().ApplyEnvironment(envSettings);
			m_CurrentEnv = envSettings;
		}

		renderer->UpdateTime(Timestep(Platform::GetTime()));

		m_CurrentStats = {};
		m_CurrentStats.EntityCount = (uint32_t)registry.storage<entt::entity>().size();

		glm::mat4 view = camera.ViewMatrix;
		glm::mat4 proj = camera.ProjectionMatrix;
		Frustum frustum = FromMatrix(proj * view);

		// Prepare lights (delegated to LightingManager via Renderer)
		{
			auto& lighting = renderer->GetLightingManager().GetLighting();
			for (auto& l : lighting.Lights)
			{
				l.enabled = 0;
			}
			lighting.LightCount = 0;
			lighting.LightsDirty = true;

			int lightCount = 0;
			auto lightView = registry.view<LightComponent>();
			for (auto entity : lightView)
			{
				if (lightCount >= LightingData::MaxLights)
				{
					break;
				}

				auto& light = lightView.get<LightComponent>(entity);
				glm::vec3 worldPos = Entity(entity, &registry).GetWorldPosition();
				if (light.Type != LightType::Directional && !IsSphereVisible(frustum, worldPos, light.Radius))
				{
					continue;
				}

				RenderLight rl;
				rl.position = worldPos;
				rl.color = ColorToVec4(light.LightColor);
				rl.intensity = light.Intensity;
				rl.radius = light.Radius;
				rl.lightType = (int)light.Type;
				rl.direction = Entity(entity, &registry).GetForward();
				rl.innerCutoff = light.InnerCutoff;
				rl.outerCutoff = light.OuterCutoff;
				rl.enabled = 1;

				lighting.Lights[lightCount++] = rl;
			}
			lighting.LightsDirty = true;
			lighting.LightCount = lightCount;
		}

		renderer->BeginScene(camera);

		// Collect entities
		m_Collector.Collect(registry, frustum, camera.Position);

		// Sort opaque queue front-to-back for early-Z rejection and group by shader/model
		auto& opaqueQueue = m_Collector.GetOpaqueQueue();
		std::sort(opaqueQueue.begin(), opaqueQueue.end(), [](const auto& a, const auto& b) {
			if (a.ShaderOverride != b.ShaderOverride)
			{
				return a.ShaderOverride < b.ShaderOverride;
			}
			if (a.Asset != b.Asset)
			{
				return a.Asset < b.Asset;
			}
			return a.Distance < b.Distance;
		});

		// Sort transparent queue back-to-front once for all passes
		auto& transparentQueue = m_Collector.GetTransparentQueue();
		std::sort(transparentQueue.begin(), transparentQueue.end(),
				  [](const auto& a, const auto& b) { return a.Distance > b.Distance; });

		for (auto& pass : m_RenderPasses)
		{
			RenderContext ctx{registry, settings, camera, options, this};
			pass->Execute(ctx);

			if (pass->GetName() == "ShadowPass")
			{
				auto* shadowPass = static_cast<ShadowPass*>(pass.get());
				uint32_t shadowTexID = 0;
				if (shadowPass->HasShadows() && shadowPass->GetShadowMap())
				{
					shadowTexID = shadowPass->GetShadowMap()->GetDepthAttachmentRendererID();
				}
				renderer->GetLightingManager().SetShadowState(shadowPass->HasShadows(), shadowTexID,
															  shadowPass->GetLightSpaceMatrix(), 0.003f);
			}
		}

		RenderSprites(registry, camera);

		// Debug overlay — delegated to DebugRenderer
		if (auto* dbg = ServiceLocator::TryGet<DebugRenderer>())
		{
			dbg->RenderDebug(registry, settings, camera, options, *renderer);
		}

		NametagSystem::DrawNametags(registry, camera);

		renderer->EndScene();

		if (auto* inst = Instrumentor::TryGet())
		{
			inst->UpdateStats(m_CurrentStats);
		}
	}

	void SceneRenderer::RenderSprites(entt::registry& registry, const Camera3D& camera)
	{
		CH_PROFILE_FUNCTION();
		auto* renderer = ServiceLocator::TryGet<Renderer>();
		if (!renderer)
		{
			return;
		}
		auto view = registry.view<TransformComponent, SpriteComponent>();

		struct SpriteEntry
		{
			entt::entity Entity;
			int ZOrder;
		};

		std::vector<SpriteEntry> sortedSprites;
		for (auto entity : view)
		{
			sortedSprites.push_back(SpriteEntry{entity, registry.get<SpriteComponent>(entity).ZOrder});
		}

		std::sort(sortedSprites.begin(), sortedSprites.end(), [](const SpriteEntry& a, const SpriteEntry& b) {
			if (a.ZOrder != b.ZOrder)
			{
				return a.ZOrder < b.ZOrder;
			}
			return a.Entity < b.Entity;
		});

		for (const auto& entry : sortedSprites)
		{
			auto& transform = registry.get<TransformComponent>(entry.Entity);
			auto& sprite = registry.get<SpriteComponent>(entry.Entity);

			if (sprite.TexturePath.empty())
			{
				continue;
			}

			auto* am = ServiceLocator::TryGet<AssetManager>();
			auto textureAsset = am ? am->Get<TextureAsset>(sprite.TexturePath) : nullptr;
			if (textureAsset && textureAsset->IsReady() && textureAsset->GetTexture())
			{
				renderer->DrawSprite(textureAsset->GetTexture()->GetNativeHandle(), transform.WorldTransform,
									 ColorToVec4(sprite.Tint), sprite.FlipX, sprite.FlipY);
			}
		}
	}

	void SceneRenderer::DrawAnimatedEntities(const std::vector<AnimatedEntry>& animatedEntries)
	{
		for (const auto& entry : animatedEntries)
		{
			std::vector<glm::mat4> boneMatrices;
			if (entry.Animation.CurrentAnimationIndex >= 0 && entry.Asset)
			{
				boneMatrices =
					entry.Asset->GetBoneMatrices(entry.Animation.CurrentAnimationIndex, entry.Animation.CurrentFrame);
			}
			DrawModel(entry.Asset, entry.WorldTransform, boneMatrices, {}, entry.ShaderOverride, entry.CustomUniforms);
		}
	}

	void SceneRenderer::DrawModel(Chained::ModelAsset* modelAsset, const glm::mat4& transform,
								  const std::vector<glm::mat4>& boneMatrices,
								  const std::vector<Chained::Material>& materials, Chained::Shader* shaderOverride,
								  const std::vector<Chained::ShaderUniform>& shaderUniformOverrides,
								  Chained::RenderPassStage pass)
	{
		auto* renderer = ServiceLocator::TryGet<Renderer>();
		if (!renderer)
		{
			return;
		}

		if (!modelAsset || modelAsset->GetState() != Chained::AssetState::Ready)
		{
			return;
		}

		const auto& model = modelAsset->GetModel();
		Shader* uniformsShader = nullptr;
		bool uniformsUseSkinning = false;
		for (const auto& inst : modelAsset->GetInstances())
		{
			int i = inst.meshIndex;
			if (i < 0 || i >= (int)model.Meshes.size())
			{
				continue;
			}

			const auto& mesh = model.Meshes[i];

			Material material = m_MaterialManager.Resolve(i, model, materials, modelAsset);

			bool isTransparent = material.Transparent || material.AlbedoColor.a < 0.99f;
			if (pass == RenderPassStage::Opaque && isTransparent)
			{
				continue;
			}
			if (pass == RenderPassStage::Transparent && !isTransparent)
			{
				continue;
			}

			bool useSkinning = mesh.HasSkinning && !boneMatrices.empty();
			std::string fallbackName = useSkinning ? "Skinned" : "Lighting";
			Shader* activeShader = shaderOverride;
			if (!activeShader)
			{
				auto fallbackAsset = renderer->GetShaderLibrary().Exists(fallbackName)
										 ? renderer->GetShaderLibrary().Get(fallbackName)
										 : nullptr;
				if (fallbackAsset)
				{
					activeShader = fallbackAsset->GetShader().get();
				}
			}

			if (!activeShader)
			{
				continue;
			}

			m_CurrentStats.DrawCalls++;
			m_CurrentStats.MeshCount++;

			if (activeShader != uniformsShader || useSkinning != uniformsUseSkinning)
			{
				BindShaderUniforms(activeShader, useSkinning ? boneMatrices : std::vector<glm::mat4>{},
								   shaderUniformOverrides);
				uniformsShader = activeShader;
				uniformsUseSkinning = useSkinning;
			}
			if (!shaderOverride)
			{
				m_MaterialManager.Bind(activeShader, material, i, model);
			}

			uint32_t originalID = material.ShaderID;
			material.ShaderID = activeShader->GetNativeHandle();

			activeShader->Bind();
			renderer->DrawMesh(mesh, material, transform * inst.localTransform);
			material.ShaderID = originalID;
		}
	}

	void SceneRenderer::DrawModelInstanced(ModelAsset* modelAsset, const std::vector<glm::mat4>& transforms,
										   const std::vector<Material>& materials, Shader* shaderOverride,
										   RenderPassStage pass)
	{
		auto* renderer = ServiceLocator::TryGet<Renderer>();
		if (!renderer || !modelAsset || modelAsset->GetState() != AssetState::Ready || transforms.empty())
		{
			return;
		}

		if (transforms.size() == 1)
		{
			DrawModel(modelAsset, transforms[0], {}, materials, shaderOverride, {}, pass);
			return;
		}

		const auto& model = modelAsset->GetModel();
		for (const auto& inst : modelAsset->GetInstances())
		{
			int i = inst.meshIndex;
			if (i < 0 || i >= (int)model.Meshes.size())
			{
				continue;
			}

			const auto& mesh = model.Meshes[i];

			Material material = m_MaterialManager.Resolve(i, model, materials, modelAsset);

			bool isTransparent = material.Transparent || material.AlbedoColor.a < 0.99f;
			if (pass == RenderPassStage::Opaque && isTransparent)
			{
				continue;
			}
			if (pass == RenderPassStage::Transparent && !isTransparent)
			{
				continue;
			}

			std::string fallbackName = "Lighting";
			Shader* activeShader = shaderOverride;
			if (!activeShader)
			{
				auto fallbackAsset = renderer->GetShaderLibrary().Exists(fallbackName)
										 ? renderer->GetShaderLibrary().Get(fallbackName)
										 : nullptr;
				if (fallbackAsset)
				{
					activeShader = fallbackAsset->GetShader().get();
				}
			}

			if (!activeShader)
			{
				continue;
			}

			m_CurrentStats.DrawCalls++;
			m_CurrentStats.MeshCount += (uint32_t)transforms.size();

			BindShaderUniforms(activeShader, {}, {});
			if (!shaderOverride)
			{
				m_MaterialManager.Bind(activeShader, material, i, model);
			}

			uint32_t originalID = material.ShaderID;
			material.ShaderID = activeShader->GetNativeHandle();

			// Precompute all instance matrices: worldTransform * localTransform
			std::vector<glm::mat4> instanceTransforms(transforms.size());
			for (size_t t = 0; t < transforms.size(); ++t)
			{
				instanceTransforms[t] = transforms[t] * inst.localTransform;
			}

			renderer->DrawMeshInstanced(mesh, material, instanceTransforms);
			material.ShaderID = originalID;
		}
	}

	void SceneRenderer::BindShaderUniforms(Chained::Shader* shader, const std::vector<glm::mat4>& boneMatrices,
										   const std::vector<Chained::ShaderUniform>& shaderUniformOverrides)
	{
		if (!shader)
		{
			return;
		}

		auto* renderer = ServiceLocator::TryGet<Renderer>();
		if (!renderer)
		{
			return;
		}

		shader->Bind();

		renderer->GetLightingManager().ApplyUniforms(shader, renderer->GetFrame());

		if (!boneMatrices.empty())
		{
			shader->SetMatrices("boneMatrices", boneMatrices.data(), std::min((int)boneMatrices.size(), 128));
		}

		ApplyShaderUniforms(shader, shaderUniformOverrides);
	}

} // namespace Chained
