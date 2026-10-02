#include "project_serializer.h"
#include "engine/core/log.h"
#include "engine/scene/serialization.h"
#include <fstream>
#include <yaml-cpp/yaml.h>

namespace Chained
{
	using namespace Serialization;

	bool EditorProjectSerializer::Serialize(const std::shared_ptr<Project>& project,
											const std::filesystem::path& filepath)
	{
		const auto& config = project->GetConfig();

		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Project" << YAML::Value;
		{
			out << YAML::BeginMap;
			out << YAML::Key << "Name" << YAML::Value << config.Name;
			out << YAML::Key << "IconPath" << YAML::Value << config.IconPath;
			out << YAML::Key << "StartScene" << YAML::Value << config.StartScene;
			out << YAML::Key << "AssetDirectory" << YAML::Value << config.AssetDirectory.string();

			SerializePath(out, "ActiveScene", config.ActiveScenePath.string());

			out << YAML::Key << "Physics" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Gravity" << YAML::Value << config.Physics.Gravity;
			out << YAML::Key << "FixedTimestep" << YAML::Value << config.Physics.FixedTimestep;
			out << YAML::EndMap;

			out << YAML::Key << "Animation" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "TargetFPS" << YAML::Value << config.Animation.TargetFPS;
			out << YAML::EndMap;

			out << YAML::Key << "Render" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "ShadowResolution" << YAML::Value << config.Render.ShadowResolution;
			out << YAML::Key << "EnableShadows" << YAML::Value << config.Render.EnableShadows;
			out << YAML::Key << "AntiAliasingSamples" << YAML::Value << config.Render.AntiAliasingSamples;
			out << YAML::EndMap;

			out << YAML::Key << "Mesh" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "ImportMaterials" << YAML::Value << config.Mesh.ImportMaterials;
			out << YAML::Key << "CalculateTangents" << YAML::Value << config.Mesh.CalculateTangents;
			out << YAML::Key << "FlipUVs" << YAML::Value << config.Mesh.FlipUVs;
			out << YAML::EndMap;

			out << YAML::Key << "Window" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Width" << YAML::Value << config.Window.Width;
			out << YAML::Key << "Height" << YAML::Value << config.Window.Height;
			out << YAML::Key << "VSync" << YAML::Value << config.Window.VSync;
			out << YAML::EndMap;

			out << YAML::Key << "Audio" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "MasterVolume" << YAML::Value << config.Audio.MasterVolume;
			out << YAML::Key << "MusicVolume" << YAML::Value << config.Audio.MusicVolume;
			out << YAML::Key << "SFXVolume" << YAML::Value << config.Audio.SFXVolume;
			out << YAML::EndMap;

			out << YAML::Key << "Runtime" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Fullscreen" << YAML::Value << config.Runtime.Fullscreen;
			out << YAML::Key << "ShowStats" << YAML::Value << config.Runtime.ShowStats;
			out << YAML::Key << "EnableConsole" << YAML::Value << config.Runtime.EnableConsole;
			out << YAML::Key << "TargetFPS" << YAML::Value << config.Runtime.TargetFPS;
			out << YAML::EndMap;

			out << YAML::Key << "Scripting" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "ModuleName" << YAML::Value << config.Scripting.ModuleName;
			out << YAML::Key << "ModuleDirectory" << YAML::Value << config.Scripting.ModuleDirectory.string();
			out << YAML::Key << "AutoLoad" << YAML::Value << config.Scripting.AutoLoad;
			out << YAML::EndMap;

			out << YAML::Key << "Export" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Mode" << YAML::Value << static_cast<int>(config.Export.Mode);
			out << YAML::Key << "ZipThreshold" << YAML::Value << config.Export.ZipThreshold;
			out << YAML::Key << "DataVersion" << YAML::Value << config.Export.DataVersion;
			out << YAML::Key << "SplitSizeMB" << YAML::Value << config.Export.SplitSizeMB;
			out << YAML::Key << "PackName" << YAML::Value << config.Export.PackName;
			out << YAML::EndMap;

			out << YAML::Key << "BuildConfig" << YAML::Value << static_cast<int>(config.BuildConfig);

			out << YAML::EndMap;
		}
		out << YAML::EndMap;

		std::ofstream fout(filepath);
		if (!fout.is_open())
		{
			CH_CORE_ERROR("Failed to save project file: {}", filepath.string());
			return false;
		}

		fout << out.c_str();
		if (fout.fail())
		{
			CH_CORE_ERROR("Failed to write project file: {}", filepath.string());
			return false;
		}

		return true;
	}

