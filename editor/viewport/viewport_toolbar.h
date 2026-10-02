#ifndef CH_VIEWPORT_TOOLBAR_H
#define CH_VIEWPORT_TOOLBAR_H

#include "viewport/camera.h"
#include "viewport/gizmo.h"
#include "engine/core/key_codes.h"
#include <imgui.h>

namespace Chained
{
	class Scene;

	struct GizmoBtn
	{
		GizmoType type;
		const char* icon;
		const char* tooltip;
		KeyCode key;
	};

	// Renders the floating toolbar in the viewport: gizmo tool buttons, 2D/3D
	// toggle, camera selector, snap controls, transform space toggle, and
	// script reload button.
	class ViewportToolbar
	{
	public:
		ViewportToolbar(EditorGizmo& gizmo, EditorCameraController& camera)
			: m_Gizmo(gizmo),
			  m_CameraController(camera)
		{
		}

		// Renders the full toolbar. Hidden during Play/Simulate mode.
		void Render(Scene* scene, const ImVec2& viewportScreenPos);

		// Keyboard shortcuts for gizmo switching (Q/W/E/R) and duplicate (Ctrl+D).
		void HandleKeyboardShortcuts();

	private:
		void DrawGizmoButtons();
		void DrawCameraSelector(Scene* scene);
		void DrawSnapSection();
		void DrawTransformSpaceToggle();
		void DrawScriptReloadButton();

		EditorGizmo& m_Gizmo;
		EditorCameraController& m_CameraController;
	};

} // namespace Chained

#endif // CH_VIEWPORT_TOOLBAR_H
