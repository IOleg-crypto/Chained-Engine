#ifndef CH_PROJECT_SELECTOR_UI_H
#define CH_PROJECT_SELECTOR_UI_H

#include "editor/project_manager.h"
#include "engine/assets/types/texture_asset.h"
#include <cstddef>
#include <cstdint>
#include <memory>

namespace Chained
{
	struct EditorConfig;

	class ProjectSelectorUI
	{
	public:
		ProjectSelectorUI(EditorProjectManager& projectManager, const EditorConfig& config);

		void OnImGuiRender();

	private:
		EditorProjectManager& m_ProjectManager;
		const EditorConfig& m_Config;

		std::shared_ptr<TextureAsset> m_NewProjectIcon = nullptr;
		std::shared_ptr<TextureAsset> m_OpenProjectIcon = nullptr;
		bool m_IconsLoaded = false;

		bool m_ShowCreateDialog = false;
		char m_ProjectNameBuffer[128] = "NewProject";
		char m_ProjectLocationBuffer[256] = "";
		bool m_Initialized = false;

		void LoadEditorIcons();
	};

} // namespace Chained

#endif // CH_PROJECT_SELECTOR_UI_H