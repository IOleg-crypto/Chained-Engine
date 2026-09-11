#include "layer.h"
#include "editor_colors.h"
#include "editor/font_manager.h"
#include "engine/core/input.h"
#include "engine/core/events/input_events.h"
#include "engine/core/key_codes.h"
#include "engine/core/service_locator.h"
#include "engine/imgui/imgui_layer.h"
#include "events.h"
#include "gui.h"
#include "editor_menu.h"
#include "layout.h"
#include "panels.h"

#include "engine/app/application.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/profiler.h"
#include "engine/graphics/api/graphics_device.h"
#include "engine/ui/ui_font_registry.h"
#include "engine/ui/widget_renderer.h"
#include "engine/project/project.h"
#include "panels/property_editor.h"
#include "panels/viewport_panel.h"
#include "engine/scripting/scriptengine.h"
#include "engine/graphics/pipeline/renderer.h"
#include "engine/graphics/pipeline/scene_renderer.h"
#include "engine/graphics/api/framebuffer.h"
#include "thirdparty/IconsFontAwesome6.h"
#include "ui/project_selector_ui.h"
#include <ImGuizmo.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <yaml-cpp/yaml.h>

namespace Chained
{
	void EditorLayer::DrawLoadingOverlay(const char* title, const char* status)
	{
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
								 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
								 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
								 ImGuiWindowFlags_NoInputs;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, EditorColors::LoadingOverlayBg);

		if (ImGui::Begin("##EditorLoadingOverlay", nullptr, flags))
		{
			float windowWidth = ImGui::GetWindowWidth();
			float windowHeight = ImGui::GetWindowHeight();

			ImGui::SetCursorPosY(windowHeight * 0.42f);

			if (title && title[0] != '\0')
			{
				ImVec2 titleSize = ImGui::CalcTextSize(title);
				ImGui::SetCursorPosX((windowWidth - titleSize.x) * 0.5f);
				ImGui::TextUnformatted(title);
				ImGui::Spacing();
			}

			if (status && status[0] != '\0')
			{
				ImVec2 statusSize = ImGui::CalcTextSize(status);
				ImGui::SetCursorPosX((windowWidth - statusSize.x) * 0.5f);
				ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1.0f), "%s", status);
				ImGui::Spacing();
			}

			// Animated progress bar
			float barWidth = std::min(320.0f, windowWidth * 0.4f);
			float barHeight = 4.0f;
			ImGui::SetCursorPosX((windowWidth - barWidth) * 0.5f);

			float time = (float)ImGui::GetTime();
			float animFraction = fmodf(time * 0.8f, 1.0f);
			ImGui::ProgressBar(animFraction, ImVec2(barWidth, barHeight), "");

			auto* assetManager = ServiceLocator::TryGet<AssetManager>();
			uint32_t totalPending = assetManager ? (uint32_t)assetManager->GetPendingFinalizeCount() : 0;
			if (totalPending > 0)
			{
				ImGui::Spacing();
				char pendingBuffer[64];
				snprintf(pendingBuffer, sizeof(pendingBuffer), "Finalizing assets: %u", totalPending);

				ImVec2 pendingSize = ImGui::CalcTextSize(pendingBuffer);
				ImGui::SetCursorPosX((windowWidth - pendingSize.x) * 0.5f);
				ImGui::TextColored(ImVec4(0.55f, 0.55f, 0.55f, 1.0f), "%s", pendingBuffer);
			}
		}

		ImGui::End();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
	}

	EditorLayer::EditorLayer()
		: Layer("EditorLayer")
	{
		s_Instance = this;

		m_ProjectManager = std::make_unique<EditorProjectManager>();
		m_SceneManager = std::make_unique<EditorSceneManager>();

		m_Menu = std::make_unique<EditorMenu>();
		m_Panels = std::make_unique<EditorPanels>();

		m_Layout = std::make_unique<EditorLayout>(*m_Panels);
		m_ProjectSelectorUI = std::make_unique<ProjectSelectorUI>(*m_ProjectManager);
		m_FontManager = std::make_unique<FontManager>(m_Config);

		LoadConfig();
	}

	EditorLayer::~EditorLayer()
	{
		SetSelectedEntity({});
		s_Instance = nullptr;
	}

	template <typename T> static void LoadYAMLField(const YAML::Node& node, const char* key, T& target)
	{
		if (node[key])
		{
			target = node[key].as<T>(target);
		}
	}

	// Single source of truth for all YAML fields — used by both LoadConfig and SaveConfig.
	// Each macro expansion: (YAML_KEY, STRUCT_FIELD)
