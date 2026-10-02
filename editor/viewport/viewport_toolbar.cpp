#include "viewport_toolbar.h"
#include "editor/editor_colors.h"
#include "editor/layer.h"
#include "engine/core/input.h"
#include "engine/core/key_codes.h"
#include "engine/project/project.h"
#include "engine/scripting/scriptengine.h"
#include "engine/core/service_locator.h"
#include "engine/scene/scene.h"
#include "engine/scene/components.h"
#include "engine/graphics/pipeline/scene_renderer.h"
#include "undo/entity_commands.h"
#include "thirdparty/IconsFontAwesome6.h"
#include "imgui_internal.h"

namespace Chained
{

	static const GizmoBtn s_GizmoBtns[] = {
		{GizmoType::NONE, ICON_FA_ARROW_POINTER "##Select", "Select (Q)", Chained::KeyCode::Q},
		{GizmoType::TRANSLATE, ICON_FA_UP_DOWN_LEFT_RIGHT "##Translate", "Translate (W)", Chained::KeyCode::W},
		{GizmoType::ROTATE, ICON_FA_ARROWS_ROTATE "##Rotate", "Rotate (E)", Chained::KeyCode::E},
		{GizmoType::SCALE, ICON_FA_UP_RIGHT_FROM_SQUARE "##Scale", "Scale (R)", Chained::KeyCode::R}};