	bool EditorProjectSerializer::Deserialize(const std::shared_ptr<Project>& project,
											  const std::filesystem::path& filepath)
	{
		std::ifstream stream(filepath);
		if (!stream.is_open())
		{
			CH_CORE_ERROR("Failed to open project file: {}", filepath.string());
			return false;
		}

		YAML::Node data;
		try
		{
			data = YAML::Load(stream);
		} catch (const YAML::Exception& e)
		{
			CH_CORE_ERROR("Failed to parse project file: {} ({})", filepath.string(), e.what());
			return false;
		}

		auto projectNode = data["Project"];
		if (!projectNode)
		{
			CH_CORE_ERROR("Missing 'Project' root node in: {}", filepath.string());
			return false;
		}

		auto& config = project->GetConfig();

		DeserializeProperty(projectNode, "Name", config.Name);
		DeserializeProperty(projectNode, "IconPath", config.IconPath);
		DeserializeProperty(projectNode, "StartScene", config.StartScene);
		DeserializePath(projectNode, "AssetDirectory", config.AssetDirectory);

		DeserializePath(projectNode, "ActiveScene", config.ActiveScenePath);

		if (auto physics = projectNode["Physics"])
		{
			DeserializeProperty(physics, "Gravity", config.Physics.Gravity);
			DeserializeProperty(physics, "FixedTimestep", config.Physics.FixedTimestep);
		}

		if (auto anim = projectNode["Animation"])
		{
			DeserializeProperty(anim, "TargetFPS", config.Animation.TargetFPS);
		}

		if (auto render = projectNode["Render"])
		{
			DeserializeProperty(render, "ShadowResolution", config.Render.ShadowResolution);
			DeserializeProperty(render, "EnableShadows", config.Render.EnableShadows);
			DeserializeProperty(render, "AntiAliasingSamples", config.Render.AntiAliasingSamples);
		}

		if (auto mesh = projectNode["Mesh"])
		{
			DeserializeProperty(mesh, "ImportMaterials", config.Mesh.ImportMaterials);
			DeserializeProperty(mesh, "CalculateTangents", config.Mesh.CalculateTangents);
			DeserializeProperty(mesh, "FlipUVs", config.Mesh.FlipUVs);
		}

		if (auto window = projectNode["Window"])
		{
			DeserializeProperty(window, "Width", config.Window.Width);
			DeserializeProperty(window, "Height", config.Window.Height);
			DeserializeProperty(window, "VSync", config.Window.VSync);
		}

		if (auto audio = projectNode["Audio"])
		{
			DeserializeProperty(audio, "MasterVolume", config.Audio.MasterVolume);
			DeserializeProperty(audio, "MusicVolume", config.Audio.MusicVolume);
			DeserializeProperty(audio, "SFXVolume", config.Audio.SFXVolume);
		}

		if (auto runtime = projectNode["Runtime"])
		{
			DeserializeProperty(runtime, "Fullscreen", config.Runtime.Fullscreen);
			DeserializeProperty(runtime, "ShowStats", config.Runtime.ShowStats);
			DeserializeProperty(runtime, "EnableConsole", config.Runtime.EnableConsole);
			DeserializeProperty(runtime, "TargetFPS", config.Runtime.TargetFPS);
		}

		if (auto scripting = projectNode["Scripting"])
		{
			DeserializeProperty(scripting, "ModuleName", config.Scripting.ModuleName);
			DeserializeProperty(scripting, "AutoLoad", config.Scripting.AutoLoad);

			std::string moduleDir;
			DeserializeProperty(scripting, "ModuleDirectory", moduleDir);
			if (!moduleDir.empty())
			{
				config.Scripting.ModuleDirectory = moduleDir;
			}
		}

		if (auto exportNode = projectNode["Export"])
		{
			if (exportNode["Mode"])
			{
				int mode = 0;
				DeserializeProperty(exportNode, "Mode", mode);
				config.Export.Mode = static_cast<PackMode>(mode);
			}
			else if (exportNode["PreferSpeed"])
			{
				bool preferSpeed = false;
				DeserializeProperty(exportNode, "PreferSpeed", preferSpeed);
				config.Export.Mode = preferSpeed ? PackMode::Fast : PackMode::Balanced;
			}
			DeserializeProperty(exportNode, "ZipThreshold", config.Export.ZipThreshold);
			DeserializeProperty(exportNode, "DataVersion", config.Export.DataVersion);
			DeserializeProperty(exportNode, "SplitSizeMB", config.Export.SplitSizeMB);
			DeserializeProperty(exportNode, "PackName", config.Export.PackName);
			if (config.Export.PackName.empty())
			{
				config.Export.PackName = "resources";
			}
		}

		int buildConfig = static_cast<int>(config.BuildConfig);
		DeserializeProperty(projectNode, "BuildConfig", buildConfig);
		config.BuildConfig = static_cast<Configuration>(buildConfig);

		config.ProjectDirectory = filepath.parent_path();

		return true;
	}
} // namespace Chained
