#ifndef CH_WORLD_PANEL_H
#define CH_WORLD_PANEL_H

#include "panel.h"

namespace Chained
{
	class WorldPanel : public Panel
	{
	public:
		WorldPanel();

	public:
		virtual void OnImGuiRender(bool readOnly = false) override;

	private:
		void DrawSceneGeneral(bool readOnly);
		void DrawSceneBackground(bool readOnly);
		void DrawPhysicsSettings(bool readOnly);
		void DrawEnvironmentSection(bool readOnly);
		void DrawEnvironmentSettings(std::shared_ptr<EnvironmentAsset> env, bool readOnly);
	};
} // namespace Chained

#endif // CH_WORLD_PANEL_H
