#include "engine/platform/dialogs/dialogs.h"
#include "engine/core/service_locator.h"
#include "project_manager.h"
#include "layer.h"
#include "engine/project/project.h"
#include "project/project_serializer.h"
#include "engine/graphics/pipeline/renderer.h"
#include "engine/ui/ui_font_registry.h"
#include "engine/ui/widget_renderer.h"
#include "engine/scene/scene_events.h"
#include "engine/scripting/scriptengine.h"
#include "engine/assets/asset_manager.h"
#include "engine/imgui/imgui_layer.h"
#include <algorithm>
#include <fstream>
#include <string>
#include <utility>
#include <format>
#include "engine/scene/scene_serializer.h"
#include "engine/core/profiler.h"

#if CH_PLATFORM_WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace Chained
{

	static std::filesystem::path FindProjectRoot()
	{
		std::filesystem::path root;
#ifdef PROJECT_ROOT_DIR
		root = PROJECT_ROOT_DIR;
#else
		root = std::filesystem::current_path();
		while (root.has_parent_path() && !std::filesystem::exists(root / "CMakeLists.txt"))
		{
			root = root.parent_path();
		}
#endif
		return root;
	}

	static const std::vector<std::string>& GetSearchSubdirs()
	{
		static const std::vector<std::string> subdirs = {"build/bin", "bin", "out/bin", "cmake-build-debug/bin",
														 "cmake-build-release/bin"};
		return subdirs;
	}

	static std::filesystem::path FindRuntimeExecutable(const std::string& projectName, const std::string& configStr)
	{
		CH_PROFILE_FUNCTION();

		std::filesystem::path root = FindProjectRoot();

		if (!std::filesystem::exists(root))
		{
			CH_CORE_ERROR("FindRuntimeExecutable: Root path not found: {}", root.string());
			return {};
		}

#if CH_PLATFORM_WINDOWS
		const std::string perGameName = projectName + ".exe";
		const std::string fallbackName = "ChainedRuntime.exe";
#else
		const std::string perGameName = projectName;
		const std::string fallbackName = "ChainedRuntime";
#endif

		auto searchFor = [&](const std::string& targetName) -> std::filesystem::path {
			std::filesystem::path currentBin = std::filesystem::current_path() / targetName;
			if (std::filesystem::exists(currentBin))
			{
				return currentBin;
			}

			auto searchSubdirs = GetSearchSubdirs();

			if (std::filesystem::exists(root / "build"))
			{
				for (const auto& entry : std::filesystem::directory_iterator(root / "build"))
				{
					if (entry.is_directory())
					{
						if (std::filesystem::exists(entry.path() / "bin" / targetName))
						{
							searchSubdirs.push_back("build/" + entry.path().filename().string() + "/bin");
						}
					}
				}
			}

			for (const auto& sub : searchSubdirs)
			{
				std::filesystem::path p = root / sub / targetName;
				if (std::filesystem::exists(p))
				{
					CH_CORE_INFO("FindRuntimeExecutable: Found '{}' at: {}", targetName, p.string());
					return p;
				}
			}
			return {};
		};

		auto result = searchFor(perGameName);
		if (!result.empty())
		{
			return result;
		}

		result = searchFor(fallbackName);
		if (!result.empty())
		{
			return result;
		}

		CH_CORE_INFO("FindRuntimeExecutable: Fast path failed, starting scoped recursive search...");
		try
		{
			for (auto it = std::filesystem::recursive_directory_iterator(root);
				 it != std::filesystem::recursive_directory_iterator(); ++it)
			{
				const auto& entry = *it;
				auto filename = entry.path().filename().string();

				if (entry.is_directory())
				{
					if (filename == ".git" || filename == ".cache" || filename == ".idea" || filename == "include" ||
						filename == "engine")
					{
						it.disable_recursion_pending();
						continue;
					}
				}

				if (entry.is_regular_file() && (filename == perGameName || filename == fallbackName))
				{
					CH_CORE_INFO("FindRuntimeExecutable: Deep search found at: {}", entry.path().string());
					return entry.path();
				}
			}
		} catch (const std::exception& e)
		{
			CH_CORE_WARN("FindRuntimeExecutable: Deep search error: {}", e.what());
		}

		return {};
	}

	static std::string ResolveLaunchVariables(std::string str, std::shared_ptr<Project> project)
	{
		CH_PROFILE_FUNCTION();
		if (!project)
		{
			return str;
		}

		std::filesystem::path root = FindProjectRoot();

		std::filesystem::path projectFile = project->GetConfig().ProjectDirectory / (project->GetName() + ".chproject");
		std::string projectPathStr = std::filesystem::absolute(projectFile).string();

		auto replaceAll = [&](const std::string& from, const std::string& to) {
			size_t pos = 0;
			while ((pos = str.find(from, pos)) != std::string::npos)
			{
				str.replace(pos, from.length(), to);
				pos += to.length();
			}
		};

		replaceAll("${ROOT}", std::filesystem::absolute(root).string());
		replaceAll("${PROJECT_FILE}", projectPathStr);

		if (str.find("${BUILD}") != std::string::npos)
		{
			std::string configStr = (project->GetBuildConfig() == Configuration::Release) ? "Release" : "Debug";
			std::filesystem::path exePath = FindRuntimeExecutable(project->GetName(), configStr);
			std::filesystem::path buildPath = exePath.parent_path();

			if (buildPath.empty())
			{
				for (const auto& sub : GetSearchSubdirs())
				{
					if (std::filesystem::exists(root / sub))
					{
						buildPath = root / sub;
						break;
					}
				}
			}
			replaceAll("${BUILD}", std::filesystem::absolute(buildPath).string());
		}

		return str;
	}

	EditorProjectManager::EditorProjectManager()
	{
	}

	void EditorProjectManager::NewProject()
	{
		// Simple default: close active project to show Project Browser
		Project::SetActive(nullptr);
	}

	void EditorProjectManager::NewProject(const std::string& name, const std::string& path)
	{
		auto project = std::make_shared<Project>();
		project->GetConfig().Name = name;
		project->GetConfig().ProjectDirectory = path;

		auto projectDir = std::filesystem::path(path);
		auto assetsDir = projectDir / "assets";

		// Create standard directory structure
		std::filesystem::create_directories(assetsDir / "scenes");
		std::filesystem::create_directories(assetsDir / "materials");
		std::filesystem::create_directories(assetsDir / "models");
		std::filesystem::create_directories(assetsDir / "audio");
		std::filesystem::create_directories(assetsDir / "animations");
		std::filesystem::create_directories(assetsDir / "shaders");
		std::filesystem::create_directories(assetsDir / "skyboxes");
		std::filesystem::create_directories(assetsDir / "prefab");
		std::filesystem::create_directories(assetsDir / "environments");
		std::filesystem::create_directories(assetsDir / "scripts" / "src");

		// Generate .csproj
		{
			auto scriptsDir = assetsDir / "scripts";
			auto engineRoot = std::filesystem::path(PROJECT_ROOT_DIR);
			auto managedCsproj = engineRoot / "scripting" / "managed" / "Chained.Managed.csproj";
			auto relativeManaged = std::filesystem::relative(managedCsproj, scriptsDir);

			std::string csprojContent =
				"<Project Sdk=\"Microsoft.NET.Sdk\">\n"
				"\n"
				"  <PropertyGroup>\n"
				"    <TargetFramework>net9.0</TargetFramework>\n"
				"    <AssemblyName>" +
				name +
				".Scripts</AssemblyName>\n"
				"    <RootNamespace>" +
				name +
				".Scripts</RootNamespace>\n"
				"    <ImplicitUsings>disable</ImplicitUsings>\n"
				"    <Nullable>enable</Nullable>\n"
				"    <AllowUnsafeBlocks>true</AllowUnsafeBlocks>\n"
				"    <OutputPath>../bin</OutputPath>\n"
				"    <AppendTargetFrameworkToOutputPath>false</AppendTargetFrameworkToOutputPath>\n"
				"    <CopyLocalLockFileAssemblies>true</CopyLocalLockFileAssemblies>\n"
				"    <EnableDefaultCompileItems>false</EnableDefaultCompileItems>\n"
				"  </PropertyGroup>\n"
				"\n"
				"  <ItemGroup>\n"
				"    <Compile Include=\"src/**/*.cs\" />\n"
				"  </ItemGroup>\n"
				"\n"
				"  <ItemGroup>\n"
				"    <ProjectReference Include=\"" +
				relativeManaged.string() +
				"\" />\n"
				"  </ItemGroup>\n"
				"\n"
				"</Project>\n";

			std::ofstream csprojOut(scriptsDir / (name + ".Scripts.csproj"));
			if (csprojOut.is_open())
			{
				csprojOut << csprojContent;
			}
			else
			{
				CH_CORE_ERROR("NewProject: Failed to create .csproj file '{}'",
							  (scriptsDir / (name + ".Scripts.csproj")).string());
			}
		}

		// ── CMake scaffolding ──────────────────────────────────────────────
		// Create CMakeLists.txt at project root (enables standalone builds;
		// place the project folder under `game/` for auto-discovery by the
		// root CMakeLists.txt).
		{
			std::filesystem::path cmakeListsPath = projectDir / "CMakeLists.txt";
			std::ofstream cmakeOut(cmakeListsPath);
			if (cmakeOut.is_open())
			{
				std::string gameDir = std::filesystem::path(path).filename().string();
				cmakeOut << "chained_add_game(" + name + "\n"
						 << "    PROJECT_GAME " << gameDir << "\n"
						 << "    CSHARP_PROJECT \"assets/scripts/" << name << ".Scripts.csproj\"\n"
						 << ")\n";
				cmakeOut.close();
			}
			else
			{
				CH_CORE_ERROR("NewProject: Failed to create CMakeLists.txt '{}'", cmakeListsPath.string());
			}
		}

		// Create src/main.cpp entry point
		{
			std::filesystem::path srcDir = projectDir / "src";
			std::filesystem::create_directories(srcDir);
			std::filesystem::path mainPath = srcDir / "main.cpp";
			std::ofstream mainOut(mainPath);
			if (mainOut.is_open())
			{
				mainOut << "#include \"engine/app/entry_point.h\"\n"
						<< "#include \"engine/core/platform.h\"\n"
						<< "#include \"engine/runtime/runtime_layer.h\"\n"
						<< "\n"
						<< "namespace Chained\n"
						<< "{\n"
						<< "Application* CreateApplication(ApplicationCommandLineArgs args)\n"
						<< "{\n"
						<< "    ApplicationSpecification spec;\n"
						<< "    spec.Name = \"" << name << "\";\n"
						<< "    spec.CommandLineArgs = args;\n"
						<< "    spec.EnableScripting = true;\n"
						<< "    spec.EngineRoot = Platform::GetExecutableDirectory();\n"
						<< "    spec.WorkingDirectory = Platform::GetExecutableDirectory().string();\n"
						<< "\n"
						<< "    // Resolve project path: first CLI arg or default\n"
						<< "    std::filesystem::path projectPath;\n"
						<< "    for (int i = 0; i < args.Count; ++i)\n"
						<< "    {\n"
						<< "        std::string arg = args.Args[i];\n"
						<< "        if (arg.ends_with(\".chproject\"))\n"
						<< "        {\n"
						<< "            projectPath = arg;\n"
						<< "            break;\n"
						<< "        }\n"
						<< "    }\n"
						<< "\n"
						<< "    if (projectPath.empty() || !std::filesystem::exists(projectPath))\n"
						<< "    {\n"
						<< "        projectPath = std::filesystem::path(spec.WorkingDirectory) / (spec.Name + "
						   "\".chproject\");\n"
						<< "    }\n"
						<< "\n"
						<< "    auto* app = new Application(spec);\n"
						<< "    app->PushLayer(std::make_unique<RuntimeLayer>(projectPath.string()));\n"
						<< "    return app;\n"
						<< "}\n"
						<< "} // namespace Chained\n";
				mainOut.close();
			}
		}

		// Generate starter script
		{
			auto scriptsDir = assetsDir / "scripts";
			std::string scriptContent = "using Chained;\n"
										"\n"
										"namespace " +
										name +
										".Scripts\n"
										"{\n"
										"    public class Starter : Script\n"
										"    {\n"
										"        public override void OnCreate()\n"
										"        {\n"
										"        }\n"
										"\n"
										"        public override void OnUpdate(float dt)\n"
										"        {\n"
										"        }\n"
										"    }\n"
										"}\n";

			std::ofstream scriptOut(scriptsDir / "src" / "Starter.cs");
			if (scriptOut.is_open())
			{
				scriptOut << scriptContent;
			}
			else
			{
				CH_CORE_ERROR("NewProject: Failed to create Starter.cs file '{}'",
							  (scriptsDir / "src" / "Starter.cs").string());
			}
		}

		// Create starter scene
		{
			auto scenePath = assetsDir / "scenes" / "untitled.chscene";
			std::ofstream sceneOut(scenePath);
			if (sceneOut.is_open())
			{
				sceneOut << "Scene: Untitled\n"
						 << "Settings:\n"
						 << "  Name: Untitled\n"
						 << "  Type: 0\n"
						 << "  Background:\n"
						 << "    Mode: 2\n"
						 << "Entities: []\n";
			}
		}

		// Create .gitignore
		{
			auto gitignorePath = projectDir / ".gitignore";
			std::ofstream gitignoreOut(gitignorePath);
			if (gitignoreOut.is_open())
			{
				gitignoreOut << "build/\n"
							 << "*.exe\n"
							 << "*.dll\n"
							 << "*.pdb\n"
							 << "*.obj\n"
							 << "assets/bin/\n"
							 << "assets/scripts/bin/\n"
							 << "assets/scripts/obj/\n";
			}
		}

		// Configure scripting settings
		project->SetScripting(name + ".Scripts.dll", "assets/bin");

		EditorProjectSerializer::Serialize(project, (std::filesystem::path(path) / (name + ".chproject")));

		Project::SetActive(project);

		ProjectOpenedEvent e((std::filesystem::path(path) / (name + ".chproject")).string());
		Application::Get().OnEvent(e);
	}

	void EditorProjectManager::OpenProject()
	{
		std::vector<DialogFilter> filters = {{"Chained Project", "chproject"}};
		auto result = Chained::Dialogs::OpenFile(filters);
		if (result)
		{
			OpenProject(*result);
		}
	}

	void EditorProjectManager::OpenProject(const std::filesystem::path& path)
	{
		auto project = std::make_shared<Project>();
		if (EditorProjectSerializer::Deserialize(project, path))
		{
			m_LastProjectPath = path.string();
			Project::SetActive(project);

			ProjectOpenedEvent e(path.string());
			Application::Get().OnEvent(e);
		}
	}

	void EditorProjectManager::SaveProject()
	{
		auto project = Project::GetActive();
		if (!project)
		{
			return;
		}

		EditorProjectSerializer::Serialize(
			project, (project->GetConfig().ProjectDirectory / (project->GetName() + ".chproject")));
	}

	bool EditorProjectManager::OnProjectOpened(ProjectOpenedEvent& e)
	{
		if (!Project::GetActive())
		{
			return false;
		}

		// This event usually fires mid-ImGui-frame (a button click in the Project
		// Selector). Mutating the font atlas while its fonts are in use crashes
		// (stbtt_InitFont on freed FontData), so defer the heavy work to the next
		// EditorLayer::OnUpdate(), which runs before ImGui::NewFrame().
		m_PendingOpenedProjectPath = e.GetPath();
		return true;
	}

	void EditorProjectManager::ProcessPendingProjectOpen()
	{
		const std::string openedPath = ConsumePendingProjectPath();
		if (openedPath.empty())
		{
			return;
		}

		auto project = Project::GetActive();
		if (project)
		{
			std::filesystem::path resolvedPath = openedPath;
			std::filesystem::path projDir =
				resolvedPath.extension() == ".chproject" ? resolvedPath.parent_path() : resolvedPath;

			auto* assetMgr = ServiceLocator::TryGet<AssetManager>();
			auto* renderer = ServiceLocator::TryGet<Renderer>();
			auto* widgetRenderer = ServiceLocator::TryGet<WidgetRenderer>();

			if (!assetMgr || !renderer)
			{
				CH_CORE_ERROR("ProjectManager: AssetManager or Renderer not available");
				return;
			}

			assetMgr->SetProjectDirectory(project->GetConfig().ProjectDirectory);
			assetMgr->SetAssetDirectory(project->GetConfig().ProjectDirectory / project->GetConfig().AssetDirectory);

			// Load engine shaders and resources
			renderer->LoadEngineResources();
			// Rebuild font atlas with both editor fonts and project fonts.
			// Must be done in one pass: Clear → add editor fonts → add project fonts → Build().
			// Calling Build() twice (once per font group) crashes because ImGui frees
			// font file data after the first Build(), making a second Build() invalid.
			EditorLayer::Get().ReloadEditorFonts();

			m_LastProjectPath = openedPath;

			// Track in recent projects list (move to front, cap at 10)
			auto& config = EditorLayer::Get().GetConfig();
			auto& recents = config.RecentProjects;
			recents.erase(std::remove(recents.begin(), recents.end(), m_LastProjectPath), recents.end());
			recents.insert(recents.begin(), m_LastProjectPath);
			if (recents.size() > 10)
			{
				recents.resize(10);
			}

			EditorLayer::Get().SaveConfig();

			// Auto-load script assembly if configured
			if (auto* scriptEngine = ServiceLocator::TryGet<ScriptEngine>())
			{
				scriptEngine->TryAutoLoad(project->GetConfig());
			}

			// Auto-load scene if available
			std::filesystem::path sceneToLoad;

			// 1. Try loading ActiveScene
			if (!project->GetActiveScenePath().empty())
			{
				sceneToLoad = project->GetConfig().ProjectDirectory / project->GetActiveScenePath();
			}

			// 2. Fallback to StartScene
			if (sceneToLoad.empty() || !std::filesystem::exists(sceneToLoad))
			{
				if (!project->GetStartScene().empty())
				{
					sceneToLoad = project->GetConfig().ProjectDirectory / project->GetConfig().AssetDirectory /
								  project->GetStartScene();
				}
			}

			// 3. Load the scene if found
			if (!sceneToLoad.empty() && std::filesystem::exists(sceneToLoad))
			{
				CH_CORE_INFO("EditorProjectManager: Auto-loading scene: {}", sceneToLoad.string());
				EditorLayer::Get().GetSceneManager().OpenScene(sceneToLoad);
			}
		}
	}

	const std::string& EditorProjectManager::GetLastProjectPath() const
	{
		return m_LastProjectPath;
	}

	void EditorProjectManager::RestoreLastProjectPath(const std::string& path)
	{
		m_LastProjectPath = path;
	}

	std::string EditorProjectManager::ConsumePendingProjectPath()
	{
		return std::exchange(m_PendingOpenedProjectPath, {});
	}

	void EditorProjectManager::LaunchStandalone(std::shared_ptr<Scene> editorScene)
	{
		CH_PROFILE_FUNCTION();
		auto project = Project::GetActive();
		if (!project)
		{
			CH_CORE_ERROR("LaunchStandalone: No active project to launch!");
			return;
		}

		auto& config = project->GetConfig();
		std::string sceneArgument;

		if (editorScene)
		{
			std::filesystem::path scenePath = editorScene->GetSettings().ScenePath;
			if (scenePath.empty())
			{
				scenePath = config.ActiveScenePath;
			}

			if (!scenePath.empty())
			{
				if (!editorScene->GetSettings().ScenePath.empty())
				{
					SceneSerializer serializer(editorScene.get());
					if (!serializer.Serialize(editorScene->GetSettings().ScenePath))
					{
						CH_CORE_ERROR("LaunchStandalone: Failed to save current editor scene before launching.");
						return;
					}
				}

				if (scenePath.is_relative())
				{
					scenePath = project->GetAssetPath(scenePath);
				}

				scenePath = std::filesystem::absolute(scenePath);
				project->SetActiveScenePath(project->GetRelativePath(scenePath));
				sceneArgument = std::format(" --scene \"{}\"", scenePath.string());
			}
		}

		std::string configStr = (config.BuildConfig == Configuration::Release) ? "Release" : "Debug";
		std::string runtimePath = FindRuntimeExecutable(config.Name, configStr).string();

		std::filesystem::path projectFile = project->GetConfig().ProjectDirectory / (project->GetName() + ".chproject");
		std::string arguments = std::format("\"{}\"", std::filesystem::absolute(projectFile).string());

		if (!sceneArgument.empty())
		{
			arguments += sceneArgument;
		}

		if (runtimePath.empty() || !std::filesystem::exists(runtimePath))
		{
			CH_CORE_WARN("LaunchStandalone: Runtime binary not found at '{}'. Searching heuristic...", runtimePath);
			runtimePath = FindRuntimeExecutable(config.Name, configStr).string();

			if (runtimePath.empty() || !std::filesystem::exists(runtimePath))
			{
				CH_CORE_ERROR(
					"LaunchStandalone: Runtime executable not found! Searched for '{}.exe' and 'ChainedRuntime.exe'.",
					config.Name);
				return;
			}
		}

		if (!std::filesystem::exists(projectFile))
		{
			CH_CORE_ERROR("LaunchStandalone: Project file not found: {}",
						  std::filesystem::absolute(projectFile).string());
			return;
		}

#if CH_PLATFORM_WINDOWS
		std::string normalizedRuntime = runtimePath;
		std::replace(normalizedRuntime.begin(), normalizedRuntime.end(), '/', '\\');

		std::string normalizedArgs = arguments;
		std::replace(normalizedArgs.begin(), normalizedArgs.end(), '/', '\\');

		CH_CORE_INFO("LaunchStandalone: Executing via ShellExecute: {} {}", normalizedRuntime, normalizedArgs);

		// Provide the executable's directory as the working directory so it doesn't inherit the editor's CWD
		std::string exeDir = std::filesystem::path(normalizedRuntime).parent_path().string();

		std::wstring wExeDir(exeDir.begin(), exeDir.end());
		HINSTANCE result =
			ShellExecuteW(NULL, L"open", std::wstring(normalizedRuntime.begin(), normalizedRuntime.end()).c_str(),
						  std::wstring(normalizedArgs.begin(), normalizedArgs.end()).c_str(), wExeDir.c_str(), SW_SHOW);
		if ((uintptr_t)result <= 32)
		{
			DWORD err = GetLastError();
			CH_CORE_ERROR("LaunchStandalone: ShellExecute failed with code {} (Win32 error: {})", (uintptr_t)result,
						  err);
		}
#else
		pid_t pid = fork();
		if (pid == 0)
		{
			execl(runtimePath.c_str(), runtimePath.c_str(), arguments.c_str(), nullptr);
			_exit(127);
		}
		else if (pid < 0)
		{
			CH_CORE_ERROR("LaunchStandalone: fork() failed");
		}
#endif
	}

} // namespace Chained
