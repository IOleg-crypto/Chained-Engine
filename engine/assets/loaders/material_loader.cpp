#include "engine/assets/loaders/material_loader.h"
#include "engine/assets/types/material_asset.h"
#include "engine/assets/types/texture_asset.h"
#include "engine/assets/loaders/yaml_helpers.h"
#include "engine/assets/asset_manager.h"

#include "engine/core/service_locator.h"

namespace Chained
{

	std::shared_ptr<Asset> MaterialLoader::Create()
	{
		return std::make_shared<MaterialAsset>();
	}

	bool MaterialLoader::Load(std::shared_ptr<Asset> asset, const std::string& resolvedPath, std::string* outError)
	{
		auto matAsset = std::static_pointer_cast<MaterialAsset>(asset);

		std::string content;

		if (auto* am = ServiceLocator::TryGet<AssetManager>())
		{
			content = am->ReadText(resolvedPath);
		}

		if (content.empty())
		{
			if (!std::filesystem::exists(resolvedPath))
			{
				if (outError)
				{
					*outError = "MaterialLoader: File not found: " + resolvedPath;
				}
				return false;
			}
			std::ifstream stream(resolvedPath);
			std::stringstream ss;
			ss << stream.rdbuf();
			content = ss.str();
		}

		try
		{
			YAML::Node root = YAML::Load(content);
			if (!root["Material"])
			{
				if (outError)
				{
					*outError = "MaterialLoader: Missing 'Material' key in " + resolvedPath;
				}
				return false;
			}

			YAML::Node mat = root["Material"];
			Material& m = matAsset->GetMaterial();

			if (mat["Name"])
			{
				m.Name = mat["Name"].as<std::string>();
			}
			if (mat["AlbedoColor"])
			{
				m.AlbedoColor = Vec4FromYAML(mat["AlbedoColor"]);
			}
			if (mat["EmissiveColor"])
			{
				m.EmissiveColor = Vec4FromYAML(mat["EmissiveColor"]);
			}
			if (mat["EmissiveIntensity"])
			{
				m.EmissiveIntensity = mat["EmissiveIntensity"].as<float>();
			}
			if (mat["Metalness"])
			{
				m.Metalness = mat["Metalness"].as<float>();
			}
			if (mat["Roughness"])
			{
				m.Roughness = mat["Roughness"].as<float>();
			}
			if (mat["Transparent"])
			{
				m.Transparent = mat["Transparent"].as<bool>();
			}
			if (mat["Alpha"])
			{
				m.Alpha = mat["Alpha"].as<float>();
			}
			if (mat["FlipUV_Y"])
			{
				m.FlipUV_Y = mat["FlipUV_Y"].as<bool>();
			}
			else if (mat["FlipUV"])
			{
				m.FlipUV_Y = mat["FlipUV"].as<bool>();
			}
			if (mat["FlipUV_X"])
			{
				m.FlipUV_X = mat["FlipUV_X"].as<bool>();
			}
			if (mat["UVScale"])
			{
				m.UVScale = Vec2FromYAML(mat["UVScale"], {1.0f, 1.0f});
			}
			if (mat["UVOffset"])
			{
				m.UVOffset = Vec2FromYAML(mat["UVOffset"], {0.0f, 0.0f});
			}
			if (mat["AlbedoMap"])
			{
				m.AlbedoPath = mat["AlbedoMap"].as<std::string>();
			}
			if (mat["NormalMap"])
			{
				m.NormalPath = mat["NormalMap"].as<std::string>();
			}
			if (mat["MetallicRoughnessMap"])
			{
				m.MetallicRoughnessPath = mat["MetallicRoughnessMap"].as<std::string>();
			}
			if (mat["EmissiveMap"])
			{
				m.EmissivePath = mat["EmissiveMap"].as<std::string>();
			}
			if (mat["OcclusionMap"])
			{
				m.OcclusionPath = mat["OcclusionMap"].as<std::string>();
			}

			if (auto* am = ServiceLocator::TryGet<AssetManager>())
			{
				auto loadTex = [&](const std::string& path, std::shared_ptr<Texture>& outTex) {
					if (path.empty() || path.front() == '*')
					{
						return;
					}
					auto texAsset = am->Get<TextureAsset>(path);
					if (texAsset && texAsset->IsReady() && texAsset->GetTexture())
					{
						outTex = texAsset->GetTexture();
					}
				};

				loadTex(m.AlbedoPath, m.AlbedoMap);
				loadTex(m.NormalPath, m.NormalMap);
				loadTex(m.MetallicRoughnessPath, m.MetallicRoughnessMap);
				loadTex(m.EmissivePath, m.EmissiveMap);
				loadTex(m.OcclusionPath, m.OcclusionMap);
			}
		} catch (const YAML::Exception& e)
		{
			if (outError)
			{
				*outError = std::string("MaterialLoader: YAML parse error in ") + resolvedPath + ": " + e.what();
			}
			return false;
		}

		return true;
	}

} // namespace Chained
