#include "script_glue_components.h"
#include "script_glue_registry.h"
#include "engine/scene/components.h"
#include "script_glue_internal.h"

namespace Chained
{
	void ScriptGlue_RegisterComponents(Coral::ManagedAssembly& assembly)
	{
		// ── PlayerComponent ──────────────────────────────────────────────
		CH_BIND_COMPONENT_GETTER(assembly, "Chained.PlayerComponent", "PlayerComponent_GetMovementSpeed_Ptr",
								 PlayerComponent, MovementSpeed);
		CH_BIND_COMPONENT_SETTER(assembly, "Chained.PlayerComponent", "PlayerComponent_SetMovementSpeed_Ptr",
								 PlayerComponent, MovementSpeed);

		CH_BIND_COMPONENT_GETTER(assembly, "Chained.PlayerComponent", "PlayerComponent_GetJumpForce_Ptr",
								 PlayerComponent, JumpForce);
		CH_BIND_COMPONENT_SETTER(assembly, "Chained.PlayerComponent", "PlayerComponent_SetJumpForce_Ptr",
								 PlayerComponent, JumpForce);

		CH_BIND_COMPONENT_GETTER(assembly, "Chained.PlayerComponent", "PlayerComponent_GetLookSensitivity_Ptr",
								 PlayerComponent, LookSensitivity);
		CH_BIND_COMPONENT_SETTER(assembly, "Chained.PlayerComponent", "PlayerComponent_SetLookSensitivity_Ptr",
								 PlayerComponent, LookSensitivity);

		// ── SpawnComponent ──────────────────────────────────────────────
		CH_BIND_COMPONENT_GETTER(assembly, "Chained.SpawnComponent", "SpawnComponent_GetIsActive_Ptr", SpawnComponent,
								 IsActive);
		CH_BIND_COMPONENT_SETTER(assembly, "Chained.SpawnComponent", "SpawnComponent_SetIsActive_Ptr", SpawnComponent,
								 IsActive);

		CH_BIND_COMPONENT_GETTER(assembly, "Chained.SpawnComponent", "SpawnComponent_IsCheckpoint_Ptr", SpawnComponent,
								 IsCheckpoint);
		CH_BIND_COMPONENT_SETTER(assembly, "Chained.SpawnComponent", "SpawnComponent_SetIsCheckpoint_Ptr",
								 SpawnComponent, IsCheckpoint);

		CH_BIND_COMPONENT_GETTER(assembly, "Chained.SpawnComponent", "SpawnComponent_GetSpawnPoint_Ptr", SpawnComponent,
								 SpawnPoint);
		CH_BIND_COMPONENT_SETTER(assembly, "Chained.SpawnComponent", "SpawnComponent_SetSpawnPoint_Ptr", SpawnComponent,
								 SpawnPoint);

		CH_BIND_COMPONENT_GETTER(assembly, "Chained.SpawnComponent", "SpawnComponent_GetRenderSpawnZoneInScene_Ptr",
								 SpawnComponent, RenderSpawnZoneInScene);
		CH_BIND_COMPONENT_GETTER(assembly, "Chained.SpawnComponent", "SpawnComponent_GetZoneSize_Ptr", SpawnComponent,
								 ZoneSize);

		// ── NetworkIdentityComponent ──────────────────────────────────────────────
		CH_BIND_COMPONENT_GETTER(assembly, "Chained.NetworkIdentityComponent",
								 "NetworkIdentityComponent_GetNetworkID_Ptr", NetworkIdentityComponent, NetworkID);
		CH_BIND_COMPONENT_GETTER(assembly, "Chained.NetworkIdentityComponent",
								 "NetworkIdentityComponent_GetIsOwner_Ptr", NetworkIdentityComponent, IsOwner);
		CH_BIND_COMPONENT_GETTER(assembly, "Chained.NetworkIdentityComponent",
								 "NetworkIdentityComponent_GetRemoteActionFlags_Ptr", NetworkIdentityComponent,
								 RemoteActionFlags);
	}
} // namespace Chained
