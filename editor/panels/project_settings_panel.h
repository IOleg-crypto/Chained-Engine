#include <vector>
#include <string>
#ifndef CH_PROJECT_SETTINGS_PANEL_H
#define CH_PROJECT_SETTINGS_PANEL_H

#include "panel.h"

namespace Chained
{
	class ProjectSettingsPanel : public Panel
	{
	public:
		ProjectSettingsPanel();

	public:
		virtual void OnImGuiRender(bool readOnly = false) override;

	private:
		int m_SelectedCategory = 0;
		std::vector<std::string> m_AllScenes;
		bool m_ScenesScanned = false;

		bool m_WidthSet = false;
	};
} // namespace Chained

#endif // CH_PROJECT_SETTINGS_PANEL_H
