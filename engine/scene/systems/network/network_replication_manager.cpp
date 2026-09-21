#include "engine/scene/systems/network/network_replication_manager.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/log.h"
#include "engine/core/service_locator.h"
#include "engine/networking/network_service.h"
#include "engine/scene/scene.h"
#include "engine/scene/prefab_serializer.h"
#include "engine/scene/components/core/id_component.h"
#include "engine/scene/components/core/transform_component.h"
#include "engine/scene/components/core/hierarchy_component.h"
#include "engine/scene/components/gameplay/player_component.h"
#include "engine/scene/components/gameplay/spawn_component.h"
#include "engine/scene/components/gameplay/network_identity_component.h"
#include "engine/scene/components/gameplay/network_interpolation_component.h"
#include "engine/scene/components/physics/physics_component.h"
#include "engine/scene/systems/transform_system.h"
#include <filesystem>
#include <cmath>

namespace Chained
{
	static bool IsLobbyOrMenuScene(Scene* scene)
	{
		if (!scene)
		{
			return false;
		}
		const std::string& path = scene->GetSettings().ScenePath;
		return path.find("start_menu") != std::string::npos || path.find("lobby") != std::string::npos ||
			   path.find("menu") != std::string::npos;
	}

	static std::string NormalizeToAssetPath(const std::string& path)
	{
		if (path.empty())
		{
			return "";
		}
		std::string s = path;
		for (char& c : s)
		{
			if (c == '\\')
			{
				c = '/';
			}
		}
		size_t pos = s.rfind("scenes/");
		if (pos != std::string::npos)
		{
			return s.substr(pos);
		}
		pos = s.rfind("assets/");
		if (pos != std::string::npos)
		{
			return s.substr(pos + 7);
		}
		return std::filesystem::path(s).filename().string();
	}

	static bool AreScenePathsMatching(const std::string& a, const std::string& b)
	{
		if (a.empty() && b.empty())
		{
			return true;
		}
		std::string normA = NormalizeToAssetPath(a);
		std::string normB = NormalizeToAssetPath(b);
		if (normA == normB)
		{
			return true;
		}
		std::filesystem::path pA(normA);
		std::filesystem::path pB(normB);
		return !pA.filename().empty() && pA.filename() == pB.filename();
	}

	static glm::vec3 FindSpawnPosition(entt::registry& reg)
	{
		for (auto [entity, spawn] : reg.view<SpawnComponent>().each())
		{
			if (spawn.IsActive)
			{
				if (auto* transform = reg.try_get<TransformComponent>(entity))
				{
					glm::vec3 pos = transform->Translation + spawn.SpawnPoint;
					CH_CORE_TRACE("Network: FindSpawnPosition — using SpawnComponent, pos=({:.1f}, {:.1f}, {:.1f}).",
								  pos.x, pos.y, pos.z);
					return pos;
				}
			}
		}
		for (auto [entity, player] : reg.view<PlayerComponent>().each())
		{
			if (auto* transform = reg.try_get<TransformComponent>(entity))
			{
				CH_CORE_TRACE("Network: FindSpawnPosition — using PlayerComponent, pos=({:.1f}, {:.1f}, {:.1f}).",
							  transform->Translation.x, transform->Translation.y, transform->Translation.z);
				return transform->Translation;
			}
		}
		CH_CORE_WARN(
			"Network: FindSpawnPosition — no SpawnComponent or PlayerComponent found, using default (0, 100, 0).");
		return {0, 100, 0};
	}

	static glm::quat SafeNormalizeQuat(const glm::quat& q)
	{
		float len2 = glm::dot(q, q);
		if (len2 < 1e-6f || std::isnan(len2) || std::isinf(len2))
		{
			return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		}
		return glm::normalize(q);
	}

	NetworkReplicationManager::NetworkReplicationManager()
	{
		Reset();
	}

	void NetworkReplicationManager::Reset()
	{
		m_PendingStates.clear();
		m_PendingEntitySpawns.clear();
		m_DeferredSceneLoaded.clear();
		m_HostTick = 0;
		m_PrefabWarnedOnce = false;
	}

