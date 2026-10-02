#ifndef CH_INSPECTOR_PANEL_H
#define CH_INSPECTOR_PANEL_H

#include "panel.h"

namespace Chained
{
	class EditorSceneManager;

	class InspectorPanel : public Panel
	{
	public:
		InspectorPanel(EditorSceneManager* sceneManager = nullptr);
		virtual void OnImGuiRender(bool readOnly = false) override;
		virtual void OnEvent(Event& e) override;
		virtual void SetContext(const std::shared_ptr<Scene>& context) override;

	public:
		void SetSelectedMeshIndex(int index);

	private:
		void DrawComponents(Entity entity, bool readOnly);

	private:
		EditorSceneManager* m_SceneManager = nullptr;
		Entity m_SelectedEntity;
		int m_SelectedMeshIndex = -1;
	};
} // namespace Chained

#endif // CH_INSPECTOR_PANEL_H
