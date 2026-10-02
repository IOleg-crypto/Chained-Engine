#include "project_selector_ui.h"
#include "editor/editor_colors.h"
#include "editor/layer.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/types/texture_asset.h"
#include "engine/core/service_locator.h"
#include "engine/platform/dialogs/dialogs.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "thirdparty/IconsFontAwesome6.h"
#include <cstring>
#include <filesystem>
#include <string>

namespace Chained
{

	namespace
	{
		void SafeCopy(char* dst, size_t dstSize, const char* src)
		{
			strncpy(dst, src, dstSize - 1);
			dst[dstSize - 1] = '\0';
		}

		constexpr float kSidebarWidth = 320.0f;
		constexpr float kCardWidth = 300.0f;
		constexpr float kCardHeight = 300.0f;
		constexpr float kCardGap = 40.0f;
		constexpr float kCardTextWrap = 280.0f;
		constexpr float kPopupWidth = 480.0f;
		constexpr float kInputWidth = 432.0f;
		constexpr float kBtnWidth = 110.0f;
		constexpr float kBtnHeight = 28.0f;
		constexpr size_t kNameBufSize = 128;
		constexpr size_t kLocationBufSize = 256;
	} // namespace

	ProjectSelectorUI::ProjectSelectorUI(EditorProjectManager& projectManager)
		: m_ProjectManager(projectManager)
	{
	}

	void ProjectSelectorUI::LoadEditorIcons()
	{
		if (m_IconsLoaded)
		{
			return;
		}

		auto assetManager = ServiceLocator::TryGet<AssetManager>();
		if (assetManager)
		{
			assetManager->LoadAsset("engine/resources/icons/newproject.jpg", TextureAsset::GetStaticType());
			assetManager->LoadAsset("engine/resources/icons/folder.png", TextureAsset::GetStaticType());

			m_NewProjectIcon = assetManager->Get<TextureAsset>("engine/resources/icons/newproject.jpg");
			m_OpenProjectIcon = assetManager->Get<TextureAsset>("engine/resources/icons/folder.png");
			m_IconsLoaded = true;
		}
	}

	void ProjectSelectorUI::OnImGuiRender()
	{
		LoadEditorIcons();

		if (!m_Initialized)
		{
			std::string cwd = std::filesystem::current_path().string();
			SafeCopy(m_ProjectLocationBuffer, kLocationBufSize, cwd.c_str());
			m_Initialized = true;
		}

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
									   ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
									   ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoScrollbar |
									   ImGuiWindowFlags_NoBringToFrontOnFocus;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

		ImGui::Begin("Project Selector", nullptr, windowFlags);

		// Sidebar
		ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorColors::DarkPanelBg);
		ImGui::BeginChild("Sidebar", ImVec2(kSidebarWidth, 0), false);