	void NetworkReplicationManager::ClearPendingStates()
	{
		m_PendingStates.clear();
	}

	std::unordered_map<int, std::string>& NetworkReplicationManager::GetDeferredSceneLoaded()
	{
		return m_DeferredSceneLoaded;
	}

	void NetworkReplicationManager::ProcessWorldStateMessage(WorldStateMessage* msg)
	{
		if (!msg)
		{
			return;
		}

		auto it = m_PendingStates.find(msg->NetworkID);
		if (it != m_PendingStates.end() && msg->Tick < it->second.LastTick &&
			(it->second.LastTick - msg->Tick) < NetworkConstants::kTickWrapGuard)
		{
			return; // Drop out-of-order / older state
		}

		PendingNetworkState state;
		state.NetworkID = msg->NetworkID;
		state.LastTick = msg->Tick;
		state.TargetPosition = {msg->Position[0], msg->Position[1], msg->Position[2]};
		state.TargetRotation = {msg->Rotation[0], msg->Rotation[1], msg->Rotation[2], msg->Rotation[3]};
		state.TargetVelocity = {msg->Velocity[0], msg->Velocity[1], msg->Velocity[2]};
		state.IsGrounded = (msg->IsGrounded != 0);
		state.ActionFlags = msg->ActionFlags;

		if (it != m_PendingStates.end())
		{
			state.RenderPosition = it->second.RenderPosition;
			state.RenderRotation = it->second.RenderRotation;
			state.RenderInitialized = it->second.RenderInitialized;
		}

		m_PendingStates[msg->NetworkID] = state;
	}

	void NetworkReplicationManager::ProcessEntitySpawnMessage(EntitySpawnMessage* msg, Scene* scene,
															  NetworkSessionTracker& session)
	{
		if (!msg || !scene)
		{
			return;
		}

		if (msg->NetworkID == 0 || msg->PrefabPath[0] == '\0')
		{
			return;
		}

		if (session.GetEntityForNetworkID(msg->NetworkID) != UUID(0))
		{
			return;
		}

		entt::registry& reg = scene->GetRegistry();
		auto view = reg.view<NetworkIdentityComponent>();
		for (auto entity : view)
		{
			if (view.get<NetworkIdentityComponent>(entity).NetworkID == msg->NetworkID)
			{
				if (reg.all_of<IDComponent>(entity))
				{
					session.MapNetworkIDToEntity(msg->NetworkID, reg.get<IDComponent>(entity).ID);
				}
				return;
			}
		}

		std::string path = msg->PrefabPath;
		if (auto* am = ServiceLocator::TryGet<AssetManager>())
		{
			path = am->ResolvePath(path);
		}

		Entity avatar = PrefabSerializer::Deserialize(scene, path);
		if (!avatar)
		{
			CH_CORE_ERROR("Network: Failed to spawn replicated prefab '{}' (netID={}).", msg->PrefabPath,
						  msg->NetworkID);
			return;
		}

		auto* net = ServiceLocator::TryGet<Network>();
		uint64_t localNetID = session.GetLocalNetworkID();
		if (localNetID == 0 && net)
		{
			localNetID = net->GetLocalNetworkID();
			if (localNetID != 0)
			{
				session.SetLocalNetworkID(localNetID);
			}
		}

		if (avatar.HasComponent<TransformComponent>())
		{
			auto& transform = avatar.GetComponent<TransformComponent>();
			glm::vec3 spawnPos = FindSpawnPosition(scene->GetRegistry());
			if (msg->NetworkID > 1)
			{
				spawnPos.x += static_cast<float>(msg->NetworkID - 1) * 4.0f;
			}
			TransformSystem::SetTranslation(transform, spawnPos);
		}

		auto& netID = avatar.AddOrReplaceComponent<NetworkIdentityComponent>();
		netID.NetworkID = msg->NetworkID;
		netID.PrefabPath = msg->PrefabPath;
		if (localNetID != 0)
		{
			netID.IsOwner = (msg->NetworkID == localNetID);
		}
		else
		{
			netID.IsOwner = false;
			CH_CORE_WARN("Network: EntitySpawn for netID={} arrived before PlayerAssign — IsOwner deferred.",
						 msg->NetworkID);
		}

		if (!netID.IsOwner)
		{
			avatar.AddOrReplaceComponent<NetworkInterpolationComponent>();
			if (avatar.HasComponent<RigidBodyComponent>())
			{
				avatar.GetComponent<RigidBodyComponent>().IsNetworkDriven = true;
			}
		}

		session.MapNetworkIDToEntity(msg->NetworkID, avatar.GetUUID());
		CH_CORE_INFO("Network: Spawned replicated entity (netID={}, owner={}).", msg->NetworkID, netID.IsOwner);
	}

