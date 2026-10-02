#include "asset_resolution_system.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/types/material_asset.h"
#include "engine/assets/types/model_asset.h"
#include "engine/assets/types/shader_asset.h"
#include "engine/assets/types/texture_asset.h"
#include "engine/core/profiler.h"
#include "engine/core/service_locator.h"
#include "engine/scene/components/render/model_component.h"
#include "engine/scene/components/render/shader_component.h"
#include "engine/scene/components/render/sprite_component.h"

#include <filesystem>

namespace Chained::AssetResolutionSystem
{

	// Common resolution logic shared by Sprite, Shader, and Model resolvers.
	// Checks Handle → UUID → Path in order, returning true when the handle is ready.
	template <typename AssetT, typename HandleT, typename UUIDT, typename PathT>
	static bool ResolveCommon(AssetManager* assets, HandleT& handle, UUIDT& uuid, PathT& path)
	{
		// 1. If path is provided, it is the primary source of truth
		if (!path.empty())
		{
			// Check if existing handle is already valid for this exact path
			if (handle != AssetHandle(0))
			{
				auto currentAsset = assets->Get<AssetT>(handle);
				if (currentAsset && currentAsset->IsReady() && currentAsset->GetPath() == path)
				{
					return true;
				}
			}

			// Load or retrieve by path
			auto asset = assets->Get<AssetT>(path);
			if (asset)
			{
				uuid = asset->GetID();
				if (asset->IsReady())
				{
					handle = asset->GetID();
					return true;
				}
			}
			return false;
		}

		// 2. Fallback to handle if path is empty
		if (handle != AssetHandle(0))
		{
			auto currentAsset = assets->Get<AssetT>(handle);
			if (currentAsset && currentAsset->IsReady())
			{
				path = currentAsset->GetPath();
				return true;
			}
		}

		// 3. Fallback to UUID if path and handle are empty
		if (uuid != 0)
		{
			auto asset = assets->GetByUUID<AssetT>(uuid);
			if (asset && asset->IsReady())
			{
				handle = asset->GetID();
				path = asset->GetPath();
				return true;
			}
		}

		return false;
	}

	static void ResolveSprite(entt::registry& reg, entt::entity e, AssetManager* assets = nullptr)
	{
		auto& sprite = reg.get<SpriteComponent>(e);
		if (!assets)
		{
			assets = ServiceLocator::TryGet<AssetManager>();
		}
		if (!assets)
		{
			return;
		}
		ResolveCommon<TextureAsset>(assets, sprite.TextureHandle, sprite.TextureUUID, sprite.TexturePath);
	}

	static void ResolveShader(entt::registry& reg, entt::entity e, AssetManager* assets = nullptr)
	{
		auto& shader = reg.get<ShaderComponent>(e);
		if (!assets)
		{
			assets = ServiceLocator::TryGet<AssetManager>();
		}
		if (!assets)
		{
			return;
		}
		ResolveCommon<ShaderAsset>(assets, shader.ShaderHandle, shader.ShaderUUID, shader.ShaderPath);
	}

	static void ResolveModel(entt::registry& reg, entt::entity e, AssetManager* assets = nullptr)
	{
		auto& model = reg.get<ModelComponent>(e);
		if (!assets)
		{
			assets = ServiceLocator::TryGet<AssetManager>();
		}
		if (!assets)
		{
			return;
		}

		if (ResolveCommon<ModelAsset>(assets, model.ModelHandle, model.ModelUUID, model.ModelPath))
		{
			return;
		}

		// Model-specific: explicitly clear handle when path has no ready asset
		if (model.ModelPath.empty())
		{
			model.ModelHandle = AssetHandle(0);
			return;
		}

		auto asset = assets->Get<ModelAsset>(model.ModelPath);
		model.ModelHandle = (asset && asset->GetState() == AssetState::Ready) ? asset->GetID() : AssetHandle(0);
	}

	void RegisterObservers(entt::registry& reg)
	{
		reg.on_construct<SpriteComponent>().connect<+[](entt::registry& r, entt::entity e) { ResolveSprite(r, e); }>();
		reg.on_update<SpriteComponent>().connect<+[](entt::registry& r, entt::entity e) { ResolveSprite(r, e); }>();

		reg.on_construct<ShaderComponent>().connect<+[](entt::registry& r, entt::entity e) { ResolveShader(r, e); }>();
		reg.on_update<ShaderComponent>().connect<+[](entt::registry& r, entt::entity e) { ResolveShader(r, e); }>();

		reg.on_construct<ModelComponent>().connect<+[](entt::registry& r, entt::entity e) { ResolveModel(r, e); }>();
		reg.on_update<ModelComponent>().connect<+[](entt::registry& r, entt::entity e) { ResolveModel(r, e); }>();
	}

	void Update(entt::registry& reg)
	{
		CH_PROFILE_FUNCTION();

		auto* assets = ServiceLocator::TryGet<AssetManager>();

		reg.view<SpriteComponent>().each([&](auto entity, auto& sprite) {
			if (sprite.TextureHandle == AssetHandle(0) && (sprite.TextureUUID != 0 || !sprite.TexturePath.empty()))
			{
				ResolveSprite(reg, entity, assets);
			}
		});

		reg.view<ShaderComponent>().each([&](auto entity, auto& shader) {
			if (shader.ShaderHandle == AssetHandle(0) && (shader.ShaderUUID != 0 || !shader.ShaderPath.empty()))
			{
				ResolveShader(reg, entity, assets);
			}
		});

		reg.view<ModelComponent>().each([&](auto entity, auto& model) {
			if (model.ModelHandle == AssetHandle(0) && (model.ModelUUID != 0 || !model.ModelPath.empty()))
			{
				ResolveModel(reg, entity, assets);
			}
		});
	}

} // namespace Chained::AssetResolutionSystem