#define EDITOR_CONFIG_FIELDS(X)                                                                                        \
	X("LastScenePath", LastScenePath)                                                                                  \
	X("LoadLastProjectOnStartup", LoadLastProjectOnStartup)                                                            \
	X("AutoSaveEnabled", AutoSaveEnabled)                                                                              \
	X("AutoSaveInterval", AutoSaveInterval)                                                                            \
	X("FontPath", FontPath)                                                                                            \
	X("FontSize", FontSize)                                                                                            \
	X("IconSizeScale", IconSizeScale)                                                                                  \
	X("IconSizeMin", IconSizeMin)                                                                                      \
	X("IconSizeMax", IconSizeMax)                                                                                      \
	X("CameraMoveSpeed", CameraMoveSpeed)                                                                              \
	X("CameraBoostMultiplier", CameraBoostMultiplier)                                                                  \
	X("DisableCameraZoom", DisableCameraZoom)                                                                          \
	X("CameraRotationSpeed", CameraRotationSpeed)                                                                      \
	X("CameraZoomSpeedMultiplier", CameraZoomSpeedMultiplier)                                                          \
	X("CameraFovDegrees", CameraFovDegrees)                                                                            \
	X("CameraNearClip", CameraNearClip)                                                                                \
	X("CameraFarClip", CameraFarClip)                                                                                  \
	X("ShowEditorIcons", ShowEditorIcons)                                                                              \
	X("GizmoScale", GizmoScale)                                                                                        \
	X("DefaultThumbnailSize", DefaultThumbnailSize)                                                                    \
	X("DefaultSortOrder", DefaultSortOrder)                                                                            \
	X("ShowFileExtensions", ShowFileExtensions)                                                                        \
	X("ConfirmOnSceneClose", ConfirmOnSceneClose)                                                                      \
	X("MaxRecentProjects", MaxRecentProjects)

	void EditorLayer::LoadConfig()
	{
		std::filesystem::path configPath = std::filesystem::current_path() / "editor_settings.yaml";
		if (!std::filesystem::exists(configPath))
		{
			return;
		}

		try
		{
			YAML::Node data = YAML::LoadFile(configPath.string());
			if (data["Editor"])
			{
				auto node = data["Editor"];
				if (node["LastProjectPath"])
				{
					std::string lastProj = node["LastProjectPath"].as<std::string>("");
					m_ProjectManager->RestoreLastProjectPath(lastProj);
					m_Config.LastProjectPath = lastProj;
				}
#define LOAD_FIELD(yamlKey, field) LoadYAMLField(node, yamlKey, m_Config.field);
				EDITOR_CONFIG_FIELDS(LOAD_FIELD)
#undef LOAD_FIELD

				if (node["RecentProjects"])
				{
					m_Config.RecentProjects.clear();
					for (const auto& entry : node["RecentProjects"])
					{
						m_Config.RecentProjects.push_back(entry.as<std::string>());
					}
				}
			}
		} catch (const std::exception& e)
		{
			CH_CORE_ERROR("EditorLayer: Failed to load editor settings: {}", e.what());
		}
	}

	void EditorLayer::SaveConfig()
	{
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Editor" << YAML::Value << YAML::BeginMap;
		out << YAML::Key << "LastProjectPath" << YAML::Value << m_ProjectManager->GetLastProjectPath();
		m_Config.LastProjectPath = m_ProjectManager->GetLastProjectPath();

#define SAVE_FIELD(yamlKey, field) out << YAML::Key << yamlKey << YAML::Value << m_Config.field;
		EDITOR_CONFIG_FIELDS(SAVE_FIELD)
#undef SAVE_FIELD

		out << YAML::Key << "RecentProjects" << YAML::Value << YAML::BeginSeq;
		for (const auto& path : m_Config.RecentProjects)
		{
			out << path;
		}
		out << YAML::EndSeq;

		out << YAML::EndMap;
		out << YAML::EndMap;

		std::filesystem::path configPath = std::filesystem::current_path() / "editor_settings.yaml";
		std::ofstream fout(configPath);
		if (!fout.is_open())
		{
			CH_CORE_ERROR("EditorLayer: Failed to open editor settings for writing: {}", configPath.string());
			return;
		}
		fout << out.c_str();
	}

	void EditorLayer::OnAttach()
	{
		// Ensure this module (EXE) uses the same ImGui context as the engine DLL
		auto& app = Application::Get();
		ImGui::SetCurrentContext(static_cast<ImGuiContext*>(app.GetImGuiLayer()->GetContext()));

		// SetTraceLogCallback removed - now using engine logging

		EditorGUI::ApplyTheme();
		PropertyEditor::Init();
		m_Panels->Init();

		// Load editor fonts BEFORE project auto-load.
		// OnProjectOpened will clear + rebuild the atlas (editor + project fonts together).
		// LoadEditorFonts must run first so there is a valid atlas for the initial UI frame.
		LoadEditorFonts();

		// Auto-load last project/scene
		const auto& config = GetConfig();

		if (config.LoadLastProjectOnStartup && !m_ProjectManager->GetLastProjectPath().empty() &&
			std::filesystem::exists(m_ProjectManager->GetLastProjectPath()))
		{
			CH_CORE_INFO("Auto-loading last project: {}", m_ProjectManager->GetLastProjectPath());
			m_ProjectManager->OpenProject(m_ProjectManager->GetLastProjectPath());
			// No ImGui frame is in flight during OnAttach, so it is safe (and
			// required — the scene below needs asset dirs set) to process now.
			m_ProjectManager->ProcessPendingProjectOpen();

			if (!config.LastScenePath.empty() && std::filesystem::exists(config.LastScenePath))
			{
				CH_CORE_INFO("Auto-loading last scene: {}", config.LastScenePath);
				m_SceneManager->OpenScene(config.LastScenePath);
			}
		}
		else
		{
			Project::SetActive(nullptr);
		}

		// Ensure layout is initialized
		const char* iniPath = ImGui::GetIO().IniFilename;
		if (iniPath && !std::filesystem::exists(iniPath))
		{
			CH_CORE_INFO("OnAttach: Layout file '{}' not found, will be reset on first frame", iniPath);
			GetEditorState().NeedsLayoutReset = true;
		}

		auto* assetManager = ServiceLocator::TryGet<AssetManager>();
		if (assetManager)
		{
			auto project = Project::GetActive();
			std::string iconPath;
			if (project && !project->GetConfig().IconPath.empty())
			{
				iconPath = assetManager->ResolvePath(project->GetConfig().IconPath);
			}
			if (iconPath.empty())
			{
				iconPath = assetManager->ResolvePath("resources/icons/chaineddecosmapeditor.jpg");
			}
			if (std::filesystem::exists(iconPath))
			{
				app.GetWindow().SetWindowIcon(iconPath);
			}
			else
			{
				CH_CORE_WARN("Editor icon not found at: {}", iconPath);
			}
		}
		CH_CORE_INFO("EditorLayer Attached with modular panels.");
	}

	void EditorLayer::LoadEditorFonts()
	{
		m_FontManager->LoadFonts();
	}

	void EditorLayer::ReloadEditorFonts()
	{
		m_FontManager->ReloadFonts();
	}

	void EditorLayer::RequestEditorFontReload()
	{
		m_FontManager->RequestReload();
	}

	void EditorLayer::OnDetach()
	{
		if (auto scene = GetActiveScene())
		{
			if (scene->GetSceneState() != SceneState::Edit)
			{
				scene->OnRuntimeStop();
			}
		}
		// Explicitly save the ImGui panel layout before shutdown.
		// ImGui's built-in autosave runs on a timer (io.IniSavingRate, default 5s) and
		// may not fire before the process exits — especially if OnDetach is called after
		// the GLFW window is already destroyed. Saving here guarantees the layout is
		// always written regardless of shutdown timing.
		if (m_Layout)
		{
			ImGui::SaveIniSettingsToDisk(ImGui::GetIO().IniFilename);
		}
		SaveConfig();
	}

	void EditorLayer::OnUpdate(Timestep ts)
	{
		CH_PROFILE_FUNCTION();

		m_ProjectManager->ProcessPendingProjectOpen();

		if (m_FontManager->HasPendingReload())
		{
			auto* imguiLayer = Application::Get().GetImGuiLayer();
			if (imguiLayer)
			{
				imguiLayer->ExecuteNextFrame([this]() {
					ReloadEditorFonts();
					ImGuiIO& io = ImGui::GetIO();
					if (!io.Fonts->Fonts.empty())
					{
						io.FontDefault = io.Fonts->Fonts[0];
					}
				});
			}
			m_FontManager->ClearPendingReload();
		}

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

		if (!m_PendingSceneTransitionPath.empty())
		{
			m_SceneManager->OpenScene(m_PendingSceneTransitionPath);

			// Ensure play mode continues after scene load
			m_SceneManager->SetSceneState(SceneState::Play);

			m_PendingSceneTransitionPath.clear();
		}

		// 1. Scene manager updates internal transitions and the current active scene
		m_SceneManager->OnUpdate(ts);

		// 2. Update panels
		m_Panels->SetContext(GetActiveScene());
		m_Panels->OnUpdate(ts);

		// 3. If loading — skip everything else
		if (m_SceneManager->IsLoading())
		{
			return;
		}

		// 4. Logic update (Play/Simulate/Edit) is now handled by the scene
		// We just call OnUpdate on the active scene
		if (auto scene = GetActiveScene())
		{
			// Detect Edit->Play transition. The same physical click that pressed the
			// Play toolbar button is still reported by ImGui::IsMouseClicked this
			// frame, so suppress UI input once to stop it leaking into game widgets.
			SceneState state = scene->GetSceneState();
			if (state == SceneState::Play && m_PrevSceneState != SceneState::Play)
			{
				m_SuppressNextUIInput = true;
			}
			m_PrevSceneState = state;

			// If scene is in Play mode, update runtime
			if (state == SceneState::Play)
			{
				// Process UI input before scripts read widget state, unconditionally
				// each frame (see WidgetRenderer::ProcessInput). Keeps a one-frame
				// click edge from sticking when the viewport canvas isn't drawn.
				if (auto* uiRenderer = ServiceLocator::TryGet<WidgetRenderer>())
				{
					bool suppress = m_SuppressNextUIInput;
					m_SuppressNextUIInput = false;
					uiRenderer->ProcessInput(scene.get(), suppress);
				}

				scene->OnUpdateRuntime(ts);
			}
			else if (scene->GetSceneState() == SceneState::Simulate)
			{
				scene->OnUpdateSimulation(ts);
			}
			else
			{
				scene->OnUpdateEditor(ts);

				if (m_Config.AutoSaveEnabled)
				{
					m_SceneManager->AutoSave(m_Config.AutoSaveInterval, ts);
				}
			}
		}
	}

	void EditorLayer::OnRender(Timestep ts)
	{
		GraphicsDevice::Get().Clear({25, 25, 25, 255});
	}

	void EditorLayer::OnImGuiRender()
	{
		// ImGuizmo context sync only — BeginFrame() is already called in ImGuiLayer::Begin()
		ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());

		if (GetEditorState().NeedsLayoutReset)
		{
			ResetLayout();
			GetEditorState().NeedsLayoutReset = false;
		}

		bool hasProject = Project::GetActive() != nullptr;

		if (hasProject && GetEditorState().FullscreenGame)
		{
			if (auto viewportPanel = m_Panels->Get<ViewportPanel>())
			{
				viewportPanel->OnImGuiRender(true);
			}
		}
		else if (hasProject)
		{
			m_Layout->OnImGuiRender();
		}
		else
		{
			m_ProjectSelectorUI->OnImGuiRender();
		}

		if (m_SceneManager->IsLoading())
		{
			DrawLoadingOverlay("Editor Busy", m_SceneManager->GetLoadingStatus().c_str());
		}
	}

	void EditorLayer::ResetLayout()
	{
		m_Layout->ResetLayout();
	}

	// Project and Scene event handlers are now managed by EditorProjectManager and EditorSceneManager.

	std::shared_ptr<Scene> EditorLayer::GetActiveScene() const
	{
		return m_SceneManager->GetActiveScene();
	}

	void EditorLayer::OnEvent(Event& e)
	{
		if (auto scene = GetActiveScene())
		{
			scene->OnEvent(e);
		}

		// Dispatch events to all editor panels
		m_Panels->OnEvent(e);

		EventDispatcher dispatcher(e);

		// 1. Scene Management
		dispatcher.Dispatch<SceneOpenedEvent>([this](auto& e) { return m_SceneManager->OnSceneOpened(e); });
		dispatcher.Dispatch<ScenePlayEvent>([this](auto& e) {
			m_SceneManager->SetSceneState(SceneState::Play);
			return true;
		});
		dispatcher.Dispatch<SceneSimulateEvent>([this](auto& e) {
			m_SceneManager->SetSceneState(SceneState::Simulate);
			return true;
		});
		dispatcher.Dispatch<SceneStopEvent>([this](auto& e) {
			m_SceneManager->SetSceneState(SceneState::Edit);
			return true;
		});
		dispatcher.Dispatch<SceneModifiedEvent>([this](auto& e) {
			m_SceneManager->MarkSceneDirty();
			return true;
		});

		// 2. Project Management
		dispatcher.Dispatch<ProjectOpenedEvent>([this](auto& e) { return m_ProjectManager->OnProjectOpened(e); });
		dispatcher.Dispatch<AppLaunchRuntimeEvent>([this](auto& e) {
			if (GetSceneState() != SceneState::Play)
			{
				m_SceneManager->SetSceneState(SceneState::Play);
			}
			else
			{
				m_SceneManager->SetSceneState(SceneState::Edit);
			}
			return true;
		});

		// 3. Keyboard shortcuts
		dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& e) { return HandleKeyboardShortcut(e); });

		// 4. Layout/System
		dispatcher.Dispatch<AppResetLayoutEvent>([this](auto& ev) {
			ResetLayout();
			return true;
		});
		dispatcher.Dispatch<SceneChangeRequestEvent>([this](auto& ev) {
			std::filesystem::path scenePath = ev.GetPath();
			if (scenePath.is_relative() && Project::GetActive())
			{
				scenePath = Project::GetActive()->GetAssetPath(ev.GetPath());
			}
			m_PendingSceneTransitionPath = scenePath.string();
			return true;
		});

		// 5. Selections/Picking
		dispatcher.Dispatch<EntitySelectedEvent>([this](auto& ev) {
			if (Scene* scene = ev.GetScene())
			{
				SetSelectedEntity(Entity(ev.GetEntity(), &scene->GetRegistry()));
			}
			GetEditorState().LastHitMeshIndex = ev.GetMeshIndex();
			return false;
		});

		// 6. Raw Input Overrides
		if (e.GetEventType() == EventType::KeyPressed)
		{
			auto& ke = (KeyPressedEvent&)e;
			if (ke.GetKeyCode() == KeyCode::Escape && GetEditorState().FullscreenGame)
			{
				GetEditorState().FullscreenGame = false;
				e.Handled = true;
			}
			else if (ke.GetKeyCode() == KeyCode::F11)
			{
				Application::Get().GetWindow().ToggleFullscreen();
				e.Handled = true;
			}
		}
	}

	bool EditorLayer::HandleKeyboardShortcut(KeyPressedEvent& e)
	{
		if (e.IsRepeat())
		{
			return false;
		}

		bool ctrl = Core::Input::IsKeyDown(KeyCode::LeftControl) || Core::Input::IsKeyDown(KeyCode::RightControl);
		bool shift = Core::Input::IsKeyDown(KeyCode::LeftShift) || Core::Input::IsKeyDown(KeyCode::RightShift);
		auto keyCode = e.GetKeyCode();

		if (ctrl)
		{
			switch (keyCode)
			{
			case KeyCode::N:
				if (GetSceneState() != SceneState::Play)
				{
					m_SceneManager->NewScene();
				}
				return true;
			case KeyCode::O:
				if (GetSceneState() != SceneState::Play)
				{
					m_SceneManager->OpenScene();
				}
				return true;
			case KeyCode::S:
				if (GetSceneState() != SceneState::Play)
				{
					shift ? m_SceneManager->SaveSceneAs() : m_SceneManager->SaveScene();
				}
				return true;
			case KeyCode::Z:
				if (GetSceneState() != SceneState::Play)
				{
					m_CommandHistory.Undo();
				}
				return true;
			case KeyCode::Y:
				if (GetSceneState() != SceneState::Play)
				{
					m_CommandHistory.Redo();
				}
				return true;
			}
		}

		if (keyCode == KeyCode::F5)
		{
			m_ProjectManager->LaunchStandalone(m_SceneManager->GetActiveScene());
			return true;
		}

		return false;
	}

	CommandHistory& EditorLayer::GetCommandHistory()
	{
		return m_CommandHistory;
	}

} // namespace Chained
