#include "viewport_icons.h"
#include "editor/icons.h"
#include "editor/layer.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/types/texture_asset.h"
#include "engine/core/service_locator.h"
#include "engine/graphics/pipeline/debug_renderer.h"
#include "engine/graphics/pipeline/renderer.h"
#include "engine/scene/components.h"
#include "engine/scene/scene.h"
#include <glad/gl.h>
#include <algorithm>
#include <cmath>

namespace Chained
{

	static EditorIcons s_CachedIcons;

	uint32_t ViewportIcons::GetIconHandle(const std::shared_ptr<TextureAsset>& icon)
	{
		if (icon && icon->IsReady())
		{
			auto tex = icon->GetTexture();
			if (tex)
			{
				return tex->GetNativeHandle();
			}
		}
		return 0;
	}

	float ViewportIcons::ComputeIconSize(const glm::vec3& worldPos, const glm::vec3& cameraPos, float minSize,
										 float maxSize, float scale)
	{
		const float distance = glm::distance(worldPos, cameraPos);
		return std::clamp(distance * scale, minSize, maxSize);
	}

	void ViewportIcons::DrawBillboard(const Camera3D& camera, uint32_t textureId, const glm::vec3& worldPos,
									  float iconSize, const glm::vec4& tint)
	{
		if (textureId != 0)
		{
			if (auto* renderer = ServiceLocator::TryGet<Renderer>())
			{
				renderer->DrawBillboard(camera, textureId, worldPos, iconSize, tint);
			}
		}
	}

	void ViewportIcons::RenderLightIcons(entt::registry& registry, const Camera3D& camera, float iconMin, float iconMax,
										 float iconScale)
	{
		const glm::vec3 activeCameraPos = camera.Position;

		auto lightView = registry.view<TransformComponent, LightComponent>();
		for (auto entity : lightView)
		{
			auto [transform, light] = lightView.get<TransformComponent, LightComponent>(entity);
			const glm::vec3 iconPos = glm::vec3(transform.WorldTransform[3]);
			const float iconSize = ComputeIconSize(iconPos, activeCameraPos, iconMin, iconMax, iconScale);

			glm::vec4 lightTint = {light.LightColor.r / 255.0f, light.LightColor.g / 255.0f,
								   light.LightColor.b / 255.0f, 0.95f};

			uint32_t iconTextureId = GetIconHandle(s_CachedIcons.LightIcon);
			DrawBillboard(camera, iconTextureId, iconPos, iconSize, lightTint);

			if (iconTextureId != 0 && light.Type == LightType::Directional)
			{
				glm::vec3 dir = glm::normalize(glm::vec3(transform.WorldTransform[2])) * 0.45f;
				if (auto* debugRenderer = ServiceLocator::TryGet<DebugRenderer>())
				{
					debugRenderer->DrawLine(iconPos, iconPos + dir, lightTint);
				}
			}
			else if (iconTextureId == 0 && light.Type == LightType::Directional)
			{
				glm::vec3 dir = glm::normalize(glm::vec3(transform.WorldTransform[2])) * 0.5f;
				if (auto* debugRenderer = ServiceLocator::TryGet<DebugRenderer>())
				{
					debugRenderer->DrawLine(iconPos, iconPos + dir, lightTint);
				}
			}
		}
	}

	void ViewportIcons::RenderAll(entt::registry& registry, const Camera3D& camera)
	{
		const glm::vec3 activeCameraPos = camera.Position;

		auto tryLoadIcon = [&](const char* path, std::shared_ptr<TextureAsset>& cachedIcon) {
			if (cachedIcon)
			{
				return;
			}
			if (auto* assetManager = ServiceLocator::TryGet<AssetManager>())
			{
				cachedIcon = assetManager->Load<TextureAsset>(path);
			}
		};

		tryLoadIcon("engine/resources/icons/camera_icon.png", s_CachedIcons.CameraIcon);
		tryLoadIcon("engine/resources/icons/light_bulb.png", s_CachedIcons.LightIcon);
		tryLoadIcon("engine/resources/icons/leaf_icon.png", s_CachedIcons.SpawnIcon);
		tryLoadIcon("engine/resources/icons/audio.png", s_CachedIcons.AudioIcon);

		const auto& editorCfg = EditorLayer::Get().GetConfig();
		const float iconMin = editorCfg.IconSizeMin;
		const float iconMax = editorCfg.IconSizeMax;
		const float iconScale = editorCfg.IconSizeScale;

		// Camera icons
		auto cameraView = registry.view<TransformComponent, CameraComponent>();
		for (auto entity : cameraView)
		{
			auto [transform, cameraComp] = cameraView.get<TransformComponent, CameraComponent>(entity);
			const glm::vec3 iconPos = glm::vec3(transform.WorldTransform[3]);
			if (glm::distance(iconPos, activeCameraPos) < 0.25f)
			{
				continue;
			}

			const float iconSize = ComputeIconSize(iconPos, activeCameraPos, iconMin, iconMax, iconScale);
			const glm::vec4 cameraTint = glm::vec4(0.65f, 0.95f, 1.0f, 0.95f);
			DrawBillboard(camera, GetIconHandle(s_CachedIcons.CameraIcon), iconPos, iconSize, cameraTint);
		}

		// Light icons
		RenderLightIcons(registry, camera, iconMin, iconMax, iconScale);

		// Spawn icons
		auto spawnView = registry.view<TransformComponent, SpawnComponent>();
		for (auto entity : spawnView)
		{
			auto [transform, spawn] = spawnView.get<TransformComponent, SpawnComponent>(entity);
			const glm::vec3 iconPos = glm::vec3(transform.WorldTransform[3]);
			const float iconSize = ComputeIconSize(iconPos, activeCameraPos, iconMin, iconMax, iconScale);
			const glm::vec4 spawnTint = {1.0f, 1.0f, 1.0f, 0.95f};
			DrawBillboard(camera, GetIconHandle(s_CachedIcons.SpawnIcon), iconPos, iconSize, spawnTint);
		}

		// Audio icons
		auto audioView = registry.view<TransformComponent, AudioComponent>();
		for (auto entity : audioView)
		{
			auto [transform, audio] = audioView.get<TransformComponent, AudioComponent>(entity);
			const glm::vec3 iconPos = glm::vec3(transform.WorldTransform[3]);
			const float iconSize = ComputeIconSize(iconPos, activeCameraPos, iconMin, iconMax, iconScale);
			const glm::vec4 audioTint = {1.0f, 1.0f, 1.0f, 0.95f};
			DrawBillboard(camera, GetIconHandle(s_CachedIcons.AudioIcon), iconPos, iconSize, audioTint);
		}
	}

} // namespace Chained
