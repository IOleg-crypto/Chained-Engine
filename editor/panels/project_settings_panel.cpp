#include "project_settings_panel.h"
#include "engine/core/platform.h"
#include "engine/platform/dialogs/dialogs.h"
#include "engine/project/project.h"
#include "imgui.h"
#include "layer.h"
#include "project/project_serializer.h"
#include "project_manager.h"
#include "thirdparty/IconsFontAwesome6.h"
#include <filesystem>
#include <misc/cpp/imgui_stdlib.h>

namespace Chained
{
	ProjectSettingsPanel::ProjectSettingsPanel()
	{
		m_Name = "Project Settings";
		m_IsOpen = false;
	}

	void ProjectSettingsPanel::OnImGuiRender(bool readOnly)
	{
		if (!m_IsOpen)
		{
			return;
		}

		auto project = Project::GetActive();
		if (!project)
		{
			m_IsOpen = false;
			return;
		}

		if (ImGui::Begin("Project Settings", &m_IsOpen))
		{
			auto& config = project->GetConfig();

			const char* categories[] = {ICON_FA_GEARS " General",
										ICON_FA_CODE " Scripting",
										ICON_FA_CUBES " Physics",
										ICON_FA_WINDOW_RESTORE " Window",
										ICON_FA_MOUNTAIN_SUN " Rendering",
										ICON_FA_VOLUME_HIGH " Audio",
										ICON_FA_CUBE " Mesh",
										ICON_FA_PLAY " Runtime"};

			// Two-column layout: sidebar left, content right
			ImGui::Columns(2, "ProjectSettingsColumns", true);

			if (!m_WidthSet)
			{
				ImGui::SetColumnWidth(0, 200.0f);
				m_WidthSet = true;
			}

			// --- Left sidebar ---
			ImGui::BeginChild("SettingsSidebar", ImVec2(0, 0), ImGuiChildFlags_NavFlattened);
			for (int i = 0; i < IM_ARRAYSIZE(categories); i++)
			{
				if (ImGui::Selectable(categories[i], m_SelectedCategory == i, ImGuiSelectableFlags_DontClosePopups))
				{
					m_SelectedCategory = i;
				}
			}
			ImGui::EndChild();

			ImGui::NextColumn();

			// --- Right content panel ---
			ImGui::BeginChild("SettingsContent", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()),
							  ImGuiChildFlags_NavFlattened);
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));

			switch (m_SelectedCategory)
			{
			case 0: // General
			{
				ImGui::TextDisabled("General Settings");
				ImGui::Separator();
				ImGui::Spacing();

				char nameBuf[256];
				snprintf(nameBuf, sizeof(nameBuf), "%s", config.Name.c_str());
				if (ImGui::InputText("Project Name", nameBuf, sizeof(nameBuf)))
				{
					config.Name = nameBuf;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Display name of the project");
				}

				char iconBuf[512];
				snprintf(iconBuf, sizeof(iconBuf), "%s", config.IconPath.c_str());
				if (ImGui::InputText("Icon Path", iconBuf, sizeof(iconBuf)))
				{
					config.IconPath = iconBuf;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Path to the project icon image");
				}
				ImGui::SameLine();
				if (ImGui::Button("...###IconBrowse"))
				{
					std::vector<DialogFilter> filters = {{"Image Files", "png,jpg,jpeg"}};
					auto result = Chained::Dialogs::OpenFile(filters);
					if (result)
					{
						config.IconPath = project->GetRelativePath(result->string());
					}
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Browse for an icon image file");
				}

				auto availableScenes = project->GetAvailableScenes();
				const char* currentScene = config.StartScene.c_str();

				if (ImGui::BeginCombo("Start Scene", currentScene))
				{
					for (const auto& scenePath : availableScenes)
					{
						bool isSelected = (config.StartScene == scenePath);
						if (ImGui::Selectable(scenePath.c_str(), isSelected))
						{
							config.StartScene = scenePath;
						}
						if (isSelected)
						{
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Scene that loads when the game starts");
				}
				break;
			}
			case 1: // Scripting
			{
				ImGui::TextDisabled("Scripting Settings");
				ImGui::Separator();
				ImGui::Spacing();

				char moduleNameBuf[256];
				snprintf(moduleNameBuf, sizeof(moduleNameBuf), "%s", config.Scripting.ModuleName.c_str());
				if (ImGui::InputText("Module Name", moduleNameBuf, sizeof(moduleNameBuf)))
				{
					config.Scripting.ModuleName = moduleNameBuf;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Name of the compiled script module DLL");
				}

				char moduleDirBuf[512];
				snprintf(moduleDirBuf, sizeof(moduleDirBuf), "%s", config.Scripting.ModuleDirectory.string().c_str());
				if (ImGui::InputText("Module Directory", moduleDirBuf, sizeof(moduleDirBuf)))
				{
					config.Scripting.ModuleDirectory = moduleDirBuf;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Directory containing the script module source");
				}
				ImGui::SameLine();
				if (ImGui::Button("...###ModuleDirBrowse"))
				{
					auto result = Chained::Dialogs::PickFolder();
					if (result)
					{
						config.Scripting.ModuleDirectory = project->GetRelativePath(result->string());
					}
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Browse for the module directory");
				}

				ImGui::Checkbox("Auto Load Module", &config.Scripting.AutoLoad);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Automatically load the script module on startup");
				}
				break;
			}
			case 2: // Physics
			{
				ImGui::TextDisabled("Physics Settings");
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::DragFloat("World Gravity", &config.Physics.Gravity, 0.1f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Gravitational acceleration (m/s^2, negative = downward)");
				}
				ImGui::DragFloat("Fixed Timestep", &config.Physics.FixedTimestep, 0.001f, 0.001f, 0.1f, "%.4f");
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Physics simulation step in seconds (lower = more precise)");
				}
				break;
			}
			case 3: // Window
			{
				ImGui::TextDisabled("Window Settings");
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::DragInt("Width", &config.Window.Width, 1, 800, 3840);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Window width in pixels");
				}
				ImGui::DragInt("Height", &config.Window.Height, 1, 600, 2160);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Window height in pixels");
				}
				ImGui::Checkbox("VSync", &config.Window.VSync);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Synchronize frame rate to display refresh rate");
				}
				break;
			}
			case 4: // Rendering
			{
				ImGui::TextDisabled("Rendering Settings");
				ImGui::Separator();
				ImGui::TextDisabled("Visual Quality");

				static constexpr int kShadowResValues[] = {512, 1024, 2048, 4096};
				static constexpr int kShadowResCount = sizeof(kShadowResValues) / sizeof(kShadowResValues[0]);
				const char* shadowResNames[kShadowResCount] = {"512", "1024", "2048", "4096"};
				int currentShadowResIdx = 2; // Default 2048
				for (int i = 0; i < kShadowResCount; i++)
				{
					if (config.Render.ShadowResolution == kShadowResValues[i])
					{
						currentShadowResIdx = i;
						break;
					}
				}

				if (ImGui::Combo("Shadow Resolution", &currentShadowResIdx, shadowResNames, kShadowResCount))
				{
					config.Render.ShadowResolution = kShadowResValues[currentShadowResIdx];
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Shadow map resolution in pixels (higher = better quality)");
				}

				static constexpr int kAAValues[] = {0, 2, 4, 8, 16};
				static constexpr int kAACount = sizeof(kAAValues) / sizeof(kAAValues[0]);
				const char* aaNames[kAACount] = {"None", "2x MSAA", "4x MSAA", "8x MSAA", "16x MSAA"};
				int currentAAIdx = 0;
				for (int i = 0; i < kAACount; i++)
				{
					if (config.Render.AntiAliasingSamples == kAAValues[i])
					{
						currentAAIdx = i;
						break;
					}
				}

				if (ImGui::Combo("Anti-Aliasing", &currentAAIdx, aaNames, kAACount))
				{
					config.Render.AntiAliasingSamples = kAAValues[currentAAIdx];
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Multisample anti-aliasing level (higher = smoother edges)");
				}

				ImGui::Checkbox("Enable Shadows", &config.Render.EnableShadows);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Enable or disable shadow rendering");
				}
				break;
			}
			case 5: // Audio
			{
				ImGui::TextDisabled("Audio Settings");
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::SliderFloat("Master Volume", &config.Audio.MasterVolume, 0.0f, 1.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Overall volume control for all audio");
				}
				ImGui::SliderFloat("Music Volume", &config.Audio.MusicVolume, 0.0f, 1.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Volume level for background music");
				}
				ImGui::SliderFloat("SFX Volume", &config.Audio.SFXVolume, 0.0f, 1.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Volume level for sound effects");
				}
				break;
			}
			case 6: // Mesh
			{
				ImGui::TextDisabled("Mesh Import Settings");
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::Checkbox("Import Materials", &config.Mesh.ImportMaterials);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Automatically import materials when loading meshes");
				}

				ImGui::Checkbox("Calculate Tangents", &config.Mesh.CalculateTangents);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Calculate tangent vectors for normal mapping");
				}

				ImGui::Checkbox("Flip UVs", &config.Mesh.FlipUVs);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Flip texture UV coordinates vertically on import");
				}
				break;
			}
			case 7: // Runtime
			{
				ImGui::TextDisabled("Runtime Settings");
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::Checkbox("Fullscreen", &config.Runtime.Fullscreen);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Start the game in fullscreen mode");
				}

				ImGui::Checkbox("Show Stats Overlay", &config.Runtime.ShowStats);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Display FPS and performance stats in the runtime window");
				}

				ImGui::Checkbox("Enable Console", &config.Runtime.EnableConsole);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Enable the in-game developer console");
				}

				ImGui::DragInt("Target FPS", &config.Runtime.TargetFPS, 1, 0, 240);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("0 = Uncapped framerate");
				}
				break;
			}
			}

			ImGui::PopStyleVar();
			ImGui::EndChild();

			ImGui::Columns(1);
			ImGui::Separator();

			float buttonWidth =
				ImGui::CalcTextSize("Save Project Settings").x + ImGui::GetStyle().FramePadding.x * 2.0f;
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - buttonWidth);

			if (ImGui::Button("Save Project Settings"))
			{
				std::filesystem::path path =
					project->GetConfig().ProjectDirectory / (project->GetName() + ".chproject");
				EditorProjectSerializer::Serialize(project, path);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Save all project settings to disk");
			}
		}
		ImGui::End();
	}
} // namespace Chained
