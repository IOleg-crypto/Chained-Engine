// runtime_layer.cpp
// Chained Engine — Runtime layer for standalone game execution.
// Handles project loading, scene transitions, scripting lifecycle, and the main runtime loop.

#include "runtime_layer.h"
#include "engine/app/application.h"
#include "engine/assets/asset_manager.h"
#include "engine/audio/audio.h"
#include "engine/common/asset_path.h"
#include "engine/core/events/window_events.h"
#include "engine/core/platform.h"
#include "engine/core/service_locator.h"
#include "engine/core/window.h"
#include "engine/graphics/pipeline/renderer.h"
#include "engine/graphics/pipeline/scene_renderer.h"
#include "engine/ui/ui_font_registry.h"
#include "engine/ui/widget_renderer.h"
#include "engine/imgui/imgui_layer.h"
#include "engine/physics/physics.h"
#include "engine/project/project.h"
#include "engine/scene/scene_events.h"
#include "engine/scene/scene_serializer.h"
#include "engine/networking/network_service.h"
#include "imgui.h"
#include "engine/scene/systems/asset_resolution_system.h"
#include "engine/scripting/scene_scripting_manager.h"
#include "engine/scripting/scriptengine.h"

#include <cctype>
#include <cmath>

namespace
{
	// Scene loading / startup timeouts (seconds)
	constexpr float kLoadingTimeoutSec = 20.0f;
	constexpr float kStartupTimeoutSec = 30.0f;
	constexpr float kBoostUploadsDuration = 5.0f;

	// Loading overlay appearance
	constexpr float kOverlayCursorY = 0.45f;
	constexpr float kDotsAnimSpeed = 2.5f;
	constexpr int kDotsCount = 3;
	constexpr float kOverlayBgDark = 0.02f;
	constexpr float kOverlayBgAlpha = 0.92f;

	// Default sizes
	constexpr float kDefaultUIFontSize = 18.0f;
	constexpr float kDefaultFontSize = 16.0f;
	constexpr uint32_t kDefaultMsaaSamples = 4u;

	// Colour channel normalisation
	constexpr float kByteToFloat = 1.0f / 255.0f;
} // anonymous namespace

namespace Chained
{

	RuntimeLayer::RuntimeLayer(const std::string& projectPath)
		: Layer("RuntimeLayer"),
		  m_ProjectPath(projectPath)
	{
		m_SceneRenderer = std::make_unique<SceneRenderer>();

		m_Renderer = ServiceLocator::TryGet<Renderer>();
		m_AssetManager = ServiceLocator::TryGet<AssetManager>();
	}

	RuntimeLayer::~RuntimeLayer() = default;

	void RuntimeLayer::OnAttach()
	{
		auto* imguiLayer = Application::Get().GetImGuiLayer();
		if (imguiLayer)
		{
			ImGui::SetCurrentContext(static_cast<ImGuiContext*>(imguiLayer->GetContext()));
		}

		auto& io = ImGui::GetIO();

		if (io.Fonts->Fonts.Size == 0)
		{
			io.Fonts->AddFontDefault();
			CH_CORE_TRACE("RuntimeSystem: Using built-in ImGui default font.");
		}

		InitProject(m_ProjectPath);

		if (auto* fontRegistry = ServiceLocator::TryGet<UIFontRegistry>())
		{
			if (ImFont* projectDefaultFont = fontRegistry->EnsureDefaultProjectFont(kDefaultUIFontSize, false))
			{
				io.FontDefault = projectDefaultFont;
				CH_CORE_TRACE("RuntimeSystem: Switched default UI font to project font.");
			}
			else
			{
				CH_CORE_WARN(
					"RuntimeSystem: EnsureDefaultProjectFont returned nullptr — staying with ImGui default font.");
			}
		}

		if (imguiLayer)
		{
			imguiLayer->RefreshFontAtlasTexture();
		}

		if (m_Scene)
		{
			Window& window = Application::Get().GetWindow();
			m_Scene->OnViewportResize(window.GetWidth(), window.GetHeight());
			EnsureRuntimeFramebuffer((uint32_t)window.GetWidth(), (uint32_t)window.GetHeight());
		}
	}

