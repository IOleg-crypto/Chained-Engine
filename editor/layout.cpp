#include "layer.h"
#include "editor_menu.h"
#include "layout.h"

#include "gui.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "engine/core/log.h"
#include <filesystem>

namespace Chained
{

	constexpr float kLeftDockRatio = 0.20f;

	EditorLayout::EditorLayout(EditorPanels& panels)
		: m_Panels(panels)
	{
		// Rebuild the default DockBuilder arrangement only when no saved layout exists.
		// If imgui.ini is present, honor it so the user's arrangement persists across launches.
		const char* iniPath = ImGui::GetIO().IniFilename;
		m_NeedsRebuild = !(iniPath != nullptr && std::filesystem::exists(iniPath));
	}

	void EditorLayout::ResetLayout()
	{
		// Delete saved layout and rebuild the default DockBuilder arrangement
		const char* iniPath = ImGui::GetIO().IniFilename;
		if (iniPath != nullptr && std::filesystem::exists(iniPath))
		{
			std::filesystem::remove(iniPath);
		}
		m_NeedsRebuild = true;
	}

	void EditorLayout::OnImGuiRender()
	{
		static bool dockspaceOpen = true;
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;

		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
						ImGuiWindowFlags_NoMove;
		window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
		{
			window_flags |= ImGuiWindowFlags_NoBackground;
		}

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::Begin("MainDockSpaceWindow", &dockspaceOpen, window_flags);
		ImGui::PopStyleVar();
		ImGui::PopStyleVar(2);

		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			m_DockSpaceID = ImGui::GetID("MyDockSpace");

			if (m_NeedsRebuild)
			{
				m_NeedsRebuild = false;

				ImGui::DockBuilderRemoveNode(m_DockSpaceID);
				ImGui::DockBuilderAddNode(m_DockSpaceID, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
				ImGui::DockBuilderSetNodeSize(m_DockSpaceID, viewport->WorkSize);

				ImGuiID dock_main_id = m_DockSpaceID;

				// Layout:
				// dock_left: Scene Hierarchy, World Settings below it
				// dock_right: Inspector, Material Editor
				// dock_bottom: Content Browser, Console
				// Center: Viewport

				// Build the layout splits
				ImGuiID dock_left =
					ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, kLeftDockRatio, nullptr, &dock_main_id);
				ImGuiID dock_right =
					ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.25f, nullptr, &dock_main_id);
				ImGuiID dock_bottom =
					ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.25f, nullptr, &dock_main_id);
				ImGuiID dock_left_bottom =
					ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.5f, nullptr, &dock_left);

				// Assign windows to locations
				ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_left);
				ImGui::DockBuilderDockWindow("World Settings", dock_left_bottom);

				ImGui::DockBuilderDockWindow("Inspector", dock_right);
				ImGui::DockBuilderDockWindow("Material Editor", dock_right); // grouped with Inspector
				ImGui::DockBuilderDockWindow("Network", dock_right);		 // grouped with Inspector

				ImGui::DockBuilderDockWindow("Content Browser", dock_bottom);
				ImGui::DockBuilderDockWindow("Console", dock_bottom);		  // grouped with Content Browser
				ImGui::DockBuilderDockWindow("Animation Graph", dock_bottom); // grouped with Content Browser
				ImGui::DockBuilderDockWindow("Effects & Debug", dock_bottom); // grouped with Content Browser
				ImGui::DockBuilderDockWindow("Profiler", dock_bottom);		  // grouped with Content Browser

				ImGui::DockBuilderDockWindow("Project Settings", dock_left_bottom); // grouped with World Settings

				ImGui::DockBuilderDockWindow("Viewport", dock_main_id); // remainder

				ImGui::DockBuilderFinish(m_DockSpaceID);
			}

			ImGui::DockSpace(m_DockSpaceID, ImVec2(0.0f, 0.0f), dockspace_flags);
		}

		auto& menu = EditorLayer::Get().GetMenu();
		menu.DrawMenuBar(m_Panels);

		bool readOnly = EditorLayer::Get().GetSceneState() == SceneState::Play;
		m_Panels.OnImGuiRender(readOnly);

		menu.DrawEditorSettings();
		menu.DrawExportDialog();
		menu.DrawExportProgressOverlay();

		ImGui::End();
	}

} // namespace Chained
