#include "script_glue_scene.h"
#include "engine/app/application.h"

#include "engine/scene/scene_events.h"
#include "engine/scene/components.h"

namespace Chained
{
	CH_SCRIPT_FUNC uint64_t Scene_FindEntityByTag(const Coral::UCChar* tag)
	{
		auto* scene = GetActiveScene();
		if (scene && tag)
		{
			auto entity = scene->FindEntityByTag(ch_u16_to_string(tag));
			if (entity)
			{
				return static_cast<uint32_t>(static_cast<entt::entity>(entity));
			}
		}
		return static_cast<uint64_t>(entt::null);
	}

	CH_SCRIPT_FUNC uint64_t Scene_CopyEntity(uint64_t entityID)
	{
		auto* scene = GetActiveScene();
		if (scene)
		{
			auto entity = scene->CopyEntity(static_cast<entt::entity>(entityID));
			return entity != entt::null ? static_cast<uint64_t>(entity) : 0;
		}
		return 0;
	}

	CH_SCRIPT_FUNC void Scene_LoadScene(const Coral::UCChar* path)
	{
		if (!path)
		{
			return;
		}
		SceneChangeRequestEvent e(ch_u16_to_string(path));
		Application::Get().OnEvent(e);
	}

	CH_SCRIPT_FUNC uint64_t Scene_GetPrimaryCameraEntity()
	{
		auto* scene = GetActiveScene();
		if (!scene)
		{
			return 0;
		}

		auto& reg = scene->GetRegistry();
		auto view = reg.view<CameraComponent>();
		for (auto entity : view)
		{
			if (view.get<CameraComponent>(entity).Primary)
			{
				return static_cast<uint64_t>(entity);
			}
		}
		return 0;
	}

	CH_SCRIPT_FUNC const Coral::UCChar* Scene_GetCurrentScenePath()
	{
		static thread_local Coral::UCString s_ScenePathBuffer;
		auto* scene = GetActiveScene();
		if (scene)
		{
			s_ScenePathBuffer = ToWide(scene->GetSettings().ScenePath);
			return s_ScenePathBuffer.c_str();
		}
		s_ScenePathBuffer = ToWide("");
		return s_ScenePathBuffer.c_str();
	}

} // namespace Chained