		// Banner / Logo Area
		ImGui::Dummy(ImVec2(0, 15));
		ImGui::SetCursorPosX(20.0f);
		ImGui::SetWindowFontScale(1.5f);
		ImGui::TextColored(ImVec4(0.2f, 0.7f, 1.0f, 1.0f), ICON_FA_LINK " Chained");
		ImGui::SameLine();
		ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 1.0f), "Engine");
		ImGui::SetWindowFontScale(1.0f);
		ImGui::Separator();

		ImGui::Dummy(ImVec2(0, 15));
		ImGui::TextDisabled("   RECENT PROJECTS");
		ImGui::Dummy(ImVec2(0, 10));

		const auto& config = EditorLayer::Get().GetConfig();
		if (config.RecentProjects.empty())
		{
			ImGui::TextDisabled("   No recent projects.");
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button, EditorColors::SidebarBg);
			ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.05f, 0.5f));
			ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 5.0f);

			for (const auto& projectPath : config.RecentProjects)
			{
				std::string fileName = std::filesystem::path(projectPath).filename().string();
				std::string dirName = std::filesystem::path(projectPath).parent_path().filename().string();

				std::string label = ICON_FA_FOLDER_OPEN "  " + fileName + "\n      " + dirName;

				ImGui::SetCursorPosX(10.0f);
				if (ImGui::Button(label.c_str(), ImVec2(kSidebarWidth - 20, 50)))
				{
					m_ProjectManager.OpenProject(projectPath);
					break;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("%s", projectPath.c_str());
				}
				ImGui::Dummy(ImVec2(0, 5));
			}

			ImGui::PopStyleVar(2);
			ImGui::PopStyleColor();
		}

		ImGui::EndChild();
		ImGui::PopStyleColor(); // ChildBg Sidebar

		ImGui::SameLine();

		// Main Area
		ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorColors::SubCardBg);
		ImGui::BeginChild("MainArea");

		float centerX = ImGui::GetContentRegionAvail().x * 0.5f;
		float centerY = ImGui::GetContentRegionAvail().y * 0.5f;

		ImGui::SetCursorPos(ImVec2(centerX - kCardWidth / 2.0f - kCardGap / 2.0f, centerY - kCardHeight / 2.0f));

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(20, 20));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, EditorColors::ProjectCardBg);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorColors::ProjectCardHover);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorColors::ProjectCardActive);
		ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::BrightText);

		ImGui::BeginGroup();
		{
			ImTextureID newProjTex = 0;
			if (m_NewProjectIcon)
			{
				auto* am = ServiceLocator::TryGet<AssetManager>();
				if (am)
				{
					auto texAsset = am->Get<TextureAsset>("engine/resources/icons/newproject.jpg");
					if (texAsset && texAsset->IsReady())
					{
						auto gpuTex = texAsset->GetTexture();
						if (gpuTex)
						{
							newProjTex = (ImTextureID)(uintptr_t)gpuTex->GetNativeHandle();
						}
					}
				}
			}

			if (ImGui::ImageButton("##NewProject", newProjTex, {kCardWidth, kCardHeight}, {0, 0}, {1, 1}))
			{
				m_ShowCreateDialog = true;
			}
		}
		ImGui::SetWindowFontScale(1.3f);
		ImGui::Text("New Project");
		ImGui::SetWindowFontScale(1.0f);
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kCardTextWrap);
		ImGui::TextDisabled("Start a fresh journey with a dedicated");
		ImGui::TextDisabled("project folder and optimized settings.");
		ImGui::PopTextWrapPos();
		ImGui::EndGroup();

		ImGui::SameLine(0, kCardGap);

		ImGui::BeginGroup();
		{
			ImTextureID openProjTex = 0;
			if (m_OpenProjectIcon)
			{
				auto* am = ServiceLocator::TryGet<AssetManager>();
				if (am)
				{
					auto texAsset = am->Get<TextureAsset>("engine/resources/icons/folder.png");
					if (texAsset && texAsset->IsReady())
					{
						auto gpuTex = texAsset->GetTexture();
						if (gpuTex)
						{
							openProjTex = (ImTextureID)(uintptr_t)gpuTex->GetNativeHandle();
						}
					}
				}
			}

			if (ImGui::ImageButton("##OpenProject", openProjTex, {kCardWidth, kCardHeight}, {0, 0}, {1, 1}))
			{
				m_ProjectManager.OpenProject();
			}
		}
		ImGui::SetWindowFontScale(1.3f);
		ImGui::Text("Open Project");
		ImGui::SetWindowFontScale(1.0f);
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kCardTextWrap);
		ImGui::TextDisabled("Browse and load an existing Chained");
		ImGui::TextDisabled("Engine project (.chproject) file.");
		ImGui::PopTextWrapPos();
		ImGui::EndGroup();

		ImGui::PopStyleColor(4);
		ImGui::PopStyleVar(2);

		ImGui::EndChild();
		ImGui::PopStyleColor(); // ChildBg MainArea

		if (m_ShowCreateDialog)
		{
			ImGui::OpenPopup("Create New Project");

			ImVec2 mainAreaCenter =
				ImVec2(kSidebarWidth + (viewport->WorkSize.x - kSidebarWidth) * 0.5f, viewport->WorkSize.y * 0.5f);
			ImGui::SetNextWindowPos(mainAreaCenter, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			ImGui::SetNextWindowSize(ImVec2(kPopupWidth, 0), ImGuiCond_Appearing);

			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24, 20));
			if (ImGui::BeginPopupModal("Create New Project", &m_ShowCreateDialog,
									   ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
			{
				// --- Header ---
				ImGui::SetWindowFontScale(1.2f);
				ImGui::TextColored(ImVec4(0.2f, 0.7f, 1.0f, 1.0f), ICON_FA_FILE "  New Project");
				ImGui::SetWindowFontScale(1.0f);
				ImGui::Dummy(ImVec2(0, 4));
				ImGui::Separator();
				ImGui::Dummy(ImVec2(0, 8));

				// --- Project Name ---
				ImGui::TextDisabled("PROJECT NAME");
				ImGui::SetNextItemWidth(kInputWidth);
				ImGui::InputText("##ProjectName", m_ProjectNameBuffer, kNameBufSize);

				bool nameEmpty = (m_ProjectNameBuffer[0] == '\0');
				if (nameEmpty)
				{
					ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f),
									   ICON_FA_CIRCLE_EXCLAMATION "  Name cannot be empty");
				}
				else
				{
					ImGui::Dummy(ImVec2(0, ImGui::GetTextLineHeight()));
				}

				ImGui::Dummy(ImVec2(0, 6));

				// --- Location ---
				ImGui::TextDisabled("LOCATION");
				ImGui::SetNextItemWidth(360);
				ImGui::InputText("##ProjectLocation", m_ProjectLocationBuffer, kLocationBufSize);
				ImGui::SameLine(0, 8);
				ImGui::PushStyleColor(ImGuiCol_Button, EditorColors::ProjectCardBorder);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorColors::ProjectCardBorderHover);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorColors::ProjectCardBorderActive);
				if (ImGui::Button(ICON_FA_FOLDER_OPEN "  Browse", ImVec2(64, 0)))
				{
					auto picked = Dialogs::PickFolder();
					if (picked)
					{
						SafeCopy(m_ProjectLocationBuffer, kLocationBufSize, picked->string().c_str());
					}
				}
				ImGui::PopStyleColor(3);

				// --- Path preview box ---
				ImGui::Dummy(ImVec2(0, 10));
				std::filesystem::path previewPath =
					std::filesystem::path(m_ProjectLocationBuffer) / m_ProjectNameBuffer;

				ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorColors::SubCardBg);
				ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
				ImGui::BeginChild("##preview", ImVec2(kInputWidth, 36), false);
				ImGui::SetCursorPos(ImVec2(10, 10));
				ImGui::TextColored(EditorColors::MutedText, ICON_FA_CIRCLE_INFO "  ");
				ImGui::SameLine(0, 0);
				ImGui::TextDisabled("%s", previewPath.string().c_str());
				ImGui::EndChild();
				ImGui::PopStyleVar();
				ImGui::PopStyleColor();

				// --- Buttons ---
				ImGui::Dummy(ImVec2(0, 12));
				ImGui::Separator();
				ImGui::Dummy(ImVec2(0, 8));

				ImGui::SetCursorPosX(kInputWidth + 24 - kBtnWidth * 2 - 8);

				ImGui::PushStyleColor(ImGuiCol_Button, EditorColors::SubCardBorder);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorColors::SubCardBorderHover);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorColors::SubCardBorderActive);
				if (ImGui::Button("Cancel", ImVec2(kBtnWidth, kBtnHeight)))
				{
					m_ShowCreateDialog = false;
					ImGui::CloseCurrentPopup();
				}
				ImGui::PopStyleColor(3);

				ImGui::SameLine(0, 8);

				ImGui::BeginDisabled(nameEmpty);
				ImGui::PushStyleColor(ImGuiCol_Button, EditorColors::PrimaryButton);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorColors::PrimaryButtonHover);
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorColors::PrimaryButtonActive);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
				if (ImGui::Button(ICON_FA_FOLDER "  Create", ImVec2(kBtnWidth, kBtnHeight)))
				{
					m_ProjectManager.NewProject(m_ProjectNameBuffer, previewPath.string());
					m_ShowCreateDialog = false;
					ImGui::CloseCurrentPopup();
				}
				ImGui::PopStyleColor(4);
				ImGui::EndDisabled();

				ImGui::EndPopup();
			}
			ImGui::PopStyleVar();
		}

		ImGui::End();
		ImGui::PopStyleVar(3);
	}

} // namespace Chained