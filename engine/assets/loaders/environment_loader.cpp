#include "engine/assets/loaders/environment_loader.h"

#include "engine/assets/types/environment_asset.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/service_locator.h"
#include "engine/scene/yaml.h"
#include "yaml-cpp/yaml.h"

namespace Chained
{
	std::shared_ptr<Asset> EnvironmentLoader::Create()
	{
		return std::make_shared<EnvironmentAsset>();
	}

	bool EnvironmentLoader::Load(std::shared_ptr<Asset> asset, const std::string& resolvedPath, std::string* outError)
	{
		std::string content;

		if (auto* am = ServiceLocator::TryGet<AssetManager>())
		{
			content = am->ReadText(resolvedPath);
		}

		// Fallback to disk
		if (content.empty())
		{
			std::filesystem::path fullPath(resolvedPath);
			std::ifstream stream(fullPath);
			if (!stream.is_open())
			{
				CH_CORE_ERROR("EnvironmentLoader: Failed to open environment file: {0}", resolvedPath);
				if (outError)
				{
					*outError = "EnvironmentLoader: failed to open environment file '" + resolvedPath + "'";
				}
				return false;
			}
			std::stringstream ss;
			ss << stream.rdbuf();
			content = ss.str();
		}

		auto envAsset = std::static_pointer_cast<EnvironmentAsset>(asset);

		try
		{
			YAML::Node data = YAML::Load(content);
			if (!data["Environment"])
			{
				if (outError)
				{
					*outError = "EnvironmentLoader: missing Environment node in '" + resolvedPath + "'";
				}
				return false;
			}

			auto envNode = data["Environment"];
			EnvironmentSettings settings;

			// New format: Lighting section
			if (envNode["Lighting"])
			{
				auto lighting = envNode["Lighting"];
				if (lighting["Direction"])
				{
					settings.Lighting.Direction = lighting["Direction"].as<glm::vec3>();
				}
				if (lighting["LightColor"])
				{
					settings.Lighting.LightColor = lighting["LightColor"].as<Color>();
				}
				if (lighting["Ambient"])
				{
					settings.Lighting.Ambient = lighting["Ambient"].as<float>();
				}
				if (lighting["Exposure"])
				{
					settings.Lighting.Exposure = lighting["Exposure"].as<float>();
				}
				if (lighting["Gamma"])
				{
					settings.Lighting.Gamma = lighting["Gamma"].as<float>();
				}
			}
			else
			{
				// Backward compat: old flat field names
				if (envNode["LightDirection"])
				{
					settings.Lighting.Direction = envNode["LightDirection"].as<glm::vec3>();
				}
				if (envNode["LightColor"])
				{
					settings.Lighting.LightColor = envNode["LightColor"].as<Color>();
				}
				if (envNode["AmbientIntensity"])
				{
					settings.Lighting.Ambient = envNode["AmbientIntensity"].as<float>();
				}
			}

			auto skyboxNode = envNode["Skybox"];
			if (skyboxNode)
			{
				if (skyboxNode["TexturePath"])
				{
					settings.Skybox.TexturePath = skyboxNode["TexturePath"].as<std::string>();
				}
				if (skyboxNode["Mode"])
				{
					settings.Skybox.Mode = skyboxNode["Mode"].as<int>();
				}
				for (int i = 0; i < 6; ++i)
				{
					std::string key = "CubeFace" + std::to_string(i);
					if (skyboxNode[key])
					{
						settings.Skybox.CubeFaces[i] = skyboxNode[key].as<std::string>();
					}
				}
				if (skyboxNode["Exposure"])
				{
					settings.Skybox.Exposure = skyboxNode["Exposure"].as<float>();
				}
				if (skyboxNode["Brightness"])
				{
					settings.Skybox.Brightness = skyboxNode["Brightness"].as<float>();
				}
				if (skyboxNode["Contrast"])
				{
					settings.Skybox.Contrast = skyboxNode["Contrast"].as<float>();
				}
			}

			auto fogNode = envNode["Fog"];
			if (fogNode)
			{
				if (fogNode["Enabled"])
				{
					settings.Fog.Enabled = fogNode["Enabled"].as<bool>();
				}
				if (fogNode["Color"])
				{
					settings.Fog.FogColor = fogNode["Color"].as<Color>();
				}
				if (fogNode["Density"])
				{
					settings.Fog.Density = fogNode["Density"].as<float>();
				}
				if (fogNode["Start"])
				{
					settings.Fog.Start = fogNode["Start"].as<float>();
				}
				if (fogNode["End"])
				{
					settings.Fog.End = fogNode["End"].as<float>();
				}
				if (fogNode["HeightFalloff"])
				{
					settings.Fog.HeightFalloff = fogNode["HeightFalloff"].as<float>();
				}
			}

			envAsset->SetSettings(settings);
			return true;
		} catch (const std::exception& e)
		{
			CH_CORE_ERROR("EnvironmentLoader: Failed to parse environment file {0}: {1}", resolvedPath, e.what());
			if (outError)
			{
				*outError = std::string("EnvironmentLoader: failed to parse '") + resolvedPath + "': " + e.what();
			}
			return false;
		} catch (...)
		{
			CH_CORE_ERROR("EnvironmentLoader: Failed to parse environment file {0} with an unknown exception",
						  resolvedPath);
			if (outError)
			{
				*outError =
					std::string("EnvironmentLoader: failed to parse '") + resolvedPath + "' with an unknown exception";
			}
			return false;
		}
	}
} // namespace Chained