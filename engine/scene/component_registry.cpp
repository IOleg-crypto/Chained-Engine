#include "component_registry.h"

namespace Chained
{

	void ComponentRegistry::Register(::entt::id_type typeId, const ComponentMetadata& metadata)
	{
		s_Registry[typeId] = metadata;
	}

	// Extern declarations for per-category registries to reduce obj file sizes
	extern void RegisterCoreComponents();
	extern void RegisterRenderingComponents();
	extern void RegisterPhysicsComponents();
	extern void RegisterGameplayComponents();
	extern void RegisterScriptingComponents();
	extern void RegisterUIComponents();
	extern void RegisterUIControlComponent();

	void ComponentRegistry::RegisterEngineComponents()
	{
		RegisterCoreComponents();
		RegisterRenderingComponents();
		RegisterPhysicsComponents();
		RegisterGameplayComponents();
		RegisterScriptingComponents();

		// UI
		RegisterUIComponents();
		RegisterUIControlComponent();
	}
} // namespace Chained
