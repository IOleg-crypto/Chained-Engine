#ifndef CH_PROJECT_H
#define CH_PROJECT_H

#include "engine/core/window.h"
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace Chained
{
	class EnvironmentAsset;

	struct PhysicsSettings
	{
		float Gravity = 20.0f;
		float FixedTimestep = 1.0f / 60.0f;
	};

	struct AnimationSettings
	{
		float TargetFPS = 30.0f;
	};

	struct RenderSettings
	{
		int ShadowResolution = 2048;
		bool EnableShadows = true;
		int AntiAliasingSamples = 4; // 0, 2, 4, 8, 16
	};

	struct MeshSettings
	{
		bool ImportMaterials = true;
		bool CalculateTangents = true;
		bool FlipUVs = true;
	};

	struct RuntimeSettings
	{
		bool Fullscreen = false;
		bool ShowStats = true;
		bool EnableConsole = false;
		int TargetFPS = 0; // 0 = Uncapped
	};

	struct AudioSettings
	{
		float MasterVolume = 1.0f;
		float MusicVolume = 1.0f;
		float SFXVolume = 1.0f;
	};

	struct ScriptingSettings
	{
		std::string ModuleName;
		std::filesystem::path ModuleDirectory;
		bool AutoLoad = true;
	};

	enum class Configuration
	{
		Debug = 0,
		Release = 1
	};

	enum class PackMode : uint8_t
	{
		Fast = 0,	  // LZ4 HC — fast compression, larger file
		Balanced = 1, // ZSTD — balanced speed/size
		Max = 2,	  // ZSTD ultra — maximum compression, slowest
		Raw = 3		  // No compression — stored as-is
	};

	struct ExportSettings
	{
		PackMode Mode = PackMode::Balanced;
		float ZipThreshold = 0.05f;
		uint32_t DataVersion = 0;
		uint32_t SplitSizeMB = 0;			// 0 = Single pack, >0 = Max uncompressed MB per chunk
		std::string PackName = "resources"; // Base name for .pack files (without extension)
	};

	struct ProjectConfig
	{
		std::string Name = "Untitled";
		std::string IconPath;
		std::string StartScene;
		std::filesystem::path AssetDirectory = "assets";
		std::filesystem::path ProjectDirectory;
		std::filesystem::path ActiveScenePath;

		PhysicsSettings Physics;
		AnimationSettings Animation;
		RenderSettings Render;
		MeshSettings Mesh;
		WindowProperties Window;
		AudioSettings Audio;
		RuntimeSettings Runtime;
		ScriptingSettings Scripting;
		ExportSettings Export;

		Configuration BuildConfig = Configuration::Debug;
	};

	/// @brief Owns the active project configuration and environment asset.
	///
	/// Projects define the game's settings, asset directories, and scene list.
	/// A single project is active at any time (set via SetActive).
	class Project
	{
	public:
		Project() = default;
		~Project();

		/// @brief Load a project from a .chproject YAML file.
		static std::shared_ptr<Project> Load(const std::filesystem::path& filepath);

		/// @brief Get the currently active project.
		static std::shared_ptr<Project> GetActive();

		/// @brief Set the active project (called by RuntimeLayer or Editor after loading).
		static void SetActive(std::shared_ptr<Project> project);

		// Config access
		const ProjectConfig& GetConfig() const
		{
			return m_Config;
		}
		ProjectConfig& GetConfig()
		{
			return m_Config;
		}

		void SetScripting(const std::string& moduleName, const std::filesystem::path& moduleDir, bool autoLoad = true)
		{
			m_Config.Scripting.ModuleName = moduleName;
			m_Config.Scripting.ModuleDirectory = moduleDir;
			m_Config.Scripting.AutoLoad = autoLoad;
		}

		// Validation setters
		void SetName(const std::string& name)
		{
			m_Config.Name = name;
		}
		void SetStartScene(const std::string& scene)
		{
			m_Config.StartScene = scene;
		}
		void SetIconPath(const std::string& path)
		{
			m_Config.IconPath = path;
		}
		void SetPhysicsGravity(float gravity)
		{
			m_Config.Physics.Gravity = std::max(0.0f, gravity);
		}
		void SetPhysicsFixedTimestep(float timestep)
		{
			m_Config.Physics.FixedTimestep = std::max(0.001f, timestep);
		}
		void SetShadowResolution(int resolution)
		{
			m_Config.Render.ShadowResolution = std::clamp(resolution, 256, 8192);
		}
		void SetAntiAliasingSamples(int samples)
		{
			m_Config.Render.AntiAliasingSamples = std::clamp(samples, 0, 16);
		}
		int GetAntiAliasingSamples() const
		{
			return m_Config.Render.AntiAliasingSamples;
		}
		void SetTargetFPS(int fps)
		{
			m_Config.Runtime.TargetFPS = std::max(0, fps);
		}
		void SetMasterVolume(float volume)
		{
			m_Config.Audio.MasterVolume = std::clamp(volume, 0.0f, 1.0f);
		}
		void SetMusicVolume(float volume)
		{
			m_Config.Audio.MusicVolume = std::clamp(volume, 0.0f, 1.0f);
		}
		void SetSFXVolume(float volume)
		{
			m_Config.Audio.SFXVolume = std::clamp(volume, 0.0f, 1.0f);
		}

		const std::string& GetName() const
		{
			return m_Config.Name;
		}
		const std::filesystem::path& GetActiveScenePath() const
		{
			return m_Config.ActiveScenePath;
		}
		void SetActiveScenePath(const std::filesystem::path& path)
		{
			m_Config.ActiveScenePath = path;
		}
		const std::string& GetStartScene() const
		{
			return m_Config.StartScene;
		}
		Configuration GetBuildConfig() const
		{
			return m_Config.BuildConfig;
		}

		// Path helpers (instance methods — operate on this project's config)
		std::filesystem::path GetAssetDirectory() const;
		std::filesystem::path GetProjectDirectory() const;
		std::filesystem::path GetAssetPath(const std::filesystem::path& relative) const;
		std::string GetRelativePath(const std::filesystem::path& path) const;
		std::filesystem::path GetAbsolutePath(const std::filesystem::path& path) const;
		std::vector<std::string> GetAvailableScenes() const;

		// Pure static utilities (no project state needed)
		static std::filesystem::path NormalizePath(const std::filesystem::path& path);
		static std::optional<std::string> TryMakeRelative(const std::filesystem::path& absolutePath,
														  const std::filesystem::path& basePath);

		// Environment
		std::shared_ptr<EnvironmentAsset> GetEnvironment() const
		{
			return m_Environment;
		}

	private:
		std::string GetRelativePathInternal(const std::filesystem::path& path) const;
		std::filesystem::path GetAbsolutePathInternal(const std::filesystem::path& path) const;

		static std::shared_ptr<Project> s_ActiveProject;
		static std::mutex s_Mutex;

		ProjectConfig m_Config;
		std::shared_ptr<EnvironmentAsset> m_Environment;
	};
} // namespace Chained

#endif // CH_PROJECT_H
