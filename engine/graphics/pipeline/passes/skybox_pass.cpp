#include "skybox_pass.h"
#include "engine/core/service_locator.h"
#include "engine/graphics/pipeline/scene_renderer.h"
#include "engine/graphics/pipeline/renderer.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/types/texture_asset.h"
#include "engine/assets/types/model_asset.h"
#include "engine/graphics/api/texture.h"
#include "types/environment_asset.h"

namespace Chained
{

	void SkyboxPass::Execute(const RenderContext& ctx)
	{
		auto environment = ctx.Settings.Environment;
		if (!environment)
		{
			environment = ctx.Options.EnvironmentOverride;
		}

		if (environment)
		{
			const auto& envSettings = environment->GetSettings();
			const auto& skySettings = envSettings.Skybox;

			if (skySettings.Mode == 3)
			{
				// Mode 3: Six Faces Cubemap
				std::string key;
				bool hasAny = false;
				for (int i = 0; i < 6; ++i)
				{
					key += skySettings.CubeFaces[i] + ";";
					if (!skySettings.CubeFaces[i].empty())
					{
						hasAny = true;
					}
				}

				if (hasAny)
				{
					if (key != m_CachedFacesKey || !m_CachedSixFacesCubemap)
					{
						m_CachedSixFacesCubemap = Texture::CreateCubemapFromFiles(skySettings.CubeFaces);
						m_CachedFacesKey = key;
					}

					if (m_CachedSixFacesCubemap)
					{
						uint32_t texId = m_CachedSixFacesCubemap->GetNativeHandle();
						if (auto* r = ServiceLocator::TryGet<Renderer>())
						{
							r->DrawSkybox(texId, 2, false, skySettings.Exposure, skySettings.Brightness,
										  skySettings.Contrast, ctx.Camera, false);
						}
					}
				}
			}
			else if (!skySettings.TexturePath.empty())
			{
				auto* am = ServiceLocator::TryGet<AssetManager>();
				if (!am)
				{
					return;
				}
				auto textureAsset = am->Get<TextureAsset>(skySettings.TexturePath);
				if (textureAsset && textureAsset->IsReady() && textureAsset->GetTexture())
				{
					int skyboxMode = std::clamp(skySettings.Mode, 0, 2);
					uint32_t texId = textureAsset->GetTexture()->GetNativeHandle();

					// Logic mapped directly from old SceneRenderer implementation
					if (auto* r = ServiceLocator::TryGet<Renderer>())
					{
						r->DrawSkybox(texId, skyboxMode, textureAsset->IsHDR(), skySettings.Exposure,
									  skySettings.Brightness, skySettings.Contrast, ctx.Camera, true);
					}
				}
			}
		}
	}

} // namespace Chained
