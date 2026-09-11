// Rendering component registrations (Model, Light, Sprite, Shader, Spawn, Player)
// Split into its own TU to reduce obj file size in MinGW Clang Debug builds.
#include "component_registry.h"
#include "engine/reflection/reflection_rfl_impl.h"
#include "components/render/light_component.h"
#include "components/render/model_component.h"
#include "components/gameplay/player_component.h"
#include "components/render/shader_component.h"
#include "components/gameplay/spawn_component.h"
#include "components/render/sprite_component.h"
#include "thirdparty/IconsFontAwesome6.h"

namespace Chained
{
	void RegisterRenderingComponents()
	{
		ComponentRegistry::RegisterReflective<ModelComponent>("Model", ICON_FA_CUBE, "Rendering");
		ComponentRegistry::RegisterReflective<LightComponent>("Light", ICON_FA_LIGHTBULB, "Rendering");
		ComponentRegistry::RegisterReflective<SpriteComponent>("Sprite", ICON_FA_IMAGE, "Rendering");
		ComponentRegistry::RegisterReflective<ShaderComponent>("Shader", nullptr, "Rendering");
		ComponentRegistry::RegisterReflective<SpawnComponent>("Spawn", ICON_FA_LOCATION_DOT);
		ComponentRegistry::RegisterReflective<PlayerComponent>("Player", ICON_FA_USER);
	}
} // namespace Chained