	void NetworkReplicationManager::ProcessEntityDestroyMessage(EntityDestroyMessage* msg, Scene* scene,
																NetworkSessionTracker& session)
	{
		if (!msg || !scene)
		{
			return;
		}

		UUID entityUUID = session.GetEntityForNetworkID(msg->NetworkID);
		if (entityUUID != UUID(0))
		{
			Entity entity = scene->GetEntityByUUID(entityUUID);
			if (entity && entity.IsValid())
			{
				scene->DestroyEntity(entity);
			}
			session.RemoveNetworkID(msg->NetworkID);
		}

		entt::registry& reg = scene->GetRegistry();
		std::vector<entt::entity> toDestroy;
		auto view = reg.view<NetworkIdentityComponent>();
		for (auto entity : view)
		{
			if (view.get<NetworkIdentityComponent>(entity).NetworkID == msg->NetworkID)
			{
				toDestroy.push_back(entity);
			}
		}
		for (auto entity : toDestroy)
		{
			Entity e(entity, &reg);
			if (e.IsValid())
			{
				scene->DestroyEntity(e);
			}
		}

		m_PendingStates.erase(msg->NetworkID);
		CH_CORE_INFO("Network: Destroyed replicated entity (netID={}).", msg->NetworkID);
	}

	void NetworkReplicationManager::InterpolateEntities(entt::registry& reg, float dt)
	{
		auto view = reg.view<NetworkIdentityComponent, TransformComponent>();
		for (auto entity : view)
		{
			auto& netID = view.get<NetworkIdentityComponent>(entity);
			auto& transform = view.get<TransformComponent>(entity);

			auto it = m_PendingStates.find(netID.NetworkID);
			if (it == m_PendingStates.end())
			{
				if (auto* rb = reg.try_get<RigidBodyComponent>(entity))
				{
					rb->IsNetworkDriven = !netID.IsOwner;
				}
				continue;
			}

			auto& target = it->second;
			bool changed = false;

			if (netID.IsOwner)
			{
				if (auto* rb = reg.try_get<RigidBodyComponent>(entity))
				{
					rb->IsNetworkDriven = false;
					rb->IsGrounded = target.IsGrounded;
				}
			}
			else
			{
				if (auto* rb = reg.try_get<RigidBodyComponent>(entity))
				{
					rb->IsNetworkDriven = true;
				}

				if (!target.RenderInitialized)
				{
					target.RenderPosition = target.TargetPosition;
					target.RenderRotation = SafeNormalizeQuat(target.TargetRotation);
					target.RenderInitialized = true;
				}

				glm::vec3 goalPos = target.TargetPosition + target.TargetVelocity * dt;
				constexpr float RemoteInterpSpeed = 10.0f;
				float rt = glm::clamp(1.0f - std::exp(-RemoteInterpSpeed * dt), 0.0f, 1.0f);

				float snapDist = glm::length(target.RenderPosition - goalPos);
				if (snapDist > NetworkConstants::kMaxCorrectionDistance)
				{
					target.RenderPosition = target.TargetPosition;
					target.RenderRotation = SafeNormalizeQuat(target.TargetRotation);
				}
				else
				{
					target.RenderPosition = glm::mix(target.RenderPosition, goalPos, rt);
					target.RenderRotation = glm::slerp(SafeNormalizeQuat(target.RenderRotation),
													   SafeNormalizeQuat(target.TargetRotation), rt);
				}

				TransformSystem::SetTranslation(transform, target.RenderPosition);
				TransformSystem::SetRotationQuat(transform, target.RenderRotation);

				if (auto* rb = reg.try_get<RigidBodyComponent>(entity))
				{
					rb->Velocity = target.TargetVelocity;
					rb->IsGrounded = target.IsGrounded;
				}
				if (auto* netIDComp = reg.try_get<NetworkIdentityComponent>(entity))
				{
					netIDComp->RemoteActionFlags = target.ActionFlags;
				}
				changed = true;
			}

			if (changed)
			{
				glm::mat4 localMatrix = TransformSystem::ComputeLocalMatrix(transform);
				if (reg.all_of<HierarchyComponent>(entity))
				{
					auto& hc = reg.get<HierarchyComponent>(entity);
					if (hc.Parent != entt::null && reg.valid(hc.Parent) && reg.all_of<TransformComponent>(hc.Parent))
					{
						transform.WorldTransform = reg.get<TransformComponent>(hc.Parent).WorldTransform * localMatrix;
					}
					else
					{
						transform.WorldTransform = localMatrix;
					}
				}
				else
				{
					transform.WorldTransform = localMatrix;
				}
				transform.InverseWorldTransform = glm::inverse(transform.WorldTransform);
				transform.TransformChanged = false;
			}
		}
	}