	void RuntimeLayer::OnDetach()
	{
		StopCurrentScene();

		m_Scene = nullptr;
		if (auto* se = ServiceLocator::TryGet<ScriptEngine>())
		{
			se->SetContextScene(nullptr);
		}
		m_LoadState = {};
	}

	void RuntimeLayer::OnUpdate(Timestep ts)
	{
		if (!m_PendingScenePath.empty())
		{
			std::string path = std::move(m_PendingScenePath);
			m_PendingScenePath.clear();
			LoadScene(path);
			return;
		}

		if (m_AssetManager)
		{
			m_AssetManager->FinalizePendingLoads();
		}

		TickFontRebuild(ts);
		TickSceneLoading(ts);
		TickRunning(ts);
	}

	void RuntimeLayer::TickFontRebuild(Timestep /*ts*/)
	{
		if (auto* fontRegistry = ServiceLocator::TryGet<UIFontRegistry>())
		{
			if (fontRegistry->NeedsAtlasRebuild())
			{
				auto* imguiLayer = Application::Get().GetImGuiLayer();
				if (imguiLayer)
				{
					imguiLayer->ExecuteNextFrame([imguiLayer]() { imguiLayer->RefreshFontAtlasTexture(); });
				}
				fontRegistry->ClearRebuildFlag();
			}
		}
	}

	void RuntimeLayer::TickSceneLoading(Timestep ts)
	{
		if (!m_Scene || m_LoadState.State != RuntimeLoadState::LoadingScene)
		{
			return;
		}

		// Keep ENet alive during potentially long asset loading to prevent timeout disconnects.
		// Any SceneLoaded messages arriving now will be safely deferred by NetworkSystem.
		if (auto* net = ServiceLocator::TryGet<Network>())
		{
			net->Update(ts);
		}

		if (m_AssetManager)
		{
			m_AssetManager->FinalizePendingLoads();
		}

		AssetResolutionSystem::Update(m_Scene->GetRegistry());
		m_LoadState.OverlayElapsed += (float)ts;

		bool ready = IsSceneReadyToStart();
		if (!ready && m_LoadState.OverlayElapsed > kLoadingTimeoutSec)
		{
			CH_CORE_WARN(
				"RuntimeSystem: Loading timeout ({:.0f}s) reached while waiting for assets. Forcing scene start.",
				kLoadingTimeoutSec);
			ready = true;
		}

		if (!ready || m_LoadState.OverlayElapsed < m_LoadState.MinOverlayDuration)
		{
			return;
		}

		if (auto* wr = ServiceLocator::TryGet<WidgetRenderer>())
		{
			wr->ResetButtonStates(m_Scene.get());
		}

		if (m_Scene->GetSceneState() != SceneState::Play)
		{
			m_Scene->TransitionToState(SceneState::Play);
		}

		// Advance the scene startup while the loading overlay is still active.
		// PhysicsBodySystem may still be waiting for an async mesh shape bake.
		m_Scene->OnUpdateRuntime(ts);

		if (!m_Scene->IsStartingUp() || m_LoadState.OverlayElapsed > kStartupTimeoutSec)
		{
			m_LoadState.State = RuntimeLoadState::Running;
			m_LoadState.SuppressNextUIInput = true;
			CH_CORE_INFO("RuntimeSystem: Scene assets and physics are ready, entering runtime.");
		}
	}

	void RuntimeLayer::TickRunning(Timestep ts)
	{
		if (!m_Scene || !IsRunning())
		{
			return;
		}

		bool suppress = m_LoadState.SuppressNextUIInput;
		m_LoadState.SuppressNextUIInput = false;
		if (auto* wr = ServiceLocator::TryGet<WidgetRenderer>())
		{
			wr->ProcessInput(m_Scene.get(), suppress);
		}

		m_Scene->OnUpdateRuntime(ts);
	}