	void ViewportToolbar::Render(Scene* scene, const ImVec2& viewportScreenPos)
	{
		SceneState sceneState = EditorLayer::Get().GetSceneManager().GetSceneState();
		if (sceneState == SceneState::Play || sceneState == SceneState::Simulate)
		{
			return;
		}

		ImVec2 toolbarPos = {viewportScreenPos.x + 10.0f, viewportScreenPos.y + 10.0f};
		ImGui::SetNextWindowPos(toolbarPos);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorColors::ToolbarBg);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5, 5));

		if (ImGui::BeginChild("##FloatingToolbar", ImVec2(750, 40), true,
							  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			ImGui::SetCursorPosY(6);
			ImGui::Indent(5);

			DrawGizmoButtons();

			ImGui::SameLine(0, 10);
			bool is2D = m_CameraController.Is2DMode();
			bool isUIScene = scene && scene->GetSettings().Type == SceneType::UI;
			if (is2D)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::ActiveToolOrange);
			}
			if (!isUIScene)
			{
				ImGui::BeginDisabled();
			}
			if (ImGui::Button(is2D ? (ICON_FA_CAMERA " 2D") : (ICON_FA_CUBE " 3D"), {50, 28}))
			{
				m_CameraController.Set2DMode(!is2D);
				m_Gizmo.Set2DMode(!is2D);
			}
			if (!isUIScene)
			{
				ImGui::EndDisabled();
			}
			if (is2D)
			{
				ImGui::PopStyleColor();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip(isUIScene ? "Toggle 2D/3D Editor Mode" : "2D mode available for UI scenes only");
			}

			ImGui::SameLine(0, 10);
			DrawCameraSelector(scene);

			ImGui::SameLine(0, 10);
			ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
			ImGui::SameLine(0, 10);

			DrawSnapSection();
			DrawTransformSpaceToggle();

			ImGui::SameLine(0, 15);
			ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
			ImGui::SameLine(0, 15);

			DrawScriptReloadButton();
		}
		ImGui::EndChild();
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor();
	}

	void ViewportToolbar::DrawGizmoButtons()
	{
		ImGui::PushStyleColor(ImGuiCol_Button, EditorColors::TransparentButton);

		for (const auto& btn : s_GizmoBtns)
		{
			bool selected = (m_Gizmo.GetCurrentTool() == btn.type);
			if (selected)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, EditorColors::ActiveToolOrange);
			}

			if (ImGui::Button(btn.icon, {28, 28}))
			{
				m_Gizmo.SetCurrentTool(btn.type);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", btn.tooltip);
			}

			if (selected)
			{
				ImGui::PopStyleColor();
			}
			ImGui::SameLine(0, 5);
		}

		ImGui::PopStyleColor();
	}

	void ViewportToolbar::DrawCameraSelector(Scene* scene)
	{
		if (!scene)
		{
			return;
		}

		ImGui::PushStyleColor(ImGuiCol_Button, EditorColors::FloatingToolbarBg);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

		auto view = scene->GetRegistry().view<CameraComponent>();
		Entity primaryCam = SceneRenderer::GetPrimaryCameraEntity(scene->GetRegistry(), scene->GetRegistryPtr());
		std::string currentLabel = primaryCam ? primaryCam.GetComponent<TagComponent>().Tag : "Editor Camera";

		ImGui::SetNextItemWidth(150);
		if (ImGui::BeginCombo("##CameraSelector", (ICON_FA_VIDEO "  " + currentLabel).c_str(), ImGuiComboFlags_None))
		{
			for (auto entityHandle : view)
			{
				Entity entity(entityHandle, &scene->GetRegistry());
				bool isSelected = (entity == primaryCam);
				std::string tag = entity.GetComponent<TagComponent>().Tag;

				if (ImGui::Selectable(tag.c_str(), isSelected))
				{
					for (auto otherHandle : view)
					{
						scene->GetRegistry().get<CameraComponent>(otherHandle).Primary = false;
					}
					entity.GetComponent<CameraComponent>().Primary = true;
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
			ImGui::SetTooltip("Select the active game camera");
		}

		ImGui::PopStyleVar();
		ImGui::PopStyleColor();
	}

	void ViewportToolbar::DrawSnapSection()
	{
		bool snapping = m_Gizmo.IsSnappingEnabled();
		if (snapping)
		{
			ImGui::PushStyleColor(ImGuiCol_Text, EditorColors::ActiveSnapBlue);
		}
		if (ImGui::Button(ICON_FA_MAGNET "##SnapToggle", {28, 28}))
		{
			m_Gizmo.SetSnapping(!snapping);
		}
		if (snapping)
		{
			ImGui::PopStyleColor();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Enable Grid Snapping");
		}

		ImGui::SameLine(0, 5);
		auto scene = EditorLayer::Get().GetActiveScene();
		float gridSize = scene->GetSettings().Grid.Spacing;
		ImGui::SetNextItemWidth(60);
		if (ImGui::DragFloat("##SnapValue", &gridSize, 0.1f, 0.1f, 50.0f, "%.1f"))
		{
			scene->GetSettings().Grid.Spacing = gridSize;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Grid Snap Size (synced with visual grid)");
		}
	}

	void ViewportToolbar::DrawTransformSpaceToggle()
	{
		ImGui::SameLine(0, 10);

		bool isLocal = m_Gizmo.IsLocalSpace();
		if (ImGui::Button(isLocal ? (ICON_FA_CUBE " Local") : (ICON_FA_EARTH_AMERICAS " World"), {70, 28}))
		{
			m_Gizmo.SetLocalSpace(!isLocal);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Toggle Local/World Space");
		}
	}

	void ViewportToolbar::DrawScriptReloadButton()
	{
		ImGui::SameLine(0, 5);
		if (ImGui::Button(ICON_FA_FILE_CODE "##ReloadToolbar", ImVec2(28, 28)))
		{
			auto project = Project::GetActive();
			if (project)
			{
				auto assemblyPath = ScriptEngine::ResolveAssemblyPath(project->GetConfig().Scripting,
																	  project->GetConfig().ProjectDirectory);
				if (auto* scriptEngine = ServiceLocator::TryGet<ScriptEngine>())
				{
					scriptEngine->RequestAssemblyReload(assemblyPath.string(), "ViewportPanel");
				}
			}
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Reload Scripts (Ctrl+R)");
		}
	}

	void ViewportToolbar::HandleKeyboardShortcuts()
	{
		bool hasImGui = ImGui::GetCurrentContext() != nullptr;
		bool rightDown = hasImGui ? ImGui::IsMouseDown(ImGuiMouseButton_Right)
								  : Chained::Core::Input::IsMouseButtonDown(Chained::MouseCode::ButtonRight);

		if (!rightDown)
		{
			if (hasImGui)
			{
				if (ImGui::IsKeyPressed(ImGuiKey_Q, false))
				{
					m_Gizmo.SetCurrentTool(GizmoType::NONE);
				}
				else if (ImGui::IsKeyPressed(ImGuiKey_W, false))
				{
					m_Gizmo.SetCurrentTool(GizmoType::TRANSLATE);
				}
				else if (ImGui::IsKeyPressed(ImGuiKey_E, false))
				{
					m_Gizmo.SetCurrentTool(GizmoType::ROTATE);
				}
				else if (ImGui::IsKeyPressed(ImGuiKey_R, false))
				{
					m_Gizmo.SetCurrentTool(GizmoType::SCALE);
				}
			}
			else
			{
				for (const auto& btn : s_GizmoBtns)
				{
					if (Chained::Core::Input::IsKeyPressed(btn.key))
					{
						m_Gizmo.SetCurrentTool(btn.type);
					}
				}
			}
		}

		bool isCtrl = hasImGui ? (ImGui::IsKeyDown(ImGuiKey_LeftCtrl) || ImGui::IsKeyDown(ImGuiKey_RightCtrl))
							   : (Chained::Core::Input::IsKeyDown(Chained::KeyCode::LeftControl) ||
								  Chained::Core::Input::IsKeyDown(Chained::KeyCode::RightControl));
		bool isDPressed =
			hasImGui ? ImGui::IsKeyPressed(ImGuiKey_D, false) : Chained::Core::Input::IsKeyPressed(Chained::KeyCode::D);

		if (isCtrl && isDPressed)
		{
			Entity selected = EditorLayer::Get().GetEditorState().SelectedEntity;
			if (selected)
			{
				EditorLayer::Get().GetCommandHistory().PushCommand(std::make_unique<DuplicateEntityCommand>(selected));
			}
		}
	}

} // namespace Chained
