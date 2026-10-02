#include "panels.h"
#include "layer.h"
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

	void EditorPanels::Init()
	{
		Register<ViewportPanel>(EditorLayer::Get().GetViewportSizeRef());
		Register<SceneHierarchyPanel>();
		Register<InspectorPanel>();
		Register<ContentBrowserPanel>();
		Register<ConsolePanel>();
		Register<WorldPanel>();
		Register<EffectsPanel>();
		Register<MaterialPanel>();
		Register<ProfilerPanel>();
		Register<ProjectSettingsPanel>();
		Register<AnimGraphPanel>();
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
		for (auto& panel : m_Panels)
		{
			panel->SetContext(context);
		}
	}

} // namespace Chained
