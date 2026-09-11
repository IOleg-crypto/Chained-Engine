#include "camera_auto_select_system.h"
#include "engine/core/profiler.h"
#include "engine/scene/components/render/camera_component.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_settings.h"

namespace Chained::CameraAutoSelectSystem
{
	// Returns true if the primary scene camera is orthographic (2D).
	bool IsActiveCamera2D(entt::registry& reg)
	{
		auto view = reg.view<CameraComponent>();
		for (auto entity : view)
		{
			auto& comp = view.get<CameraComponent>(entity);
			if (comp.Primary)
			{
				return comp.Camera.GetProjectionType() == ProjectionType::Orthographic;
			}
		}
		// No primary camera — fall back: check if scene wants 2D
		auto* scenePtr = reg.ctx().find<Scene*>();
		if (scenePtr && *scenePtr)
		{
			auto& settings = (*scenePtr)->GetSettings();
			return (settings.Mode == BackgroundMode::Color || settings.Mode == BackgroundMode::Texture);
		}
		return false;
	}

	// Selects the best camera based on scene BackgroundMode and camera ProjectionType.
	void SelectCamera(entt::registry& reg)
	{
		CH_PROFILE_FUNCTION();

		auto* scenePtr = reg.ctx().find<Scene*>();
		if (!scenePtr || !*scenePtr)
		{
			return;
		}

		Scene* scene = *scenePtr;
		auto& settings = scene->GetSettings();
		bool want2D = (settings.Mode == BackgroundMode::Color || settings.Mode == BackgroundMode::Texture);

		auto view = reg.view<CameraComponent>();

		Entity bestCandidate{};
		for (auto entity : view)
		{
			auto& comp = view.get<CameraComponent>(entity);
			comp.Primary = false; // deselect all in one pass

			if (!bestCandidate)
			{
				bool isOrtho = (comp.Camera.GetProjectionType() == ProjectionType::Orthographic);
				if ((want2D && isOrtho) || (!want2D && !isOrtho))
				{
					bestCandidate = Entity(entity, &reg);
				}
			}
		}

		// Fallback: take the first camera if no perfect match
		if (!bestCandidate)
		{
			for (auto entity : view)
			{
				bestCandidate = Entity(entity, &reg);
				break;
			}
		}

		if (bestCandidate)
		{
			bestCandidate.GetComponent<CameraComponent>().Primary = true;
		}
	}

	void OnRuntimeStart(entt::registry& reg)
	{
		CH_PROFILE_FUNCTION();
		SelectCamera(reg);
	}

	void OnSceneUpdate(entt::registry& reg)
	{
		CH_PROFILE_FUNCTION();
		SelectCamera(reg);
	}
} // namespace Chained::CameraAutoSelectSystem
