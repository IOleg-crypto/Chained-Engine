#ifndef CH_EDITOR_LAYOUT_H
#define CH_EDITOR_LAYOUT_H

#include "editor/panels.h"
#include <string>
#include <vector>

namespace Chained
{

	class EditorLayout
	{
	public:
		EditorLayout(EditorPanels& panels);

		void ResetLayout();

		void OnImGuiRender();

	private:
		EditorPanels& m_Panels;
		uint32_t m_DockSpaceID = 0;
		bool m_NeedsRebuild = true;
	};

} // namespace Chained

#endif // CH_EDITOR_LAYOUT_H
