#ifndef CH_EDITOR_LAYER_H
#define CH_EDITOR_LAYER_H

#include <memory>
#include <string>
#include "imgui.h"

#include "engine/scene/scene.h"
#include "editor/project_manager.h"
#include "editor/types.h"
#include "editor/scene_manager.h"
#include "engine/app/application.h"
#include "engine/core/layer.h"
#include "editor/layout.h"
#include "editor/panels.h"
#include "editor/undo/command_history.h"

namespace Chained
{

	class ProjectSelectorUI;
	class EditorMenu;
	class KeyPressedEvent;
	class FontManager;
	class Framebuffer;
	class SceneRenderer;

	class EditorLayer : public Layer
	{
	public:
		static EditorLayer& Get()
		{
			return *s_Instance;
		}

		EditorLayer();
		virtual ~EditorLayer();

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(Timestep ts) override;
		virtual void OnRender(Timestep ts) override;
		virtual void OnImGuiRender() override;
		virtual void OnEvent(Event& e) override;

		void ResetLayout();

		EditorSceneManager& GetSceneManager()
		{
			return *m_SceneManager;
		}
		EditorProjectManager& GetProjectManager()
		{
			return *m_ProjectManager;
		}

		Entity GetSelectedEntity() const
		{
			return m_EditorState.SelectedEntity;
		}
		void SetSelectedEntity(Entity entity)
		{
			m_EditorState.SelectedEntity = entity;
		}

		DebugRenderFlags& GetDebugRenderFlags()
		{
			return m_EditorState.DebugRenderFlags;
		}
		EditorState& GetEditorState()
		{
			return m_EditorState;
		}

		SceneState GetSceneState() const
		{
			return m_SceneManager->GetSceneState();
		}
		void SetSceneState(SceneState state)
		{
			m_SceneManager->SetSceneState(state);
		}

	private:
		void LoadEditorFonts();
		void DrawLoadingOverlay(const char* title, const char* status);
		bool HandleKeyboardShortcut(KeyPressedEvent& e);

	public:
		CommandHistory& GetCommandHistory();
		EditorPanels& GetPanels()
		{
			return *m_Panels;
		}
		EditorMenu& GetMenu()
		{
			return *m_Menu;
		}

		ImVec2 GetViewportSize() const
		{
			return m_ViewportSize;
		}
		ImVec2& GetViewportSizeRef()
		{
			return m_ViewportSize;
		}
		void OnViewportResized(const ImVec2& size)
		{
			m_ViewportSize = size;
		}
		void SetLastScenePath(const std::string& path)
		{
			m_Config.LastScenePath = path;
		}

		void LoadConfig();
		void SaveConfig();
		const EditorConfig& GetConfig() const
		{
			return m_Config;
		}
		EditorConfig& GetConfig()
		{
			return m_Config;
		}

		void ReloadEditorFonts();
		void RequestEditorFontReload();
		FontManager& GetFontManager()
		{
			return *m_FontManager;
		}

		std::shared_ptr<Scene> GetActiveScene() const;
		EditorLayout* GetLayout() const
		{
			return m_Layout.get();
		}

	private:
		EditorConfig m_Config;
		EditorState m_EditorState;

		std::unique_ptr<EditorLayout> m_Layout;
		std::unique_ptr<EditorPanels> m_Panels;
		std::unique_ptr<EditorProjectManager> m_ProjectManager;
		std::unique_ptr<EditorSceneManager> m_SceneManager;
		std::unique_ptr<ProjectSelectorUI> m_ProjectSelectorUI;
		std::unique_ptr<EditorMenu> m_Menu;
		std::unique_ptr<FontManager> m_FontManager;
		CommandHistory m_CommandHistory;

		std::string m_PendingSceneTransitionPath;
		ImVec2 m_ViewportSize = {1280, 720};

		// Tracks the scene state seen on the previous frame so we can detect the
		// Edit->Play transition. On the first Play frame we suppress UI input so the
		// physical mouse click that pressed the Play toolbar button (still reported
		// as IsMouseClicked this frame) does not leak through to game widgets.
		SceneState m_PrevSceneState = SceneState::Edit;
		bool m_SuppressNextUIInput = false;

		static inline EditorLayer* s_Instance = nullptr;
	};
} // namespace Chained

#endif // CH_EDITOR_LAYER_H