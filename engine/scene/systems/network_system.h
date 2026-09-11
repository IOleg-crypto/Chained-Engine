#ifndef CH_NETWORK_SYSTEM_H
#define CH_NETWORK_SYSTEM_H

#include "engine/common/timestep.h"
#include "engine/common/uuid.h"
#include "engine/core/service.h"
#include "engine/networking/net_packet.h"
#include "engine/networking/network_service.h"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <cstdint>

namespace Chained
{
	class Scene;

	// NetworkPeerHandle and Role are now defined in network_service.h

	struct PendingNetworkState
	{
		uint64_t NetworkID = 0;
		uint32_t LastTick = 0;
		glm::vec3 TargetPosition = {0, 0, 0};
		glm::quat TargetRotation = {1, 0, 0, 0};
		glm::vec3 TargetVelocity = {0, 0, 0};
		bool IsGrounded = false;
		uint8_t ActionFlags = 0;
	};

	struct ProcessedInput
	{
		uint64_t NetworkID = 0;
		float MoveX = 0.0f;
		float MoveZ = 0.0f;
		uint8_t ActionFlags = 0;
		float MouseX = 0.0f;
		float MouseY = 0.0f;
	};

	class NetworkSystem : public Service
	{
	public:
		static NetworkSystem& GetInstance();

		void Initialize() override
		{
		}
		void Shutdown() override
		{
			Reset();
		}

		// Maximum distance (m) between local and server position before snap-correction
		// kicks in for owned entities. Below this threshold, local physics runs freely.
		static constexpr float kMaxCorrectionDistance = 10.0f;

		// Ensure the local player entity has a NetworkIdentityComponent BEFORE scripts
		// run. Must be called early in the frame so PlayerController etc. can find it.
		void EnsureLocalIdentity(Scene* scene);

		// Resets all per-session state. Must be called from OnRuntimeStop.
		void Reset();

		void PollNetwork(Scene* scene, Timestep ts);
		void FinalizeFrame(Scene* scene, Timestep ts);
		void ApplyHostInputs(entt::registry& reg, Timestep ts);
		void InterpolateEntities(entt::registry& reg, float dt);
		void CheckAndPropagateSceneChange(Scene* scene);

		const std::vector<ProcessedInput>& GetPendingInputs();
		void ClearPendingInputs();

		void RegisterPeerEntity(int peer, uint64_t networkID);
		void UnregisterPeer(int peer);

		// Prefab instantiated for each connecting client. Relative to the assets
		// directory. Empty by default — clients get no avatar until this is set.
		void SetPlayerPrefab(const std::string& path);
		const std::string& GetPlayerPrefab();

	private:
		// ---- Incoming message processing ----
		void ProcessWorldStateMessage(WorldStateMessage* msg);
		void ProcessSceneChangeMessage(SceneChangeMessage* msg);
		void ProcessEntitySpawnMessage(EntitySpawnMessage* msg, Scene* scene);
		void ProcessEntityDestroyMessage(EntityDestroyMessage* msg, Scene* scene);
		void ProcessPlayerAssignMessage(PlayerAssignMessage* msg);

		// ---- Incoming message processing (host side) ----
		void ProcessInputStateMessage(InputStateMessage* msg, int clientIndex);
		void ProcessPlayerInfoMessage(PlayerInfoMessage* msg, int clientIndex);
		void ProcessPlayerListMessage(PlayerListMessage* msg);
		void ProcessChatMessageMessage(Chained::ChatMessageMessage* msg);

		// ---- Host-side helpers ----
		void EnsureHostIdentity(Scene* scene);
		void SyncPeerAvatars(Scene* scene, Network* net);
		void BroadcastWorldState(entt::registry& reg);
		void SendEntitySpawn(Network* net, uint64_t networkID, const std::string& prefabPath, int clientIndex);
		void SendEntityDestroy(Network* net, uint64_t networkID);
		void ResyncClientEntities(int clientIndex, Scene* scene);

		// ---- Client-side helpers ----
		void CollectAndSendInput(Network* net, float dt);

		// ---- Packet callback installer ----
		void InstallPacketCallback();

		// ---- Member state (previously file-scope globals) ----
		std::unordered_map<uint64_t, PendingNetworkState> m_PendingStates;
		std::vector<ProcessedInput> m_PendingInputs;
		std::unordered_map<uint64_t, ProcessedInput> m_ActiveClientInputs;
		std::unordered_map<uint64_t, float> m_ActiveClientInputTimers;
		uint32_t m_ClientTick = 0;
		uint32_t m_HostTick = 0;
		std::unordered_map<int, uint64_t> m_PeerToNetworkID;
		Role m_CallbackRole = Role::Offline;
		std::string m_PlayerPrefab = "prefab/player.chprefab";
		std::unordered_map<int, UUID> m_PeerToAvatar;
		std::unordered_map<uint64_t, UUID> m_NetworkIDToEntity;
		uint64_t m_LocalNetworkID = 0;
		std::unordered_map<int, std::pair<std::string, uint8_t>> m_PendingPlayerInfo;
		std::unordered_map<uint64_t, uint8_t> m_LastActionFlags;
		Scene* m_ReplicationScene = nullptr;
		bool m_PrefabWarnedOnce = false;
		bool m_SceneLoadedPending = false;
		std::unordered_map<int, std::string> m_DeferredSceneLoaded;
		std::vector<EntitySpawnMessage>
			m_PendingEntitySpawns; // BUG #2: buffer spawns that arrive before scene is ready
		std::unordered_set<uint64_t> m_WarnedInputNetID;
		float m_NetworkTickAccumulator = 0.0f;
		static constexpr float kNetworkTickInterval = 1.0f / 64.0f; // 64 Hz (Valve / Source tickrate standard)
	};

} // namespace Chained

#endif // CH_NETWORK_SYSTEM_H
