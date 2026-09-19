#ifndef CH_WORLD_PANEL_H
#define CH_WORLD_PANEL_H

#include "panel.h"

namespace Chained
{
	class EditorSceneManager;

	class WorldPanel : public Panel
	{
	public:
		explicit WorldPanel(EditorSceneManager* sceneManager = nullptr);

	public:
		virtual void OnImGuiRender(bool readOnly = false) override;

	private:
		void DrawSceneGeneral(bool readOnly);
		void DrawSceneBackground(bool readOnly);
		void DrawPhysicsSettings(bool readOnly);
		void DrawEnvironmentSection(bool readOnly);
		void DrawEnvironmentSettings(std::shared_ptr<EnvironmentAsset> env, bool readOnly);

	private:
		EditorSceneManager* m_SceneManager = nullptr;
	};
} // namespace Chained

#endif // CH_WORLD_PANEL_H
