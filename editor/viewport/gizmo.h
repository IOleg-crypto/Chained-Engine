#ifndef CH_EDITOR_GIZMO_H
#define CH_EDITOR_GIZMO_H

#include "engine/graphics/camera_types.h"
#include "engine/scene/components.h"
#include "engine/scene/scene.h"
#include <imgui.h>
#include <ImGuizmo.h>

namespace Chained
{

	enum class GizmoType
	{
		NONE = -1,
		TRANSLATE = ImGuizmo::OPERATION::TRANSLATE,
		ROTATE = ImGuizmo::OPERATION::ROTATE,
		SCALE = ImGuizmo::OPERATION::SCALE,
		BOUNDS = ImGuizmo::OPERATION::BOUNDS
	};

	class EditorGizmo
	{
	public:
		EditorGizmo() = default;
		~EditorGizmo() = default;

		// Render and handle gizmo interaction
		// true if the gizmo is being used (captured mouse)
		bool RenderAndHandle(GizmoType type, ImVec2 viewportPos, ImVec2 viewportSize, const Camera3D& camera);

		void Set2DMode(bool enabled)
		{
			m_Is2DMode = enabled;
		}
		bool Is2DMode() const
		{
			return m_Is2DMode;
		}

		bool IsHovered() const
		{
			return ImGuizmo::IsOver();
		}
		bool IsDragging() const
		{
			return ImGuizmo::IsUsing();
		}

		// Snapping
		void SetSnapping(bool enabled)
		{
			m_SnappingEnabled = enabled;
		}
		bool IsSnappingEnabled() const
		{
			return m_SnappingEnabled;
		}

		void SetRotationStep(float step)
		{
			m_RotationSnap = step;
		}
		void SetScaleStep(float step)
		{
			m_ScaleSnap = step;
		}

		float GetRotationStep() const
		{
			return m_RotationSnap;
		}
		float GetScaleStep() const
		{
			return m_ScaleSnap;
		}

		void SetLocalSpace(bool local)
		{
			m_IsLocalSpace = local;
		}
		bool IsLocalSpace() const
		{
			return m_IsLocalSpace;
		}

		void SetCurrentTool(GizmoType tool)
		{
			m_CurrentTool = tool;
		}
		GizmoType GetCurrentTool() const
		{
			return m_CurrentTool;
		}

	private:
		GizmoType m_CurrentTool = GizmoType::TRANSLATE;
		bool m_SnappingEnabled = false;

		float m_RotationSnap = 45.0f;
		float m_ScaleSnap = 0.1f;

		bool m_IsLocalSpace = false;
		bool m_Is2DMode = false;

		// Undo state
		TransformComponent m_OldTransform;
		bool m_WasUsing = false;
	};

} // namespace Chained

#endif // CH_EDITOR_GIZMO_H