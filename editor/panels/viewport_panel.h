#ifndef CH_VIEWPORT_PANEL_H
#define CH_VIEWPORT_PANEL_H

#include "panel.h"
#include "engine/common/timestep.h"
#include "engine/scene/scene_settings.h"
#include "viewport/camera.h"
#include "viewport/gizmo.h"
#include "viewport/ui_manipulator.h"
#include "viewport/viewport_renderer.h"
#include "viewport/viewport_picking.h"
#include "viewport/viewport_toolbar.h"

#include <memory>
#include <utility>
#include <glm/glm.hpp>

struct GLFWwindow;

namespace Chained
{
	class Scene;
	struct Camera3D;
	class Event;

	class ViewportPanel : public Panel
	{
	public:
		ViewportPanel(ImVec2& editorViewportSize);
		~ViewportPanel() override;

		void OnImGuiRender(bool readOnly = false) override;
		void OnUpdate(Timestep ts) override;
		void OnEvent(Event& e) override;

		bool IsFocused() const
		{
			return m_Focused;
		}
		bool IsHovered() const
		{
			return m_Hovered;
		}
		glm::vec2 GetSize() const
		{
			return m_ViewportSize;
		}
		GizmoType GetCurrentTool()
		{
			return m_Gizmo.GetCurrentTool();
		}

		std::shared_ptr<Framebuffer> GetViewportFramebuffer() const
		{
			return m_Renderer.GetViewportFramebuffer();
		}

	private:
		glm::vec2 m_ViewportSize = {0, 0};
		bool m_Focused = false;
		bool m_Hovered = false;
		bool m_CursorLocked = false;
		GLFWwindow* m_PlatformWindow = nullptr;
		GLFWwindow* m_LockedWindow = nullptr;

		// Sub-modules
		ViewportRenderer m_Renderer;
		ViewportPicking m_Picking;
		std::unique_ptr<EditorCameraController> m_CameraController;
		EditorGizmo m_Gizmo;
		std::unique_ptr<ViewportToolbar> m_Toolbar;
		EditorUIManipulator m_UIManipulator;

		std::pair<SceneType, BackgroundMode> m_LastSceneState = {SceneType::Default, BackgroundMode::Environment3D};

		ImVec2& m_EditorViewportSize;

	private:
		void HandleResize(const ImVec2& viewportSize, Scene* activeScene);
		void HandleDragDrop(Scene* activeScene);
		void RenderOverlays(Scene* activeScene, const ImVec2& viewportSize, const ImVec2& viewportScreenPos);

		Camera3D GetActiveOrEditorCamera(Scene* scene) const;
	};

} // namespace Chained

#endif // CH_VIEWPORT_PANEL_H