	void NetworkReplicationManager::BroadcastWorldState(entt::registry& reg, Network* net,
														const std::unordered_map<uint64_t, uint8_t>& lastActionFlags)
	{
		if (!net || !net->IsHost())
		{
			return;
		}

		struct EntityNetState
		{
			uint64_t NetworkID;
			glm::vec3 Position;
			glm::quat Rotation;
			glm::vec3 Velocity;
			bool IsGrounded;
			uint8_t ActionFlags;
		};

		std::vector<EntityNetState> states;
		auto view = reg.view<NetworkIdentityComponent, TransformComponent>();
		for (auto entity : view)
		{
			auto& netID = view.get<NetworkIdentityComponent>(entity);
			auto& transform = view.get<TransformComponent>(entity);

			EntityNetState s;
			s.NetworkID = netID.NetworkID;
			s.Position = TransformSystem::GetTranslation(transform);
			s.Rotation = glm::quat_cast(transform.WorldTransform);
			if (auto* rb = reg.try_get<RigidBodyComponent>(entity))
			{
				s.Velocity = rb->Velocity;
				s.IsGrounded = rb->IsGrounded;
			}
			else
			{
				s.Velocity = {0, 0, 0};
				s.IsGrounded = false;
			}
			auto flagIt = lastActionFlags.find(netID.NetworkID);
			s.ActionFlags = (flagIt != lastActionFlags.end()) ? flagIt->second : 0;
			states.push_back(s);
		}

		if (states.empty() || net->GetClientCount() == 0)
		{
			return;
		}

		++m_HostTick;

		for (auto& s : states)
		{
			net->BroadcastPacket(MessageType_WorldState, false, [this, &s](ByteWriter& bw) {
				WorldStateMessage msg;
				msg.Tick = m_HostTick;
				msg.NetworkID = s.NetworkID;
				msg.Position[0] = s.Position.x;
				msg.Position[1] = s.Position.y;
				msg.Position[2] = s.Position.z;
				msg.Rotation[0] = s.Rotation.w;
				msg.Rotation[1] = s.Rotation.x;
				msg.Rotation[2] = s.Rotation.y;
				msg.Rotation[3] = s.Rotation.z;
				msg.Velocity[0] = s.Velocity.x;
				msg.Velocity[1] = s.Velocity.y;
				msg.Velocity[2] = s.Velocity.z;
				msg.IsGrounded = s.IsGrounded ? 1 : 0;
				msg.ActionFlags = s.ActionFlags;
				msg.Encode(bw);
			});
		}
	}

