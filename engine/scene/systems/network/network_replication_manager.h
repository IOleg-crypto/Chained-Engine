#ifndef CH_NETWORK_REPLICATION_MANAGER_H
#define CH_NETWORK_REPLICATION_MANAGER_H

#include "engine/scene/systems/network/network_types.h"
#include "engine/scene/systems/network/network_session_tracker.h"
#include "engine/common/base.h"
#include "engine/common/uuid.h"
#include "engine/networking/net_packet.h"
#include <entt/entt.hpp>
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdint>

namespace Chained
{
	class Network;
	class Scene;

	class CH_API NetworkReplicationManager
	{
	public:
		NetworkReplicationManager();
		~NetworkReplicationManager() = default;

		void Reset();

		void ProcessWorldStateMessage(WorldStateMessage* msg);
		void ProcessEntitySpawnMessage(EntitySpawnMessage* msg, Scene* scene, NetworkSessionTracker& session);
		void ProcessEntityDestroyMessage(EntityDestroyMessage* msg, Scene* scene, NetworkSessionTracker& session);

		void InterpolateEntities(entt::registry& reg, float dt);
		void BroadcastWorldState(entt::registry& reg, Network* net,
								 const std::unordered_map<uint64_t, uint8_t>& lastActionFlags);

		void SendEntitySpawn(Network* net, uint64_t networkID, const std::string& prefabPath, int clientIndex = -1);
		void SendEntityDestroy(Network* net, uint64_t networkID);

		void EnsureHostIdentity(Scene* scene, NetworkSessionTracker& session);
		void SyncPeerAvatars(Scene* scene, Network* net, NetworkSessionTracker& session);
		void ResyncClientEntities(int clientIndex, Scene* scene, Network* net, NetworkSessionTracker& session);

		void FlushPendingSpawns(Scene* scene, NetworkSessionTracker& session);
		void ClearPendingStates();

		std::unordered_map<int, std::string>& GetDeferredSceneLoaded();

	private:
		std::unordered_map<uint64_t, PendingNetworkState> m_PendingStates;
		std::vector<EntitySpawnMessage> m_PendingEntitySpawns;
		std::unordered_map<int, std::string> m_DeferredSceneLoaded;
		uint32_t m_HostTick = 0;
		bool m_PrefabWarnedOnce = false;
	};
} // namespace Chained

#endif // CH_NETWORK_REPLICATION_MANAGER_H
