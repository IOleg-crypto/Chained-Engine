#include "editor/viewport/gizmo.h"
#include "engine/scene/systems/transform_system.h"
#include "engine/scene/components/core/hierarchy_component.h"
#include "gui.h"
#include "imgui_internal.h"
#include "editor/scene_manager.h"
#include "layer.h"
#include "undo/modify_component_command.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace Chained
{

	bool EditorGizmo::RenderAndHandle(GizmoType type, ImVec2 viewportPos, ImVec2 viewportSize,
									  const Chained::Camera3D& camera)
	{
		auto& layer = EditorLayer::Get();
		Scene* scene = layer.GetActiveScene().get();
		Entity entity = layer.GetSelectedEntity();

		if (!scene || !entity || !entity.HasComponent<TransformComponent>() || type == GizmoType::NONE ||
			layer.GetSceneState() == SceneState::Play || layer.GetSceneManager().IsTransitioning())
		{
			return false;
		}

		// Disable gizmo in UI scenes with 2D (orthographic) camera
		if (camera.Projection == ProjectionType::Orthographic && scene->GetSettings().Type == SceneType::UI)
		{
			return false;
		}

		if (viewportSize.x <= 1.0f || viewportSize.y <= 1.0f)
		{
			return false;
		}

		auto& transform = entity.GetComponent<TransformComponent>();
		glm::mat4 modelMat = transform.WorldTransform;

		// Setup ImGuizmo
		ImGuizmo::SetOrthographic(camera.Projection != ProjectionType::Perspective);
		ImGuizmo::SetDrawlist(ImGui::GetForegroundDrawList());
		ImGuizmo::SetAlternativeWindow(ImGui::GetCurrentWindow());

		// Ensure we are using absolute screen coordinates for SetRect
		ImGuizmo::SetRect(viewportPos.x, viewportPos.y, viewportSize.x, viewportSize.y);

		// 2. Use the pre-built View/Projection matrices from Camera3D so that the gizmo
		//    is pixel-perfectly aligned with the renderer's own matrices.
		const glm::mat4& view = camera.ViewMatrix;
		const glm::mat4& projection = camera.ProjectionMatrix;

		// Read snap value from scene grid settings (always in sync with visual grid)
		float currentSnapValues[3] = {0.0f, 0.0f, 0.0f};
		if (m_SnappingEnabled)
		{
			const float snapValue = scene->GetSettings().Grid.Spacing;
			if (type == GizmoType::TRANSLATE)
			{
				currentSnapValues[0] = snapValue;
				currentSnapValues[1] = snapValue;
				currentSnapValues[2] = snapValue;
			}
			else if (type == GizmoType::ROTATE)
			{

				currentSnapValues[0] = m_RotationSnap;
			}
			else if (type == GizmoType::SCALE)
			{
				currentSnapValues[0] = m_ScaleSnap;
				currentSnapValues[1] = m_ScaleSnap;
				currentSnapValues[2] = m_ScaleSnap;
			}
		}

		float* snap = m_SnappingEnabled ? currentSnapValues : nullptr;

		// 4. Manipulation
		ImGuizmo::MODE mode = m_IsLocalSpace ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

		ImGuizmo::OPERATION op = static_cast<ImGuizmo::OPERATION>(type);
		if (m_Is2DMode)
		{
			if (op == ImGuizmo::TRANSLATE)
			{
				op = (ImGuizmo::OPERATION)(ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y);
			}
			else if (op == ImGuizmo::SCALE)
			{
				op = (ImGuizmo::OPERATION)(ImGuizmo::SCALE_X | ImGuizmo::SCALE_Y);
			}
			else if (op == ImGuizmo::ROTATE)
			{
				op = ImGuizmo::ROTATE_Z;
			}
		}

		const bool wasUsing = m_WasUsing;
		const bool manipulated = ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), op, mode,
													  glm::value_ptr(modelMat), nullptr, snap);

		const bool isUsingNow = ImGuizmo::IsUsing();
		if (isUsingNow && !wasUsing)
		{
			m_WasUsing = true;
			m_OldTransform = transform;
		}

		if (manipulated || isUsingNow)
		{
			glm::mat4 localMat = modelMat;
			if (entity.HasComponent<HierarchyComponent>())
			{
				auto& hierarchy = entity.GetComponent<HierarchyComponent>();
				if (hierarchy.Parent != entt::null && scene->GetRegistry().valid(hierarchy.Parent) &&
					scene->GetRegistry().all_of<TransformComponent>(hierarchy.Parent))
				{
					const auto& parentTransform = scene->GetRegistry().get<TransformComponent>(hierarchy.Parent);
					localMat = glm::inverse(parentTransform.WorldTransform) * modelMat;
				}
			}

			glm::vec3 translation, rotation, scale;
			ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(localMat), glm::value_ptr(translation),
												  glm::value_ptr(rotation), glm::value_ptr(scale));

			TransformSystem::SetTranslation(transform, translation);
			TransformSystem::SetRotation(transform, glm::radians(rotation));
			TransformSystem::SetScale(transform, scale);
			transform.WorldTransform = modelMat;
			transform.InverseWorldTransform = glm::inverse(modelMat);
		}
		else if (m_WasUsing && !isUsingNow)
		{
			m_WasUsing = false;

			const bool changed = glm::length(transform.Translation - m_OldTransform.Translation) > 0.0001f ||
								 glm::length(transform.Rotation - m_OldTransform.Rotation) > 0.0001f ||
								 glm::length(transform.Scale - m_OldTransform.Scale) > 0.0001f;

			if (changed)
			{
				EditorLayer::Get().GetCommandHistory().PushCommand(
					std::make_unique<ModifyComponentCommand<TransformComponent>>(entity, m_OldTransform, transform,
																				 "Transform Entity"));
			}
		}

		return ImGuizmo::IsOver() || isUsingNow;
	}

} // namespace Chained