	void NetworkReplicationManager::SendEntitySpawn(Network* net, uint64_t networkID, const std::string& prefabPath,
													int clientIndex)
	{
		if (!net)
		{
			return;
		}

		if (clientIndex == kInvalidPeerHandle)
		{
			net->BroadcastPacket(MessageType_EntitySpawn, true, [networkID, &prefabPath](ByteWriter& bw) {
				EntitySpawnMessage msg;
				msg.NetworkID = networkID;
				std::strncpy(msg.PrefabPath, prefabPath.c_str(), sizeof(msg.PrefabPath) - 1);
				msg.PrefabPath[sizeof(msg.PrefabPath) - 1] = '\0';
				msg.Encode(bw);
			});
		}
		else
		{
			EntitySpawnMessage msg;
			msg.NetworkID = networkID;
			std::strncpy(msg.PrefabPath, prefabPath.c_str(), sizeof(msg.PrefabPath) - 1);
			msg.PrefabPath[sizeof(msg.PrefabPath) - 1] = '\0';
			ByteWriter w;
			msg.Encode(w);
			net->SendPacket(clientIndex, MessageType_EntitySpawn, w.Data().data(), w.Data().size(), true);
		}
	}

	void NetworkReplicationManager::SendEntityDestroy(Network* net, uint64_t networkID)
	{
		if (!net)
		{
			return;
		}

		net->BroadcastPacket(MessageType_EntityDestroy, true, [networkID](ByteWriter& bw) {
			EntityDestroyMessage msg;
			msg.NetworkID = networkID;
			msg.Encode(bw);
		});
	}

	void NetworkReplicationManager::EnsureHostIdentity(Scene* scene, NetworkSessionTracker& session)
	{
		if (!scene || IsLobbyOrMenuScene(scene))
		{
			return;
		}

		entt::registry& reg = scene->GetRegistry();
		auto owned = reg.view<NetworkIdentityComponent>();
		for (auto entity : owned)
		{
			if (owned.get<NetworkIdentityComponent>(entity).NetworkID == kHostNetworkID)
			{
				return;
			}
		}

		const std::string& playerPrefab = session.GetPlayerPrefab();

		auto players = reg.view<PlayerComponent>();
		for (auto entity : players)
		{
			if (reg.all_of<NetworkIdentityComponent>(entity))
			{
				continue;
			}

			auto& netID = reg.emplace<NetworkIdentityComponent>(entity);
			netID.NetworkID = kHostNetworkID;
			netID.IsOwner = true;
			netID.PrefabPath = playerPrefab;

			if (reg.all_of<TransformComponent>(entity))
			{
				auto& transform = reg.get<TransformComponent>(entity);
				glm::vec3 spawnPos = FindSpawnPosition(reg);
				CH_CORE_INFO("Network: Tagged host player with netID={}, spawnPos=({:.1f}, {:.1f}, {:.1f}).",
							 kHostNetworkID, spawnPos.x, spawnPos.y, spawnPos.z);
				TransformSystem::SetTranslation(transform, spawnPos);
			}
			return;
		}

		if (playerPrefab.empty())
		{
			CH_CORE_WARN("Network: EnsureHostIdentity — no PlayerComponent in scene and playerPrefab is empty.");
			return;
		}

		std::string path = playerPrefab;
		if (auto* am = ServiceLocator::TryGet<AssetManager>())
		{
			path = am->ResolvePath(playerPrefab);
		}

		Entity hostAvatar = PrefabSerializer::Deserialize(scene, path);
		if (!hostAvatar)
		{
			CH_CORE_ERROR("Network: EnsureHostIdentity — failed to deserialize prefab '{}'.", playerPrefab);
			return;
		}

		if (hostAvatar.HasComponent<TransformComponent>())
		{
			auto& transform = hostAvatar.GetComponent<TransformComponent>();
			glm::vec3 spawnPos = FindSpawnPosition(reg);
			CH_CORE_INFO("Network: Spawned host avatar from prefab, spawnPos=({:.1f}, {:.1f}, {:.1f}).", spawnPos.x,
						 spawnPos.y, spawnPos.z);
			TransformSystem::SetTranslation(transform, spawnPos);
		}

		auto& netID = hostAvatar.AddOrReplaceComponent<NetworkIdentityComponent>();
		netID.NetworkID = kHostNetworkID;
		netID.IsOwner = true;
		netID.PrefabPath = playerPrefab;
	}