	void RuntimeLayer::OnRender(Timestep ts)
	{
		(void)ts; // No work
		Window& window = Application::Get().GetWindow();
		uint32_t width = (uint32_t)window.GetWidth();
		uint32_t height = (uint32_t)window.GetHeight();

		if (!m_Scene)
		{
			m_Renderer->Clear({0.0f, 0.0f, 0.0f, 1.0f});
			return;
		}

		if (width == 0 || height == 0)
		{
			return;
		}

		EnsureRuntimeFramebuffer(width, height);

		glm::vec4 bgColor = CalculateBackgroundColor();

		auto camConfig = GetActiveCamera();
		if (camConfig)
		{
			SceneRenderOptions options;

			m_HDRFramebuffer->Bind();
			m_Renderer->Clear(bgColor);
			m_SceneRenderer->RenderScene(m_Scene->GetRegistry(), m_Scene->GetSettings(), camConfig.value(), options);
			m_HDRFramebuffer->Unbind();
			m_HDRFramebuffer->Resolve();

			m_Renderer->SetViewport(0, 0, (int)width, (int)height);
			m_Renderer->Clear(bgColor);
			m_Renderer->ApplyPostProcessing(m_HDRFramebuffer->GetColorAttachmentRendererID(),
											m_HDRFramebuffer->GetDepthAttachmentRendererID(), camConfig.value(),
											nullptr, {});
		}
		else
		{
			m_Renderer->Clear(bgColor);
		}
	}

	void RuntimeLayer::OnImGuiRender()
	{
		if (m_Scene)
		{
			ImGuiViewport* viewport = ImGui::GetMainViewport();
			ImGui::SetNextWindowPos(viewport->WorkPos);
			ImGui::SetNextWindowSize(viewport->WorkSize);
			ImGui::SetNextWindowViewport(viewport->ID);

			ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
									 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
									 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus |
									 ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking;

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0, 0});
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

			if (ImGui::Begin("RuntimeUI", nullptr, flags))
			{
				if (IsRunning())
				{
					ImVec2 childSize = ImGui::GetContentRegionAvail();
					if (ImGui::BeginChild("##RuntimeUICanvas", childSize, false,
										  ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
											  ImGuiWindowFlags_NoScrollWithMouse))
					{
						ImVec2 canvasPos = ImGui::GetCursorScreenPos();
						ImVec2 canvasSize = ImGui::GetContentRegionAvail();
						if (auto* wr = ServiceLocator::TryGet<WidgetRenderer>())
						{
							wr->DrawCanvas(m_Scene.get(), canvasPos, canvasSize, false);
						}
						m_Scene->OnRenderUI();
					}
					ImGui::EndChild();
				}
			}

			ImGui::End();
			ImGui::PopStyleVar(2);

