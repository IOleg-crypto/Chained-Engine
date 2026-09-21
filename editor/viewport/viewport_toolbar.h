#ifndef CH_VIEWPORT_TOOLBAR_H
#define CH_VIEWPORT_TOOLBAR_H

#include "viewport/camera.h"
#include "viewport/gizmo.h"
#include "engine/core/key_codes.h"
#include <imgui.h>

namespace Chained
{
	class Scene;
	class EditorSceneManager;
	struct EditorState;
	class CommandHistory;

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
		ViewportToolbar(EditorGizmo& gizmo, EditorCameraController& camera, EditorSceneManager* sceneManager = nullptr,
						EditorState* editorState = nullptr, CommandHistory* commandHistory = nullptr)
			: m_Gizmo(gizmo),
			  m_CameraController(camera),
			  m_SceneManager(sceneManager),
			  m_EditorState(editorState),
			  m_CommandHistory(commandHistory)
		{
		}

		// Renders the full toolbar. Hidden during Play/Simulate mode.
		void Render(Scene* scene, const ImVec2& viewportScreenPos);

		// Keyboard shortcuts for gizmo switching (Q/W/E/R) and duplicate (Ctrl+D).
		void HandleKeyboardShortcuts();

	private:
		void DrawGizmoButtons();
		void DrawCameraSelector(Scene* scene);
		void DrawSnapSection(Scene* scene);
		void DrawTransformSpaceToggle();
		void DrawScriptReloadButton();

		EditorGizmo& m_Gizmo;
		EditorCameraController& m_CameraController;
		EditorSceneManager* m_SceneManager = nullptr;
		EditorState* m_EditorState = nullptr;
		CommandHistory* m_CommandHistory = nullptr;
	};

} // namespace Chained

#endif // CH_VIEWPORT_TOOLBAR_H