	void NetworkReplicationManager::SyncPeerAvatars(Scene* scene, Network* net, NetworkSessionTracker& session)
	{
		if (!scene || !net || IsLobbyOrMenuScene(scene))
		{
			return;
		}

		const std::string& playerPrefab = session.GetPlayerPrefab();

		if (playerPrefab.empty())
		{
			if (!m_PrefabWarnedOnce && net->GetClientCount() > 0)
			{
				m_PrefabWarnedOnce = true;
				CH_CORE_ERROR("Network: No player prefab configured — clients will connect without an avatar. "
							  "Call NetworkSystem::SetPlayerPrefab().");
			}
			return;
		}

		for (int clientIndex = 0; clientIndex < net->GetMaxClients(); ++clientIndex)
		{
			if (!net->IsClientConnected(clientIndex))
			{
				continue;
			}

			if (session.GetPeerAvatar(clientIndex) != UUID(0))
			{
				continue;
			}

			uint64_t networkID = net->GetNetworkIDForConnection(clientIndex);
			if (networkID == 0)
			{
				continue;
			}
			session.RegisterPeer(clientIndex, networkID);

			const std::string& currentScenePath = scene->GetSettings().ScenePath;
			if (!currentScenePath.empty())
			{
				std::string relScenePath = NormalizeToAssetPath(currentScenePath);
				SceneChangeMessage sceneMsg;
				std::strncpy(sceneMsg.ScenePath, relScenePath.c_str(), sizeof(sceneMsg.ScenePath) - 1);
				sceneMsg.ScenePath[sizeof(sceneMsg.ScenePath) - 1] = '\0';
				ByteWriter sw;
				sceneMsg.Encode(sw);
				net->SendPacket(clientIndex, MessageType_SceneChange, sw.Data().data(), sw.Data().size(), true);
				CH_CORE_INFO("Network: Sent active scene '{}' to newly connected client {}.", relScenePath,
							 clientIndex);
			}

			std::string path = playerPrefab;
			if (auto* am = ServiceLocator::TryGet<AssetManager>())
			{
				path = am->ResolvePath(playerPrefab);
			}

			Entity avatar = PrefabSerializer::Deserialize(scene, path);
			if (!avatar)
			{
				CH_CORE_ERROR("Network: Failed to spawn player prefab '{}' for client {}.", playerPrefab, clientIndex);
				session.SetPeerAvatar(clientIndex, UUID(0));
				continue;
			}

			if (avatar.HasComponent<TransformComponent>())
			{
				auto& transform = avatar.GetComponent<TransformComponent>();
				glm::vec3 spawnPos = FindSpawnPosition(scene->GetRegistry());
				spawnPos.x += static_cast<float>(networkID - 1) * 4.0f;
				TransformSystem::SetTranslation(transform, spawnPos);
			}

			auto& netID = avatar.AddOrReplaceComponent<NetworkIdentityComponent>();
			netID.NetworkID = networkID;
			netID.IsOwner = false;
			netID.PrefabPath = playerPrefab;

			session.SetPeerAvatar(clientIndex, avatar.GetUUID());
			CH_CORE_INFO("Network: Spawned avatar (netID={}) for client {}.", networkID, clientIndex);

			PlayerAssignMessage assignMsg;
			assignMsg.NetworkID = networkID;
			ByteWriter w;
			assignMsg.Encode(w);
			net->SendPacket(clientIndex, MessageType_PlayerAssign, w.Data().data(), w.Data().size(), true);

			for (const auto& [existingClient, uuid] : session.GetPeerToAvatarMap())
			{
				if (existingClient == clientIndex || uuid == UUID(0))
				{
					continue;
				}
				uint64_t existingNetID = session.GetNetworkIDForPeer(existingClient);
				if (existingNetID != 0)
				{
					SendEntitySpawn(net, existingNetID, playerPrefab, clientIndex);
				}
				SendEntitySpawn(net, networkID, playerPrefab, existingClient);
			}
			SendEntitySpawn(net, kHostNetworkID, playerPrefab, clientIndex);
			SendEntitySpawn(net, networkID, playerPrefab, clientIndex);
		}

		std::vector<int> disconnectedClients;
		for (const auto& [clientIndex, _] : session.GetPeerToAvatarMap())
		{
			if (!net->IsClientConnected(clientIndex))
			{
				disconnectedClients.push_back(clientIndex);
			}
		}

		for (int clientIndex : disconnectedClients)
		{
			uint64_t netIDToDestroy = session.GetNetworkIDForPeer(clientIndex);
			UUID avatarUUID = session.GetPeerAvatar(clientIndex);
			if (avatarUUID != UUID(0))
			{
				Entity avatar = scene->GetEntityByUUID(avatarUUID);
				if (avatar && avatar.IsValid())
				{
					scene->DestroyEntity(avatar);
				}
			}

			if (netIDToDestroy != 0)
			{
				entt::registry& reg = scene->GetRegistry();
				std::vector<entt::entity> toDestroy;
				auto view = reg.view<NetworkIdentityComponent>();
				for (auto entity : view)
				{
					if (view.get<NetworkIdentityComponent>(entity).NetworkID == netIDToDestroy)
					{
						toDestroy.push_back(entity);
					}
				}
				for (auto entity : toDestroy)
				{
					Entity e(entity, &reg);
					if (e.IsValid())
					{
						scene->DestroyEntity(e);
					}
				}

				SendEntityDestroy(net, netIDToDestroy);
			}

			session.UnregisterPeer(clientIndex);
			CH_CORE_INFO("Network: Despawned avatar for client {} (netID={}).", clientIndex, netIDToDestroy);
		}
	}