			if (m_LoadState.State == RuntimeLoadState::LoadingScene &&
				m_Scene->GetSettings().Type == SceneType::Default)
			{
				DrawLoadingOverlay();
			}
		}
	}

	void RuntimeLayer::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowResizeEvent>([this](auto& ev) {
			if (ev.GetWidth() == 0 || ev.GetHeight() == 0)
			{
				return false;
			}

			EnsureRuntimeFramebuffer(ev.GetWidth(), ev.GetHeight());
			if (m_Scene)
			{
				m_Scene->OnViewportResize(ev.GetWidth(), ev.GetHeight());
			}
			return false;
		});

		// Allow C# scripts (and any engine code) to request a scene change at runtime
		// by firing SceneChangeRequestEvent via Application::Get().OnEvent().
		dispatcher.Dispatch<SceneChangeRequestEvent>([this](SceneChangeRequestEvent& ev) {
			std::filesystem::path scenePath = ev.GetPath();
			if (scenePath.is_relative() && Project::GetActive())
			{
				scenePath = Project::GetActive()->GetAssetPath(ev.GetPath());
			}
			m_PendingScenePath = scenePath.string();
			CH_CORE_INFO("RuntimeLayer: Scene change requested → '{}'", m_PendingScenePath);
			return true;
		});
	}

	//-----------------------------------------------------------------------------
	// Purpose: Load a new scene from file
	//-----------------------------------------------------------------------------
	void RuntimeLayer::LoadScene(const std::string& path)
	{
		const std::string normalizedPath = NormalizeAssetPath(path);
		if (normalizedPath.empty())
		{
			CH_CORE_WARN("RuntimeSystem: Ignoring empty scene path request.");
			return;
		}

		std::filesystem::path scenePath = normalizedPath;
		if (scenePath.is_relative() && Project::GetActive())
		{
			scenePath = Project::GetActive()->GetAssetPath(scenePath);
		}

		if (!scenePath.is_absolute())
		{
			std::error_code ec;
			scenePath = std::filesystem::absolute(scenePath, ec);
			if (ec)
			{
				CH_CORE_ERROR("RuntimeSystem: Failed to resolve absolute scene path '{}' ({})", normalizedPath,
							  ec.message());
				return;
			}
		}

		bool sceneAccessible = FileExists(scenePath);
		if (!sceneAccessible && m_AssetManager && m_AssetManager->IsPacked())
		{
			sceneAccessible = !m_AssetManager->ReadProjectAsset(scenePath).empty();
		}
		if (!sceneAccessible)
		{
			CH_CORE_ERROR("RuntimeSystem: Scene file not found '{}'.", scenePath.string());
			return;
		}

		if (!TransitionToScene(scenePath))
		{
			CH_CORE_ERROR("RuntimeSystem: Failed to transition to scene '{}'.", scenePath.string());
		}
	}

	bool RuntimeLayer::InitProject(const std::string& projectPath)
	{
		if (!DiscoverAndLoadProject(projectPath))
		{
			return false;
		}

		auto project = Project::GetActive();
		if (!project)
		{
			CH_CORE_ERROR("Runtime: No active project after DiscoverAndLoadProject - cannot initialize scripting.");
			return false;
		}
		auto assemblyPath =
			ScriptEngine::ResolveAssemblyPath(project->GetConfig().Scripting, project->GetConfig().ProjectDirectory);

		CH_CORE_INFO("RuntimeSystem: Loading project assembly: {}", assemblyPath.string());

		auto* se = ServiceLocator::TryGet<ScriptEngine>();
		if (assemblyPath.empty() || !se || !se->ReloadAssembly(assemblyPath.string()))
		{
			CH_CORE_WARN(
				"RuntimeSystem: Script reload failed during project initialization (path: {}). Runtime continues "
				"without scripts.",
				assemblyPath.string());
		}

		if (auto* wr = ServiceLocator::TryGet<WidgetRenderer>())
		{
			wr->LoadProjectFonts();
		}

		ApplyWindowConfiguration();
		SetupBrandingAndIcon();

		LoadInitialScene();

		return true;
	}

	bool RuntimeLayer::DiscoverAndLoadProject(const std::string& projectPath)
	{
		std::filesystem::path discoveryPath = projectPath;

		if (discoveryPath.empty())
		{
			std::filesystem::path exePath = std::filesystem::absolute(
				std::filesystem::path(Application::Get().GetSpecification().CommandLineArgs.Args[0]));
			discoveryPath = exePath.parent_path();
		}

		if (discoveryPath.extension() == ".chproject")
		{
			m_ProjectPath = discoveryPath.string();
		}
		else
		{
			m_ProjectPath = (discoveryPath / (Application::Get().GetSpecification().Name + ".chproject")).string();
		}

		if (m_ProjectPath.empty())
		{
			return false;
		}

		size_t openedPacks = m_AssetManager->OpenAllPacksInDirectory(Platform::GetExecutableDirectory());
		if (openedPacks > 0)
		{
			CH_CORE_INFO("RuntimeSystem: Mounted {} resource pack(s)", openedPacks);
		}

		auto project = Project::Load(m_ProjectPath);
		if (!project)
		{
			CH_CORE_ERROR("RuntimeSystem: Failed to load project file at '{}'", m_ProjectPath);
			return false;
		}

		Project::SetActive(project);

		CH_CORE_TRACE("RuntimeSystem: Project loaded: {}", project->GetName());
		CH_CORE_TRACE("RuntimeSystem: Project Directory: {}", project->GetConfig().ProjectDirectory.string());
		CH_CORE_TRACE("RuntimeSystem: Asset Directory: {}", project->GetAssetDirectory().string());

		m_AssetManager->SetProjectDirectory(project->GetConfig().ProjectDirectory);
		m_AssetManager->SetAssetDirectory(project->GetAssetDirectory());

		// Source directories for dev mode — runtime reads from source tree instead of copied assets
		// These are set by CMake via CH_SOURCE_RESOURCES_DIR / CH_SOURCE_ASSETS_DIR compile definitions.
		// In exported games the source dirs don't exist, so SetSource*Dir() is never called
		// and the resolver falls back to ProjectDirectory / pack files as usual.
#ifdef CH_SOURCE_RESOURCES_DIR
		{
			std::filesystem::path srcResDir(CH_SOURCE_RESOURCES_DIR);
			if (std::filesystem::exists(srcResDir))
			{
				CH_CORE_INFO("RuntimeSystem: Using source engine resources: {}", srcResDir.string());
				m_AssetManager->SetSourceResourcesDir(srcResDir);
			}
		}
#endif
#ifdef CH_SOURCE_ASSETS_DIR
		{
			std::filesystem::path srcAssetsDir(CH_SOURCE_ASSETS_DIR);
			if (std::filesystem::exists(srcAssetsDir))
			{
				CH_CORE_INFO("RuntimeSystem: Using source game assets: {}", srcAssetsDir.string());
				m_AssetManager->SetSourceAssetsDir(srcAssetsDir);
			}
		}
#endif

		m_Renderer->LoadEngineResources();

		if (auto* se = ServiceLocator::TryGet<ScriptEngine>())
		{
			se->TryAutoLoad(project->GetConfig());
		}

		return true;
	}

	void RuntimeLayer::ApplyWindowConfiguration()
	{
		auto project = Project::GetActive();
		if (!project)
		{
			return;
		}

		auto& config = project->GetConfig();
		Window& window = Application::Get().GetWindow();

		bool vsync = config.Window.VSync;
		int width = config.Window.Width;
		int height = config.Window.Height;
		bool fullscreen = config.Runtime.Fullscreen;

		const auto& args = Application::Get().GetSpecification().CommandLineArgs;
		for (int i = 1; i < args.Count; ++i)
		{
			std::string arg = args.Args[i];
			if (arg == "--width" && i + 1 < args.Count)
			{
				width = std::stoi(args.Args[++i]);
			}
			else if (arg == "--height" && i + 1 < args.Count)
			{
				height = std::stoi(args.Args[++i]);
			}
			else if (arg == "--fullscreen")
			{
				fullscreen = true;
			}
			else if (arg == "--windowed")
			{
				fullscreen = false;
			}
			else if (arg == "--vsync" && i + 1 < args.Count)
			{
				vsync = (std::string(args.Args[++i]) == "on");
			}
		}

		window.SetVSync(vsync);
		if (width != window.GetWidth() || height != window.GetHeight())
		{
			window.SetSize(width, height);
		}
		window.SetFullscreen(fullscreen);
	}

	void RuntimeLayer::SetupBrandingAndIcon()
	{
		auto project = Project::GetActive();
		if (!project)
		{
			return;
		}

		auto& config = project->GetConfig();
		Window& window = Application::Get().GetWindow();
		window.SetTitle(config.Name);

		auto* am = ServiceLocator::TryGet<AssetManager>();
		if (!am)
		{
			CH_CORE_WARN("RuntimeSystem: AssetManager unavailable, cannot resolve window icon");
			return;
		}

		std::vector<std::string> candidatePaths;
		if (!config.IconPath.empty())
		{
			candidatePaths.push_back(config.IconPath);
			if (config.IconPath.rfind("engine/", 0) == 0)
			{
				candidatePaths.push_back(config.IconPath.substr(7));
			}
		}

		// Resolve via AssetManager — handles disk, source tree, and pack fallback
		for (const auto& cand : candidatePaths)
		{
			std::string absPath = am->ResolvePath(cand);
			if (!absPath.empty() && std::filesystem::exists(absPath))
			{
				CH_CORE_TRACE("RuntimeSystem: Setting window icon from disk: {}", absPath);
				window.SetWindowIcon(absPath);
				return;
			}

			auto data = am->ReadAssetData(cand);
			if (!data.empty())
			{
				CH_CORE_TRACE("RuntimeSystem: Setting window icon from pack: {}", cand);
				window.SetWindowIconFromMemory(data.data(), data.size());
				return;
			}
		}

		CH_CORE_WARN("RuntimeSystem: Failed to resolve window icon from candidates");
	}

	void RuntimeLayer::LoadInitialScene()
	{
		auto project = Project::GetActive();
		if (!project)
		{
			return;
		}

		auto& config = project->GetConfig();
		std::string sceneToLoad = config.StartScene;

		const auto& args = Application::Get().GetSpecification().CommandLineArgs;
		for (int i = 1; i < args.Count; ++i)
		{
			if (std::string(args.Args[i]) == "--scene" && i + 1 < args.Count)
			{
				sceneToLoad = args.Args[++i];
				break;
			}
		}

		if (sceneToLoad.empty())
		{
			sceneToLoad = config.ActiveScenePath.string();
		}

		CH_CORE_INFO("RuntimeSystem: Initial scene to load: '{}'", sceneToLoad);

		if (sceneToLoad.empty())
		{
			std::filesystem::path scenesDir = project->GetAssetDirectory() / "scenes";
			if (std::filesystem::exists(scenesDir))
			{
				try
				{
					for (const auto& entry : std::filesystem::recursive_directory_iterator(scenesDir))
					{
						if (entry.path().extension() == ".chscene")
						{
							sceneToLoad =
								std::filesystem::relative(entry.path(), project->GetAssetDirectory()).string();
							break;
						}
					}
				} catch (const std::filesystem::filesystem_error& e)
				{
					CH_CORE_WARN("RuntimeSystem: Failed to enumerate scenes in '{}': {}", scenesDir.string(), e.what());
				}
			}
		}

		if (!sceneToLoad.empty())
		{
			LoadScene(sceneToLoad);
		}
	}

	void RuntimeLayer::StopCurrentScene()
	{
		if (auto* audio = ServiceLocator::TryGet<Audio>())
		{
			audio->StopAll();
		}

		if (!m_Scene)
		{
			return;
		}

		m_Scene->OnRuntimeStop();
	}

	void RuntimeLayer::AppendFontRequest(const TextStyle& style, std::vector<std::pair<std::string, float>>& out,
										 std::unordered_set<std::string>& dedupe) const
	{
		std::string fontName = NormalizeAssetPath(style.FontName);
		if (fontName.empty() || fontName == "Default")
		{
			return;
		}

		const float fontSize = (style.FontSize > 0.0f) ? style.FontSize : kDefaultFontSize;
		const int roundedHalf = static_cast<int>(std::lround(fontSize * 2.0f));
		const std::string key = fontName + "|" + std::to_string(roundedHalf);

		if (!dedupe.insert(key).second)
		{
			return;
		}

		out.emplace_back(fontName, fontSize);
	}

	std::vector<std::pair<std::string, float>> RuntimeLayer::CollectSceneFontRequests() const
	{
		std::vector<std::pair<std::string, float>> requests;
		if (!m_Scene)
		{
			return requests;
		}

		std::unordered_set<std::string> dedupe;
		auto& registry = m_Scene->GetRegistry();

		auto view = registry.view<UIControlComponent>();
		for (entt::entity id : view)
		{
			AppendFontRequest(view.get<UIControlComponent>(id).TextStyle, requests, dedupe);
		}

		return requests;
	}

	void RuntimeLayer::PreloadSceneFonts(bool allowRuntimeMutation)
	{
		auto requests = CollectSceneFontRequests();
		if (requests.empty())
		{
			return;
		}

		const int loadedCount = [&]() {
			if (auto* fontRegistry = ServiceLocator::TryGet<UIFontRegistry>())
			{
				return fontRegistry->PreloadFonts(requests, allowRuntimeMutation);
			}
			return 0;
		}();
		if (loadedCount <= 0)
		{
			return;
		}

		CH_CORE_TRACE("RuntimeSystem: Preloaded {} scene font tuple(s).", loadedCount);

		if (allowRuntimeMutation && ImGui::GetFrameCount() > 0)
		{
			if (auto* imguiLayer = Application::Get().GetImGuiLayer())
			{
				if (!imguiLayer->RefreshFontAtlasTexture())
				{
					CH_CORE_WARN("RuntimeSystem: Scene fonts were loaded, but font atlas refresh failed.");
				}
			}
		}
	}

	bool RuntimeLayer::SetupNewScene(const std::filesystem::path& scenePath)
	{
		auto nextScene = std::make_shared<Scene>();
		SceneSerializer serializer(nextScene.get());
		if (!serializer.Deserialize(scenePath.string()))
		{
			m_Scene = nullptr;
			if (auto* se = ServiceLocator::TryGet<ScriptEngine>())
			{
				se->SetContextScene(nullptr);
			}
			return false;
		}

		m_Scene = nextScene;
		m_Scene->SetScenePath(scenePath.string());

		if (auto* se = ServiceLocator::TryGet<ScriptEngine>())
		{
			se->SetContextScene(m_Scene.get());
		}

		Window& window = Application::Get().GetWindow();
		m_Scene->OnViewportResize(window.GetWidth(), window.GetHeight());
		EnsureRuntimeFramebuffer((uint32_t)window.GetWidth(), (uint32_t)window.GetHeight());

		return true;
	}

	void RuntimeLayer::ResetUIState()
	{
		if (!m_Scene)
		{
			return;
		}

		auto& registry = m_Scene->GetRegistry();
		auto view = registry.view<UIControlComponent>();
		for (entt::entity id : view)
		{
			view.get<UIControlComponent>(id).PressedThisFrame = false;
		}
	}

	void RuntimeLayer::BeginSceneLoading()
	{
		m_LoadState.SuppressNextUIInput = true;

		if (m_Scene)
		{
			AssetResolutionSystem::Update(m_Scene->GetRegistry());
		}

		PreloadSceneFonts(ImGui::GetFrameCount() > 0);

		m_LoadState.State = RuntimeLoadState::LoadingScene;
		m_LoadState.OverlayElapsed = 0.0f;
		CH_CORE_INFO("RuntimeSystem: Scene loaded, waiting for async assets before runtime start.");
	}

	bool RuntimeLayer::TransitionToScene(const std::filesystem::path& scenePath)
	{
		StopCurrentScene();
		m_LoadState = {};

		if (!SetupNewScene(scenePath))
		{
			return false;
		}

		ResetUIState();
		BeginSceneLoading();
		return true;
	}

	glm::vec4 RuntimeLayer::CalculateBackgroundColor() const
	{
		if (!m_Scene)
		{
			return {0.0f, 0.0f, 0.0f, 1.0f};
		}

		const auto& settings = m_Scene->GetSettings();
		glm::vec4 bgColor = {settings.BackgroundColor.r * kByteToFloat, settings.BackgroundColor.g * kByteToFloat,
							 settings.BackgroundColor.b * kByteToFloat, settings.BackgroundColor.a * kByteToFloat};

		if (settings.Environment && settings.Mode != BackgroundMode::Color)
		{
			auto& env = settings.Environment->GetSettings();
			if (env.Fog.Enabled)
			{
				bgColor = glm::vec4(env.Fog.FogColor.r * kByteToFloat, env.Fog.FogColor.g * kByteToFloat,
									env.Fog.FogColor.b * kByteToFloat, env.Fog.FogColor.a * kByteToFloat);
			}
		}

		return bgColor;
	}

	std::optional<Camera3D> RuntimeLayer::GetActiveCamera()
	{
		if (m_Scene)
		{
			return SceneRenderer::GetActiveCamera(m_Scene->GetRegistry());
		}
		return std::nullopt;
	}

	void RuntimeLayer::EnsureRuntimeFramebuffer(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
		{
			return;
		}

		auto project = Project::GetActive();
		int msaaSampleCount = project ? project->GetConfig().Render.AntiAliasingSamples : (int)kDefaultMsaaSamples;
		uint32_t msaaSamplesClamped = msaaSampleCount > 1 ? (uint32_t)msaaSampleCount : 1u;

		if (m_HDRFramebuffer && msaaSamplesClamped != m_MSAAFramebufferSamples)
		{
			m_HDRFramebuffer.reset();
		}

		if (!m_HDRFramebuffer)
		{
			FramebufferSpecification hdrSpec;
			hdrSpec.Width = width;
			hdrSpec.Height = height;
			hdrSpec.Samples = msaaSamplesClamped;
			hdrSpec.ColorFormat = FramebufferColorFormat::RGBA16F;
			m_HDRFramebuffer = Framebuffer::Create(hdrSpec);
			m_MSAAFramebufferSamples = msaaSamplesClamped;
		}
		else
		{
			const auto& spec = m_HDRFramebuffer->GetSpecification();
			if (spec.Width != width || spec.Height != height)
			{
				m_HDRFramebuffer->Resize(width, height);
			}
		}
	}

	bool RuntimeLayer::IsSceneReadyToStart() const
	{
		auto* assetManager = m_AssetManager;
		if (!assetManager)
		{
			return true;
		}
		return !assetManager->HasBackgroundWork();
	}

	void RuntimeLayer::DrawLoadingOverlay()
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
								 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
								 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::PushStyleColor(ImGuiCol_WindowBg,
							  ImVec4(kOverlayBgDark, kOverlayBgDark, kOverlayBgDark, kOverlayBgAlpha));

		if (ImGui::Begin("##RuntimeLoadingOverlay", nullptr, flags))
		{
			bool fontPushed = false;
			if (auto* fontRegistry = ServiceLocator::TryGet<UIFontRegistry>())
			{
				if (ImFont* font = fontRegistry->GetDefaultFont())
				{
					ImGui::PushFont(font);
					fontPushed = true;
				}
			}

			const size_t totalPending = m_AssetManager ? m_AssetManager->GetLoadingAssetCount() : 0u;

			int dotsCount = (static_cast<int>(ImGui::GetTime() * kDotsAnimSpeed) % kDotsCount) + 1;
			std::string dots(static_cast<size_t>(dotsCount), '.');
			std::string loadingLine = "Preparing world" + dots;
			std::string pendingLine = "Pending assets: " + std::to_string(totalPending);

			ImGui::SetCursorPosY(ImGui::GetWindowHeight() * kOverlayCursorY);

			const char* title = "Loading scene";
			ImVec2 titleSize = ImGui::CalcTextSize(title);
			ImGui::SetCursorPosX((ImGui::GetWindowWidth() - titleSize.x) * 0.5f);
			ImGui::TextUnformatted(title);

			ImVec2 loadingSize = ImGui::CalcTextSize(loadingLine.c_str());
			ImGui::SetCursorPosX((ImGui::GetWindowWidth() - loadingSize.x) * 0.5f);
			ImGui::TextUnformatted(loadingLine.c_str());

			ImVec2 pendingSize = ImGui::CalcTextSize(pendingLine.c_str());
			ImGui::SetCursorPosX((ImGui::GetWindowWidth() - pendingSize.x) * 0.5f);
			ImGui::TextUnformatted(pendingLine.c_str());

			if (fontPushed)
			{
				ImGui::PopFont();
			}
		}

		ImGui::End();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
	}
} // namespace Chained
