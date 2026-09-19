#ifndef CH_NETWORK_SYSTEM_H
#define CH_NETWORK_SYSTEM_H

#include "engine/scene/systems/network/network_types.h"
#include "engine/scene/systems/network/network_session_tracker.h"
#include "engine/scene/systems/network/network_input_controller.h"
#include "engine/scene/systems/network/network_replication_manager.h"
#include "engine/common/timestep.h"
#include "engine/common/uuid.h"
#include "engine/networking/net_packet.h"
#include "engine/networking/network_service.h"
#include <entt/entt.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace Chained
{
	class Scene;

	namespace NetworkSystem
	{
		struct SceneNetworkContext
		{
			NetworkSessionTracker Session;
			NetworkInputController Input;
			NetworkReplicationManager Replication;
			Role CallbackRole = Role::Offline;
			bool SceneLoadedPending = false;
			float NetworkTickAccumulator = 0.0f;
		};

		SceneNetworkContext& GetOrCreateContext(Scene* scene);
		SceneNetworkContext* TryGetContext(Scene* scene);

		void EnsureLocalIdentity(Scene* scene);
		void Reset(Scene* scene = nullptr);

		void PollNetwork(Scene* scene, Timestep ts);
		void FinalizeFrame(Scene* scene, Timestep ts);
		void ApplyHostInputs(entt::registry& reg, Timestep ts);
		void InterpolateEntities(entt::registry& reg, float dt);
		void CheckAndPropagateSceneChange(Scene* scene);

		const std::vector<ProcessedInput>& GetPendingInputs(Scene* scene);
		void ClearPendingInputs(Scene* scene);

		void RegisterPeerEntity(Scene* scene, int peer, uint64_t networkID);
		void UnregisterPeer(Scene* scene, int peer);

		void SetPlayerPrefab(Scene* scene, const std::string& path);
		const std::string& GetPlayerPrefab(Scene* scene);

		NetworkSessionTracker* GetSessionTracker(Scene* scene);
		NetworkInputController* GetInputController(Scene* scene);
		NetworkReplicationManager* GetReplicationManager(Scene* scene);
	} // namespace NetworkSystem

} // namespace Chained

#endif // CH_NETWORK_SYSTEM_H