	void NetworkReplicationManager::ResyncClientEntities(int clientIndex, Scene* scene, Network* net,
														 NetworkSessionTracker& session)
	{
		if (!net || !scene)
		{
			return;
		}

		EnsureHostIdentity(scene, session);

		uint64_t clientNetID = session.GetNetworkIDForPeer(clientIndex);
		if (clientNetID == 0)
		{
			clientNetID = net->GetNetworkIDForConnection(clientIndex);
		}

		if (clientNetID == 0)
		{
			CH_CORE_WARN("Network: ResyncClientEntities — no networkID for client {} yet, deferring.", clientIndex);
			m_DeferredSceneLoaded[clientIndex] = scene->GetSettings().ScenePath;
			return;
		}

		{
			PlayerAssignMessage assignMsg;
			assignMsg.NetworkID = clientNetID;
			ByteWriter w;
			assignMsg.Encode(w);
			net->SendPacket(clientIndex, MessageType_PlayerAssign, w.Data().data(), w.Data().size(), true);
		}

		int count = 0;
		entt::registry& reg = scene->GetRegistry();
		auto view = reg.view<NetworkIdentityComponent>();
		for (auto entity : view)
		{
			auto& netID = view.get<NetworkIdentityComponent>(entity);
			if (netID.NetworkID == 0)
			{
				continue;
			}
			std::string prefabPath = netID.PrefabPath.empty() ? session.GetPlayerPrefab() : netID.PrefabPath;
			SendEntitySpawn(net, netID.NetworkID, prefabPath, clientIndex);
			count++;
		}

		CH_CORE_INFO("Network: Resynced {} entities for client {} (netID={}).", count, clientIndex, clientNetID);
		net->BroadcastPlayerList();
	}

	void NetworkReplicationManager::FlushPendingSpawns(Scene* scene, NetworkSessionTracker& session)
	{
		if (!scene || m_PendingEntitySpawns.empty())
		{
			return;
		}

		for (auto& spawnMsg : m_PendingEntitySpawns)
		{
			ProcessEntitySpawnMessage(&spawnMsg, scene, session);
		}
		m_PendingEntitySpawns.clear();
	}

} // namespace Chained
