#ifndef CH_SCENE_SETTINGS_H
#define CH_SCENE_SETTINGS_H

#include "engine/assets/types/environment_asset.h"
#include "engine/common/color.h"
#include "engine/ui/ui_style.h"
#include <memory>
#include <string>

namespace Chained
{
	struct DebugRenderFlags
	{
		bool DrawColliders = false;
		bool DrawHierarchy = false;
		bool DrawGrid = false;
		bool DrawSelection = true;
		bool DrawLights = true;
		bool DrawSpawnZones = true;
		int SetCollisionWireframeMode = 0;
	};

	enum class BackgroundMode
	{
		Color = 0,
		Texture = 1,
		Environment3D = 2
	};

	struct GridSettings
	{
		int Slices = 20;
		float Spacing = 1.0f;
		float SecondarySpacing = 10.0f;
		glm::vec4 Color = {1.0f, 1.0f, 1.0f, 0.4f};
		float FadeStart = 0.0f;
		float FadeEnd = 8000.0f;
		float PlaneSize = 15000.0f;
	};

	enum class SceneType
	{
		Default = 0,
		UI = 1
	};

	struct SceneSettings
	{
		std::string Name = "Untitled Scene";
		std::string ScenePath;
		std::shared_ptr<EnvironmentAsset> Environment;

		SceneType Type = SceneType::Default;
		BackgroundMode Mode = BackgroundMode::Environment3D;
		Color BackgroundColor = {30, 30, 30, 255};
		std::string BackgroundTexturePath;

		CanvasSettings Canvas;
		GridSettings Grid;

		DebugRenderFlags DebugFlags;
		float DiagnosticMode = 0.0f;
	};

} // namespace Chained

#endif // CH_SCENE_SETTINGS_H
