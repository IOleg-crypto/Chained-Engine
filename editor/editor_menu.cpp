#include "editor_menu.h"
#include "editor/editor_colors.h"
#include "editor/layer.h"
#include "editor/panels.h"
#include "editor/project/project_exporter.h"
#include "engine/app/application.h"
#include "engine/common/thread_pool.h"
#include "engine/core/service_locator.h"
#include "engine/platform/dialogs/dialogs.h"
#include "engine/project/project.h"
#include "events.h"
#include "thirdparty/IconsFontAwesome6.h"
#include "engine/scripting/scriptengine.h"
#include "editor/scene_manager.h"
#include "gui.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "engine/assets/asset_manager.h"
#include "editor/font_choice_gui.h"

#include <filesystem>

namespace Chained
{
	constexpr float kPlaybackBarWidth = 330.0f;

	void EditorMenu::DrawMenuBar(EditorPanels& panels)
	{
		if (!ImGui::BeginMenuBar())
		{
			return;
		}

		DrawFileMenu();
		DrawViewMenu(panels);
		DrawProjectMenu();
		DrawEditorMenu();
		DrawPlaybackControls();
		DrawExportResultPopup();
		DrawUnsavedChangesPopup();

		ImGui::EndMenuBar();
	}

	void EditorMenu::DrawFileMenu()
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem(ICON_FA_FILE " New Project", "Ctrl+Shift+N"))
			{
				auto newScene = Scene::CreateDefault();
				EditorLayer::Get().GetSceneManager().SetScene(newScene);
			}
			if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Open Project", "Ctrl+O"))
			{
				std::vector<DialogFilter> filters = {{"Chained Scene", "chscene"}};
				auto result = Chained::Dialogs::OpenFile(filters);
				if (result)
				{
					EditorLayer::Get().GetSceneManager().OpenScene(*result);
				}
			}
			if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save Project"))
			{
				EditorLayer::Get().GetSceneManager().SaveScene();
			}
			if (ImGui::MenuItem(ICON_FA_XMARK " Close Project"))
			{
				Project::SetActive(nullptr);
			}
			ImGui::Separator();
			if (ImGui::MenuItem(ICON_FA_FILE_CODE " New Scene", "Ctrl+N"))
			{
				EditorLayer::Get().GetSceneManager().NewScene();
			}
			if (ImGui::MenuItem(ICON_FA_FLOPPY_DISK " Save Scene", "Ctrl+S"))
			{
				EditorLayer::Get().GetSceneManager().SaveScene();
			}
			if (ImGui::MenuItem(ICON_FA_FILE_EXPORT " Save Scene As...", "Ctrl+Shift+S"))
			{
				EditorLayer::Get().GetSceneManager().SaveSceneAs();
			}
			if (ImGui::MenuItem(ICON_FA_FOLDER_OPEN " Load Scene", "Ctrl+L"))
			{
				EditorLayer::Get().GetSceneManager().OpenScene();
			}
			ImGui::Separator();
			if (ImGui::MenuItem(ICON_FA_POWER_OFF " Exit"))
			{
				Application::Get().Close();
			}
			ImGui::EndMenu();
		}
	}

	void EditorMenu::DrawViewMenu(EditorPanels& panels)
	{
		if (ImGui::BeginMenu("View"))
		{
			panels.ForEach([](const std::shared_ptr<Panel>& panel) {
				if (panel->GetName() != "Project Browser")
				{
					ImGui::MenuItem(panel->GetName().c_str(), nullptr, &panel->IsOpen());
				}
			});
			ImGui::Separator();
			if (ImGui::MenuItem(ICON_FA_EXPAND " Fullscreen", "F11"))
			{
				Application::Get().GetWindow().ToggleFullscreen();
			}
			ImGui::EndMenu();
		}
	}

	void EditorMenu::DrawProjectMenu()
	{
		if (ImGui::BeginMenu("Project"))
		{
			if (ImGui::MenuItem(ICON_FA_GEARS " Settings"))
			{
				if (auto p = EditorLayer::Get().GetPanels().Get("Project Settings"))
				{
					p->IsOpen() = true;
				}
			}
			bool isExporting = false;
			{
				std::lock_guard<std::mutex> lock(m_ExportState.Mutex);
				isExporting = m_ExportState.IsExporting;
			}

			if (ImGui::MenuItem(isExporting ? ICON_FA_FILE_EXPORT " Exporting..."
											: ICON_FA_FILE_EXPORT " Export Project..."))
			{
				if (!isExporting)
				{
					m_ExportState.CancelRequested.store(false, std::memory_order_relaxed);
					m_ExportDialog.Open = true;
					auto project = Project::GetActive();
					if (project)
					{
						m_ExportDialog.SelectedMode = project->GetConfig().Export.Mode;
						m_ExportDialog.ZipThreshold = project->GetConfig().Export.ZipThreshold;
						m_ExportDialog.DataVersion = project->GetConfig().Export.DataVersion;
						m_ExportDialog.SplitSizeMB = project->GetConfig().Export.SplitSizeMB;
						uint32_t splitSize = m_ExportDialog.SplitSizeMB;
						m_ExportDialog.SplitCustom =
							(splitSize != 0 && splitSize != 512 && splitSize != 1024 && splitSize != 2048);
						m_ExportDialog.PackName = project->GetConfig().Export.PackName;
					}
				}
			}
			ImGui::Separator();
			if (ImGui::MenuItem(ICON_FA_ARROWS_ROTATE " Reload Shaders"))
			{
				if (auto* renderer = ServiceLocator::TryGet<Renderer>())
				{
					renderer->GetShaderLibrary().ReloadAll();
				}
			}
			if (ImGui::MenuItem(ICON_FA_ARROWS_ROTATE " Reload All Stale Assets"))
			{
				auto* am = ServiceLocator::TryGet<AssetManager>();
				if (am)
				{
					size_t count = am->ReloadAllStale();
					CH_CORE_INFO("EditorMenu: Reloaded {} stale assets", count);
				}
			}
			if (ImGui::MenuItem(ICON_FA_TRASH " Clear .chasset Cache"))
			{
				auto* am = ServiceLocator::TryGet<AssetManager>();
				if (am)
				{
					size_t count = am->DeleteAllChassets();
					CH_CORE_INFO("EditorMenu: Deleted {} .chasset file(s)", count);
				}
			}
			if (ImGui::MenuItem(ICON_FA_FILE_CODE " Reload Scripts", "Ctrl+R"))
			{
				auto project = Project::GetActive();
				if (project)
				{
					auto assemblyPath = ScriptEngine::ResolveAssemblyPath(project->GetConfig().Scripting,
																		  project->GetConfig().ProjectDirectory);
					if (auto* scriptEngine = ServiceLocator::TryGet<ScriptEngine>())
					{
						scriptEngine->RequestAssemblyReload(assemblyPath.string(), "EditorGUI");
					}
				}
			}
			ImGui::EndMenu();
		}
	}

	void EditorMenu::DrawEditorMenu()
	{
		if (ImGui::BeginMenu("Editor"))
		{
			if (ImGui::MenuItem(ICON_FA_SLIDERS " Settings"))
			{
				m_ShowEditorSettings = true;
			}
			ImGui::EndMenu();
		}
	}

	void EditorMenu::DrawPlaybackControls()
	{
		float barWidth = ImGui::GetWindowWidth();
		float centerPos = (barWidth - kPlaybackBarWidth) * 0.5f;
		if (centerPos > ImGui::GetCursorPosX())
		{
			ImGui::SameLine(centerPos);
		}

		SceneState sceneState = EditorLayer::Get().GetSceneManager().GetSceneState();
		bool isPlaying = (sceneState == SceneState::Play);
		bool isSimulating = (sceneState == SceneState::Simulate);

		// Play / Stop Button
		if (isPlaying)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::PlayGreen);
		}
		if (ImGui::Button(isPlaying ? (ICON_FA_STOP " Stop") : (ICON_FA_PLAY " Play"), ImVec2(65, 20)))
		{
			EditorLayer::Get().GetSceneManager().SetSceneState(isPlaying ? SceneState::Edit : SceneState::Play);
		}
		if (isPlaying)
		{
			ImGui::PopStyleColor();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(isPlaying ? "Stop Game" : "Play Game (Run Physics & Scripts)");
		}

		ImGui::SameLine(0, 5);

		// Simulate / Stop Button
		if (isSimulating)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::SimulateOrange);
		}
		if (ImGui::Button(isSimulating ? (ICON_FA_STOP " Stop") : (ICON_FA_GEARS " Simulate"), ImVec2(80, 20)))
		{
			EditorLayer::Get().GetSceneManager().SetSceneState(isSimulating ? SceneState::Edit : SceneState::Simulate);
		}
		if (isSimulating)
		{
			ImGui::PopStyleColor();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip(isSimulating ? "Stop Simulation" : "Simulate (Physics Only)");
		}
	}

	void EditorMenu::DrawExportResultPopup()
	{
		bool shouldOpenExportPopup = false;
		{
			std::lock_guard<std::mutex> lock(m_ExportState.Mutex);
			if (m_ExportState.Open)
			{
				m_ExportResultSuccess = m_ExportState.Success;
				m_ExportResultMessage = m_ExportState.Message;
				m_ExportResultOutDir = m_ExportState.OutDir;
				m_ExportState.Open = false;
				shouldOpenExportPopup = true;
			}
		}
		if (shouldOpenExportPopup)
		{
			ImGui::OpenPopup("Export Result");
		}
		if (ImGui::BeginPopupModal("Export Result", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			if (m_ExportResultSuccess)
			{
				ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), ICON_FA_CIRCLE_INFO " Success");
			}
			else
			{
				ImGui::TextColored(ImVec4(0.9f, 0.3f, 0.3f, 1.0f), ICON_FA_CIRCLE_EXCLAMATION " Failed");
			}
			ImGui::Spacing();
			ImGui::TextWrapped("%s", m_ExportResultMessage.c_str());
			if (!m_ExportResultOutDir.empty())
			{
				ImGui::Spacing();
				ImGui::Text("Output: ");
				ImGui::SameLine();
				ImGui::TextDisabled("%s", m_ExportResultOutDir.c_str());
			}
			ImGui::Spacing();
			ImGui::Separator();
			if (ImGui::Button("OK", ImVec2(120.f, 0.f)) || ImGui::IsKeyPressed(ImGuiKey_Escape) ||
				ImGui::IsKeyPressed(ImGuiKey_Enter))
			{
				m_ExportState.CancelRequested.store(false, std::memory_order_relaxed);
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	void EditorMenu::DrawUnsavedChangesPopup()
	{
		auto& sceneMgr = EditorLayer::Get().GetSceneManager();
		if (sceneMgr.IsConfirmPending())
		{
			ImGui::OpenPopup("Unsaved Changes");
		}
		if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Scene has unsaved changes.");
			ImGui::Spacing();
			ImGui::TextDisabled("Do you want to save before continuing?");
			ImGui::Spacing();
			ImGui::Separator();

			if (ImGui::Button("Save", ImVec2(120.f, 0.f)))
			{
				sceneMgr.SaveScene();
				sceneMgr.ConfirmPendingAction();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Don't Save", ImVec2(120.f, 0.f)))
			{
				sceneMgr.ConfirmPendingAction();
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120.f, 0.f)))
			{
				sceneMgr.CancelPendingAction();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	void EditorMenu::DrawExportDialog()
	{
		if (!m_ExportDialog.Open)
		{
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(480, 0), ImGuiCond_Once);
		if (ImGui::Begin("Export Project", &m_ExportDialog.Open))
		{
			ImGui::TextUnformatted("Choose export mode:");
			ImGui::Spacing();

			struct ModeInfo
			{
				PackMode mode;
				const char* label;
				const char* desc;
				const char* icon;
			};
			ModeInfo modes[] = {
				{PackMode::Fast, "Fast", "LZ4 HC compression.\nFast export, larger pack.", ICON_FA_BOLT},
				{PackMode::Balanced, "Balanced", "ZSTD compression.\nBalanced speed and size.", ICON_FA_CUBES},
				{PackMode::Max, "Max", "ZSTD ultra compression.\nSmallest pack, slowest export.", ICON_FA_GEARS},
				{PackMode::Raw, "Raw", "No compression.\nStored as-is, fastest.", ICON_FA_FOLDER_OPEN},
			};
			constexpr size_t modeCount = 4;

			const float avail = ImGui::GetContentRegionAvail().x;
			const float spacing = ImGui::GetStyle().ItemSpacing.x;
			const float modeWidth = (avail - spacing * 3.0f) / 4.0f;

			for (size_t i = 0; i < modeCount; ++i)
			{
				const auto& m = modes[i];
				const bool selected = (m_ExportDialog.SelectedMode == m.mode);
				ImGui::PushID(static_cast<int>(m.mode));

				ImGui::PushStyleColor(ImGuiCol_Button,
									  selected ? ImVec4(0.20f, 0.62f, 0.78f, 1.0f) : ImVec4(0.18f, 0.20f, 0.24f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
									  selected ? ImVec4(0.28f, 0.72f, 0.88f, 1.0f) : ImVec4(0.22f, 0.24f, 0.29f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive,
									  selected ? ImVec4(0.16f, 0.52f, 0.68f, 1.0f) : ImVec4(0.14f, 0.16f, 0.20f, 1.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 12.0f));

				std::string btnLabel = std::string(m.icon) + " " + m.label;
				if (ImGui::Button(btnLabel.c_str(), ImVec2(modeWidth, 0)))
				{
					m_ExportDialog.SelectedMode = m.mode;
					if (m.mode == PackMode::Fast)
					{
						m_ExportDialog.ZipThreshold = 0.10f;
					}
					else if (m.mode == PackMode::Balanced)
					{
						m_ExportDialog.ZipThreshold = 0.05f;
					}
					else if (m.mode == PackMode::Max)
					{
						m_ExportDialog.ZipThreshold = 0.00f;
					}
				}

				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", m.desc);
				}

				ImGui::PopStyleVar();
				ImGui::PopStyleColor(3);
				ImGui::PopID();

				if (i + 1 < modeCount)
				{
					ImGui::SameLine();
				}
			}

			// Description for selected mode
			ImGui::Spacing();
			for (size_t i = 0; i < modeCount; ++i)
			{
				if (m_ExportDialog.SelectedMode == modes[i].mode)
				{
					ImGui::TextColored(ImVec4(0.6f, 0.75f, 0.85f, 1.0f), "%s", modes[i].desc);
					break;
				}
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
			if (m_ExportDialog.SelectedMode != PackMode::Raw)
			{
				int displayPercent =
					std::clamp(100 - static_cast<int>(std::round(m_ExportDialog.ZipThreshold * 100.0f)), 0, 100);
				if (ImGui::SliderInt("Compression Level", &displayPercent, 0, 100, "%d%%"))
				{
					m_ExportDialog.ZipThreshold = static_cast<float>(100 - displayPercent) * 0.01f;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Compression level / aggressiveness.\n"
									  "100%% = compress all files (smallest pack size).\n"
									  "Lower values skip compression for already-compressed assets (e.g. PNG, OGG).\n"
									  "0%% = store uncompressed.");
				}
			}

			{
				int dataVersion = static_cast<int>(m_ExportDialog.DataVersion);
				if (ImGui::InputInt("Data Version", &dataVersion))
				{
					m_ExportDialog.DataVersion = static_cast<uint32_t>(dataVersion);
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Increment to invalidate cached packs at runtime.");
				}
			}

			if (m_ExportDialog.SelectedMode != PackMode::Raw)
			{
				// Reserve 128 chars for the pack name buffer
				static char packNameBuf[128] = {};
				if (ImGui::IsWindowAppearing())
				{
					std::strncpy(packNameBuf, m_ExportDialog.PackName.c_str(), sizeof(packNameBuf) - 1);
					packNameBuf[sizeof(packNameBuf) - 1] = '\0';
				}
				if (ImGui::InputText("Pack Name", packNameBuf, sizeof(packNameBuf)))
				{
					std::string newName = packNameBuf;
					// Strip spaces and extension characters that would break the filename
					newName.erase(std::remove_if(newName.begin(), newName.end(),
												 [](char c) { return c == '.' || c == '/' || c == '\\' || c == ':'; }),
								  newName.end());
					if (!newName.empty())
					{
						m_ExportDialog.PackName = newName;
					}
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Base name for the .pack file(s).\nResult: %s.pack, %s_1.pack, ...",
									  m_ExportDialog.PackName.c_str(), m_ExportDialog.PackName.c_str());
				}
			}

			if (m_ExportDialog.SelectedMode != PackMode::Raw)
			{
				const char* splitLabels[] = {"Single File (No Split)", "512 MB per pack", "1 GB (1024 MB)",
											 "2 GB (2048 MB)", "Custom Size..."};

				// Determine which preset slot is active, respecting the explicit custom flag
				int currentSplitIdx = 0;
				if (m_ExportDialog.SplitCustom)
				{
					currentSplitIdx = 4;
				}
				else if (m_ExportDialog.SplitSizeMB == 0)
				{
					currentSplitIdx = 0;
				}
				else if (m_ExportDialog.SplitSizeMB == 512)
				{
					currentSplitIdx = 1;
				}
				else if (m_ExportDialog.SplitSizeMB == 1024)
				{
					currentSplitIdx = 2;
				}
				else if (m_ExportDialog.SplitSizeMB == 2048)
				{
					currentSplitIdx = 3;
				}
				else
				{
					currentSplitIdx = 4; // unknown preset → treat as custom
				}

				if (ImGui::BeginCombo("Split Pack Size", splitLabels[currentSplitIdx]))
				{
					if (ImGui::Selectable(splitLabels[0], currentSplitIdx == 0))
					{
						m_ExportDialog.SplitSizeMB = 0;
						m_ExportDialog.SplitCustom = false;
					}
					if (ImGui::Selectable(splitLabels[1], currentSplitIdx == 1))
					{
						m_ExportDialog.SplitSizeMB = 512;
						m_ExportDialog.SplitCustom = false;
					}
					if (ImGui::Selectable(splitLabels[2], currentSplitIdx == 2))
					{
						m_ExportDialog.SplitSizeMB = 1024;
						m_ExportDialog.SplitCustom = false;
					}
					if (ImGui::Selectable(splitLabels[3], currentSplitIdx == 3))
					{
						m_ExportDialog.SplitSizeMB = 2048;
						m_ExportDialog.SplitCustom = false;
					}
					if (ImGui::Selectable(splitLabels[4], currentSplitIdx == 4))
					{
						m_ExportDialog.SplitCustom = true;
						// Keep current SplitSizeMB value so the user sees a sensible default
						if (m_ExportDialog.SplitSizeMB == 0)
						{
							m_ExportDialog.SplitSizeMB = 1024;
						}
					}
					ImGui::EndCombo();
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Split exported assets across multiple .pack files (%s.pack, %s_1.pack, etc.).",
									  m_ExportDialog.PackName.c_str(), m_ExportDialog.PackName.c_str());
				}

				// Show custom input only when "Custom Size..." is selected
				if (m_ExportDialog.SplitCustom)
				{
					int customMB = static_cast<int>(m_ExportDialog.SplitSizeMB);
					if (ImGui::InputInt("Chunk Size (MB)", &customMB))
					{
						m_ExportDialog.SplitSizeMB = static_cast<uint32_t>(std::max(1, customMB));
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Maximum uncompressed size per chunk in MB.\nActual .pack file will be "
										  "smaller after compression.");
					}
				}
			}

			ImGui::Spacing();
			ImGui::Checkbox("Force repack", &m_ExportDialog.ForceRepack);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Rebuild resources.pack even when it is already up to date.\nBy default the pack is "
								  "reused if no source file changed.");
			}
			ImGui::Checkbox("Skip KTX2 conversion", &m_ExportDialog.SkipKtx2);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Keep textures in original format (PNG/JPG).\nFaster export but larger pack.\n"
								  "Enable if your GPU doesn't support BC7/KTX2.");
			}
			ImGui::PopStyleVar();

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			ImGui::TextDisabled("Output folder");
			if (ImGui::Button("Browse Output Folder...", ImVec2(-1, 0)))
			{
				auto outDir = Dialogs::PickFolder();
				if (outDir)
				{
					m_ExportDialog.OutputDir = outDir->string();

					// Save export settings to project config
					auto project = Project::GetActive();
					if (project)
					{
						project->GetConfig().Export.Mode = m_ExportDialog.SelectedMode;
						project->GetConfig().Export.ZipThreshold = m_ExportDialog.ZipThreshold;
						project->GetConfig().Export.DataVersion = m_ExportDialog.DataVersion;
						project->GetConfig().Export.SplitSizeMB = m_ExportDialog.SplitSizeMB;
						project->GetConfig().Export.PackName = m_ExportDialog.PackName;
					}

					m_ExportDialog.Open = false;

					// Start the export
					{
						std::lock_guard<std::mutex> lock(m_ExportState.Mutex);
						m_ExportState.IsExporting = true;
						m_ExportState.PackedFiles = 0;
						m_ExportState.TotalFiles = 0;
						m_ExportState.CurrentFile.clear();
						m_ExportState.StartTime = std::chrono::steady_clock::now();
					}
					m_ExportState.CancelRequested.store(false, std::memory_order_relaxed);

					auto* threadPool = ServiceLocator::TryGet<ThreadPool>();
					if (!threadPool)
					{
						CH_CORE_ERROR("EditorMenu: ThreadPool not available, cannot export");
						std::lock_guard<std::mutex> lock(m_ExportState.Mutex);
						m_ExportState.IsExporting = false;
					}
					else
					{
						std::string outDirPath = m_ExportDialog.OutputDir;
						bool forceRepack = m_ExportDialog.ForceRepack;
						bool skipKtx2 = m_ExportDialog.SkipKtx2;
						threadPool->QueueTask([outDirPath, forceRepack, skipKtx2, this]() {
							ExportProgressCallback progressCb = [this](uint64_t packed, uint64_t total,
																	   const std::string& file) {
								std::lock_guard<std::mutex> lock(m_ExportState.Mutex);
								m_ExportState.PackedFiles = packed;
								m_ExportState.TotalFiles = total;
								m_ExportState.CurrentFile = file;
							};
							auto result = ProjectExporter::ExportTo(
								outDirPath, progressCb, &m_ExportState.CancelRequested, forceRepack, skipKtx2);
							std::lock_guard<std::mutex> lock(m_ExportState.Mutex);
							m_ExportState.Success = result.Success;
							m_ExportState.Message = result.Cancelled  ? "Export cancelled."
													: !result.Success ? ("Export failed: " + result.Error)
													: result.PackSkipped
														? "Export complete! (pack reused — no asset changes)"
														: "Export complete!";
							m_ExportState.OutDir = result.Cancelled ? "" : result.OutDir.string();
							m_ExportState.Open = true;
							m_ExportState.IsExporting = false;
						});
					}
				}
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip(
					"Choose a folder and start exporting.\nExisting settings are saved to the project config.");
			}

			ImGui::End();
		}
	}

	void EditorMenu::DrawExportProgressOverlay()
	{
		bool showProgress = false;
		uint64_t packed = 0, total = 0;
		std::string currentFile;
		std::chrono::steady_clock::time_point startTime;
		{
			std::lock_guard<std::mutex> lock(m_ExportState.Mutex);
			showProgress = m_ExportState.IsExporting;
			packed = m_ExportState.PackedFiles;
			total = m_ExportState.TotalFiles;
			currentFile = m_ExportState.CurrentFile;
			startTime = m_ExportState.StartTime;
		}

		if (!showProgress)
		{
			return;
		}

		// Position: bottom-right corner with a small margin.
		ImGuiViewport* vp = ImGui::GetMainViewport();
		const float margin = 16.0f;
		const float windowW = 420.0f;
		ImVec2 winPos =
			ImVec2(vp->WorkPos.x + vp->WorkSize.x - windowW - margin, vp->WorkPos.y + vp->WorkSize.y - margin);
		ImGui::SetNextWindowPos(winPos, ImGuiCond_Always, ImVec2(0.0f, 1.0f));
		ImGui::SetNextWindowSize(ImVec2(windowW, 0.0f), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.92f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));

		if (ImGui::Begin("##ExportProgress", nullptr,
						 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
							 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_AlwaysAutoResize))
		{
			// ── Title
			ImGui::TextColored(ImVec4(0.55f, 0.85f, 1.0f, 1.0f), ICON_FA_FILE_EXPORT "  Exporting Project");
			ImGui::Spacing();

			// ── File counter & ETA
			if (total > 0)
			{
				float pct = (static_cast<float>(packed) / static_cast<float>(total)) * 100.0f;
				ImGui::Text("Packed %llu of %llu files (%.0f%%)", (unsigned long long)packed, (unsigned long long)total,
							pct);

				auto now = std::chrono::steady_clock::now();
				float elapsedSec = std::chrono::duration<float>(now - startTime).count();
				if (elapsedSec > 1.0f && packed > 3 && packed < total)
				{
					float filesPerSec = static_cast<float>(packed) / elapsedSec;
					if (filesPerSec > 0.01f)
					{
						float remainingSec = static_cast<float>(total - packed) / filesPerSec;
						int mins = static_cast<int>(remainingSec) / 60;
						int secs = static_cast<int>(remainingSec) % 60;

						ImGui::SameLine();
						if (mins > 0)
						{
							ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 0.85f), "|  ETA: ~%dm %02ds", mins, secs);
						}
						else
						{
							ImGui::TextColored(ImVec4(0.7f, 0.85f, 1.0f, 0.85f), "|  ETA: ~%ds", std::max(1, secs));
						}
					}
				}
			}
			else
			{
				ImGui::TextDisabled("Preparing export...");
			}

			ImGui::Spacing();

			// ── Progress bar
			float fraction = (total > 0) ? static_cast<float>(packed) / static_cast<float>(total) : 0.0f;
			ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.25f, 0.65f, 1.0f, 1.0f));
			ImGui::ProgressBar(fraction, ImVec2(-1.0f, 8.0f), "");
			ImGui::PopStyleColor();

			// ── Current file hint
			if (!currentFile.empty())
			{
				ImGui::Spacing();
				ImGui::TextDisabled("%s", currentFile.c_str());
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// ── Cancel button
			bool alreadyCancelling = m_ExportState.CancelRequested.load(std::memory_order_relaxed);
			if (alreadyCancelling)
			{
				ImGui::BeginDisabled();
			}

			if (ImGui::Button(alreadyCancelling ? ICON_FA_BOLT " Cancelling..." : ICON_FA_BOLT " Cancel",
							  ImVec2(-1.0f, 0.0f)))
			{
				m_ExportState.CancelRequested.store(true, std::memory_order_relaxed);
			}

			if (alreadyCancelling)
			{
				ImGui::EndDisabled();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Request cancellation of the current export.\nThe export will stop after the current "
								  "file finishes.");
			}
		}
		ImGui::End();
		ImGui::PopStyleVar(2);
	}

	void EditorMenu::DrawEditorSettings()
	{

		if (!m_ShowEditorSettings)
		{
			return;
		}

		ImGui::SetNextWindowSize(ImVec2(700, 480), ImGuiCond_FirstUseEver);
		auto& config = EditorLayer::Get().GetConfig();

		if (ImGui::Begin(ICON_FA_SLIDERS " Editor Settings", &m_ShowEditorSettings))
		{
			static int selectedCategory = 0;
			const char* categories[] = {ICON_FA_PALETTE " Appearance",	  ICON_FA_CAMERA " Camera",
										ICON_FA_VIDEO " Viewport",		  ICON_FA_IMAGE " Content Browser",
										ICON_FA_FLOPPY_DISK " Auto-Save", ICON_FA_ROCKET " Startup",
										ICON_FA_GEAR " General"};

			float buttonRowHeight = ImGui::GetFrameHeightWithSpacing();

			// --- Left sidebar ---
			ImGui::BeginChild("EditorSettingsSidebar", ImVec2(180, -buttonRowHeight), ImGuiChildFlags_NavFlattened);
			for (int i = 0; i < IM_ARRAYSIZE(categories); i++)
			{
				if (ImGui::Selectable(categories[i], selectedCategory == i))
				{
					selectedCategory = i;
				}
			}
			ImGui::EndChild();

			ImGui::SameLine();

			// --- Right content ---
			ImGui::BeginChild("EditorSettingsContent", ImVec2(0, -buttonRowHeight), ImGuiChildFlags_NavFlattened);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));

			if (selectedCategory == 0) // Appearance
			{
				ImGui::TextDisabled("Font");
				ImGui::Separator();
				ImGui::Spacing();

				const auto& fontChoices = GetEditorFontChoices();
				int currentFont = -1;
				for (int i = 0; i < (int)fontChoices.size(); i++)
				{
					if (config.FontPath == fontChoices[i].Path)
					{
						currentFont = i;
						break;
					}
				}
				const char* preview = currentFont >= 0 ? fontChoices[currentFont].Label.c_str() : "Custom";
				if (ImGui::BeginCombo("Editor Font", preview))
				{
					for (int i = 0; i < (int)fontChoices.size(); i++)
					{
						bool sel = (currentFont == i);
						if (ImGui::Selectable(fontChoices[i].Label.c_str(), sel))
						{
							config.FontPath = fontChoices[i].Path;
						}
						if (sel)
						{
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Choose the font used throughout the editor UI.");
				}

				ImGui::DragFloat("Font Size", &config.FontSize, 0.25f, 8.0f, 48.0f, "%.0f px");
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Base font size in pixels. Requires restart to take full effect.");
				}

				ImGui::Spacing();
				ImGui::Spacing();
				ImGui::TextDisabled("Viewport Icons");
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::DragFloat("Icon Scale", &config.IconSizeScale, 0.005f, 0.01f, 1.0f, "%.3f");
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("How fast gizmo icons grow with camera distance.");
				}
				ImGui::DragFloat("Icon Min Size", &config.IconSizeMin, 0.05f, 0.1f, config.IconSizeMax, "%.2f");
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Minimum on-screen size for viewport icons (in world units).");
				}
				ImGui::DragFloat("Icon Max Size", &config.IconSizeMax, 0.05f, config.IconSizeMin, 40.0f, "%.2f");
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Maximum on-screen size for viewport icons (in world units).");
				}
			}
			else if (selectedCategory == 1) // Camera
			{
				ImGui::TextDisabled("Editor Camera (Edit Mode)");
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::SliderFloat("Move Speed", &config.CameraMoveSpeed, 0.1f, 100.0f, "%.1f");
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Base camera movement speed in units per second.");
				}
				ImGui::SliderFloat("Boost Multiplier", &config.CameraBoostMultiplier, 1.0f, 10.0f, "%.1f");
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Speed multiplier when holding Shift.");
				}
				ImGui::SliderFloat("Rotation Speed", &config.CameraRotationSpeed, 0.1f, 5.0f, "%.1f");
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("How fast the camera rotates when holding right-click.");
				}
				ImGui::SliderFloat("Zoom Speed", &config.CameraZoomSpeedMultiplier, 0.1f, 5.0f, "%.1f");
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Multiplier for mouse wheel zoom speed.");
				}
				ImGui::DragFloat("FOV", &config.CameraFovDegrees, 0.5f, 20.0f, 120.0f, "%.1f deg");
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Vertical field of view for the editor camera.");
				}
				ImGui::DragFloat("Near Clip", &config.CameraNearClip, 0.01f, 0.001f, 10.0f, "%.3f");
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Distance to the near clipping plane.");
				}
				ImGui::DragFloat("Far Clip", &config.CameraFarClip, 100.0f, 100.0f, 100000.0f, "%.0f");
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Distance to the far clipping plane.");
				}
				ImGui::Checkbox("Disable Camera Zoom", &config.DisableCameraZoom);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Prevent the mouse wheel from zooming the editor camera.");
				}
			}
			else if (selectedCategory == 2) // Viewport
			{
				ImGui::TextDisabled("Viewport");
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::Checkbox("Show Editor Icons", &config.ShowEditorIcons);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Show camera, light, and spawn zone icons in the viewport.");
				}
				ImGui::DragFloat("Gizmo Scale", &config.GizmoScale, 0.05f, 0.5f, 3.0f, "%.2f");
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Scale of the transform gizmo in the viewport.");
				}
			}
			else if (selectedCategory == 3) // Content Browser
			{
				ImGui::TextDisabled("Content Browser");
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::DragFloat("Thumbnail Size", &config.DefaultThumbnailSize, 4.0f, 32.0f, 256.0f, "%.0f px");
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Size of asset thumbnails in the Content Browser.");
				}
				const char* sortNames[] = {"Name", "Date", "Size"};
				ImGui::Combo("Sort Order", &config.DefaultSortOrder, sortNames, 3);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("How assets are sorted in the Content Browser.");
				}
				ImGui::Checkbox("Show File Extensions", &config.ShowFileExtensions);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Display file extensions (e.g. .png, .ogg) in the Content Browser.");
				}
			}
			else if (selectedCategory == 4) // Auto-Save
			{
				ImGui::TextDisabled("Auto-Save");
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::Checkbox("Enable Auto-Save", &config.AutoSaveEnabled);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Automatically save the current scene at regular intervals.");
				}
				ImGui::DragFloat("Interval (s)", &config.AutoSaveInterval, 1.0f, 10.0f, 3600.0f, "%.0f");
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Time in seconds between auto-saves.");
				}
			}
			else if (selectedCategory == 5) // Startup
			{
				ImGui::TextDisabled("Startup");
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::Checkbox("Load Last Project on Startup", &config.LoadLastProjectOnStartup);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Automatically open the most recent project when the editor starts.");
				}
				ImGui::Spacing();
				ImGui::TextDisabled("Last project:");
				ImGui::SameLine();
				ImGui::TextWrapped("%s", config.LastProjectPath.empty() ? "(none)" : config.LastProjectPath.c_str());
			}
			else if (selectedCategory == 6) // General
			{
				ImGui::TextDisabled("General");
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::Checkbox("Confirm on Scene Close", &config.ConfirmOnSceneClose);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Show a warning when closing/switching a scene with unsaved changes.");
				}
				ImGui::DragInt("Max Recent Projects", &config.MaxRecentProjects, 1, 1, 50);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Maximum number of projects shown in the Recent Projects list.");
				}
			}

			ImGui::PopStyleVar();
			ImGui::EndChild();

			if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save Settings", ImVec2(-1, 0)))
			{
				EditorLayer::Get().SaveConfig();
				EditorGUI::ApplyTheme();
				EditorLayer::Get().RequestEditorFontReload();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Save and apply all settings.");
			}
		}
		ImGui::End();
	}

} // namespace Chained
