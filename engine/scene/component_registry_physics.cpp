// Physics component registrations (RigidBody, Collider)
// Split into its own TU to reduce obj file size in MinGW Clang Debug builds.
#include "component_registry.h"
#include "engine/reflection/reflection_rfl_impl.h"
#include "components/physics/physics_component.h"
#include "thirdparty/IconsFontAwesome6.h"

namespace Chained
{
	void RegisterPhysicsComponents()
	{
		ComponentRegistry::RegisterReflective<RigidBodyComponent>("Rigid Body", ICON_FA_CUBES, "Physics");
		ComponentRegistry::RegisterReflective<ColliderComponent>("Collider", ICON_FA_SHIELD, "Physics");
	}
} // namespace Chained
