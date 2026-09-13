#include "scriptengine.h"
#include "engine/app/application.h"

#include "engine/common/engine_assert.h"
#include "engine/scene/scene.h"
#include <exception>

namespace Chained
{

	ScriptEngine::ScriptEngine(bool enableScripting)
		: m_EnableScripting(enableScripting)
	{
	}

	ScriptEngine::~ScriptEngine()
	{
		if (m_Host.IsInitialized())
		{
			ScriptEngine::Shutdown();
		}
	}

	void ScriptEngine::Initialize()
	{
		if (!m_EnableScripting)
		{
			CH_CORE_INFO("ScriptEngine: Scripting is disabled via config.");
			return;
		}

		if (m_Host.IsInitialized())
		{
			return;
		}

		CH_CORE_INFO("ScriptEngine: Initializing CoreCLR Host...");

		if (!m_Host.Init())
		{
			CH_CORE_ERROR("ScriptEngine: Failed to initialize ScriptHost.");
			return;
		}

		CH_CORE_INFO("ScriptEngine: CoreCLR host and Glue system initialized successfully.");
	}

	void ScriptEngine::Shutdown()
	{
		if (!m_Host.IsInitialized())
		{
			return;
		}

		CH_CORE_INFO("ScriptEngine: Shutting down CoreCLR...");

		m_Registry.Clear();
		m_Host.Shutdown();

		CH_CORE_INFO("ScriptEngine: ScriptEngine cleanup complete.");
	}

	bool ScriptEngine::LoadAppAssembly(const std::string& path)
	{
		if (path.empty() || !std::filesystem::exists(path))
		{
			return false;
		}

		CH_CORE_INFO("ScriptEngine: Loading app assembly '{}'...", path);

		bool success = m_Host.LoadAppAssembly(path);
		if (success)
		{
			// InternalCalls are already registered to CoreAssembly inside LoadAssembliesTransactional
			m_Registry.Clear();
			m_Registry.Discover(*m_Host.GetAppAssembly(), *m_Host.GetCoreAssembly());
		}
		return success;
	}

	bool ScriptEngine::ReloadAssembly(const std::string& assemblyPath)
	{
		if (assemblyPath.empty() || !std::filesystem::exists(assemblyPath))
		{
			return false;
		}

		CH_CORE_INFO("ScriptEngine: Reloading assembly '{}'...", assemblyPath);

		bool success = m_Host.ReloadAppAssembly(assemblyPath);
		if (success)
		{
			// InternalCalls are already registered to CoreAssembly inside LoadAssembliesTransactional
			m_Registry.Clear();
			m_Registry.Discover(*m_Host.GetAppAssembly(), *m_Host.GetCoreAssembly());
		}
		return success;
	}

	bool ScriptEngine::RequestAssemblyReload(const std::string& assemblyPath, const char* requestSource)
	{
		if (assemblyPath.empty() || !std::filesystem::exists(assemblyPath))
		{
			CH_CORE_WARN("ScriptEngine: Assembly reload request failed - file does not exist: '{}'", assemblyPath);
			return false;
		}

		CH_CORE_INFO("ScriptEngine: Assembly reload requested by {}", requestSource);
		return ReloadAssembly(assemblyPath);
	}

	Coral::Type* ScriptEngine::GetScriptClass(const std::string& name)
	{
		if (name.empty())
		{
			CH_CORE_WARN("ScriptEngine: GetScriptClass called with empty name.");
			return nullptr;
		}
		return m_Registry.GetScriptClass(name);
	}

	std::filesystem::path ScriptEngine::ResolveAssemblyPath(const ScriptingSettings& scripting,
															const std::filesystem::path& projectDir)
	{
		if (scripting.ModuleName.empty())
		{
			return {};
		}

		std::string dllName = scripting.ModuleName;
		if (!dllName.ends_with(".dll"))
		{
			dllName += ".dll";
		}

		// Try ModuleDirectory first (relative to project dir)
		std::filesystem::path dllPath = scripting.ModuleDirectory / dllName;
		if (dllPath.is_relative())
		{
			dllPath = projectDir / dllPath;
		}

		if (std::filesystem::exists(dllPath))
		{
			return dllPath;
		}

		// Try MinGW "lib" prefix
		std::filesystem::path libPath = dllPath.parent_path() / ("lib" + dllName);
		if (std::filesystem::exists(libPath))
		{
			return libPath;
		}

		// Try exe directory (build output)
		std::filesystem::path exeDir = Application::GetExecutableDirectory();
		std::filesystem::path exePath = exeDir / dllName;
		if (std::filesystem::exists(exePath))
		{
			return exePath;
		}

		// Try exe directory with lib prefix
		std::filesystem::path exeLibPath = exeDir / ("lib" + dllName);
		if (std::filesystem::exists(exeLibPath))
		{
			return exeLibPath;
		}

		// Scan exeDir/scripts/*/ for CMake build output layout
		std::filesystem::path scriptsRoot = exeDir / "scripts";
		if (std::filesystem::is_directory(scriptsRoot))
		{
			std::error_code ec;
			for (auto& entry : std::filesystem::directory_iterator(scriptsRoot, ec))
			{
				if (!entry.is_directory())
				{
					continue;
				}
				std::filesystem::path candidate = entry.path() / dllName;
				if (std::filesystem::exists(candidate))
				{
					return candidate;
				}
				candidate = entry.path() / ("lib" + dllName);
				if (std::filesystem::exists(candidate))
				{
					return candidate;
				}
			}
		}

		return {};
	}

	bool ScriptEngine::TryAutoLoad(const ProjectConfig& config)
	{
		if (!config.Scripting.AutoLoad || config.Scripting.ModuleName.empty())
		{
			return false;
		}

		auto dllPath = ResolveAssemblyPath(config.Scripting, config.ProjectDirectory);
		if (dllPath.empty())
		{
			CH_CORE_WARN("ScriptEngine: Script assembly not found for '{}' (module: '{}', dir: '{}').", config.Name,
						 config.Scripting.ModuleName, config.Scripting.ModuleDirectory.string());
			return false;
		}

		SetEnabled(true);
		Initialize();
		if (!LoadAppAssembly(dllPath.string()))
		{
			CH_CORE_ERROR("ScriptEngine: Failed to auto-load script assembly '{}'.", dllPath.string());
			return false;
		}
		CH_CORE_INFO("ScriptEngine: Auto-loaded script assembly '{}'.", dllPath.string());
		return true;
	}

} // namespace Chained