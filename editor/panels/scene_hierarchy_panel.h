#ifndef CH_SCENE_HIERARCHY_PANEL_H
#define CH_SCENE_HIERARCHY_PANEL_H

#include "panel.h"
#include "unordered_set"

namespace Chained
{

	class CommandHistory;
	struct EditorState;

	class SceneHierarchyPanel : public Panel
	{
	public:
		SceneHierarchyPanel();

		virtual void OnImGuiRender(bool readOnly = false) override;

	private:
		void DrawEntityNodeRecursive(Entity entity, bool readOnly);
		void DrawContextMenu();
		const char* GetEntityIcon(Entity entity);
		void StartRename(Entity entity);

	private:
		std::unordered_set<entt::entity> m_DrawnEntities;
		std::vector<entt::entity> m_EntitiesToDestroyPending;

		char m_SearchBuffer[128] = {0};
		bool m_Renaming = false;
		char m_RenameBuffer[128] = {0};
		Entity m_RenamingEntity;
	};

} // namespace Chained

#endif // CH_SCENE_HIERARCHY_PANEL_H
