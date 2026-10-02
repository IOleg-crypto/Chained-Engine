#include "panels.h"
#include "panels/console_panel.h"
#include "panels/content_browser_panel.h"
#include "panels/world_panel.h"
#include "panels/effects_panel.h"
#include "panels/material_panel.h"
#include "panels/inspector_panel.h"
#include "panels/panel.h"
#include "panels/profiler_panel.h"
#include "panels/project_settings_panel.h"
#include "panels/scene_hierarchy_panel.h"
#include "panels/viewport_panel.h"
#include "panels/anim_graph_panel.h"
#include "panels/network_panel.h"

namespace Chained
{

	void EditorPanels::Init(EditorSceneManager* sceneManager, EditorState* editorState, const EditorConfig* config,
							ImVec2* viewportSize, CommandHistory* commandHistory)
	{
		static ImVec2 s_FallbackViewportSize(1280, 720);
		Register<ViewportPanel>(viewportSize ? *viewportSize : s_FallbackViewportSize, sceneManager, editorState,
								config, commandHistory);

		Register<SceneHierarchyPanel>(editorState, commandHistory, sceneManager);
		Register<InspectorPanel>(sceneManager);
		Register<ContentBrowserPanel>(sceneManager, config);
		Register<ConsolePanel>();
		Register<WorldPanel>(sceneManager);
		Register<EffectsPanel>();
		Register<MaterialPanel>(sceneManager);
		Register<ProfilerPanel>();
		Register<ProjectSettingsPanel>();
		Register<AnimGraphPanel>(editorState, sceneManager);
		Register<NetworkPanel>();
	}

	void EditorPanels::OnUpdate(Timestep ts)
	{
		for (auto& panel : m_Panels)
		{
			if (!panel->IsPendingKill())
			{
				panel->OnUpdate(ts);
			}
		}

		m_Panels.erase(std::remove_if(m_Panels.begin(), m_Panels.end(),
									  [](const std::shared_ptr<Panel>& panel) { return panel->IsPendingKill(); }),
					   m_Panels.end());
	}

	void EditorPanels::OnImGuiRender(bool readOnly)
	{
		for (auto& panel : m_Panels)
		{
			if (!panel->IsPendingKill())
			{
				panel->OnImGuiRender(readOnly);
			}
		}
	}

	void EditorPanels::OnEvent(Event& e)
	{
		for (auto& panel : m_Panels)
		{
			if (e.Handled)
			{
				break;
			}
			if (!panel->IsPendingKill())
			{
				panel->OnEvent(e);
			}
		}
	}

	void EditorPanels::SetContext(const std::shared_ptr<Scene>& context)
	{
		if (!context || m_Context == context)
		{
			return;
		}

		m_Context = context;
		for (auto& panel : m_Panels)
		{
			panel->SetContext(context);
		}
	}

} // namespace Chained
