#include "network_system.h"
#include "engine/scene/components/gameplay/network_identity_component.h"
#include "engine/scene/components/core/transform_component.h"
#include "engine/scene/components/core/hierarchy_component.h"
#include "engine/scene/systems/transform_system.h"
#include "engine/scene/components/gameplay/player_component.h"
#include "engine/scene/components/physics/physics_component.h"
#include "engine/scene/components/render/camera_component.h"
#include "engine/scene/components/gameplay/spawn_component.h"
#include "engine/networking/network_service.h"
#include "engine/scene/scene.h"
#include "engine/scene/prefab_serializer.h"
#include "engine/scene/scene_events.h"
#include "engine/app/application.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/service_locator.h"
#include "engine/core/input.h"
#include "engine/core/key_codes.h"

#include <glm/gtc/quaternion.hpp>
#include <cstring>
#include <unordered_set>

namespace Chained
{

	NetworkSystem& NetworkSystem::GetInstance()
	{
		return *ServiceLocator::Get<NetworkSystem>();
	}

	// ---- Peer mapping ----

	void NetworkSystem::SetPlayerPrefab(const std::string& path)
	{
		m_PlayerPrefab = path;
	}

	const std::string& NetworkSystem::GetPlayerPrefab()
	{
		return m_PlayerPrefab;
	}

	void NetworkSystem::RegisterPeerEntity(int peer, uint64_t networkID)
	{
		m_PeerToNetworkID[peer] = networkID;
	}

	void NetworkSystem::UnregisterPeer(int peer)
	{
		m_PeerToNetworkID.erase(peer);
	}

	// ---- Incoming message processing (client side) ----

	void NetworkSystem::ProcessWorldStateMessage(WorldStateMessage* msg)
	{
		if (!msg)
		{
			return;
		}

		auto it = m_PendingStates.find(msg->NetworkID);
		if (it != m_PendingStates.end() && msg->Tick < it->second.LastTick &&
			(it->second.LastTick - msg->Tick) < 100000)
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

		m_PendingStates[state.NetworkID] = state;
	}

	void NetworkSystem::ProcessSceneChangeMessage(SceneChangeMessage* msg)
	{
		if (!msg || msg->ScenePath[0] == '\0')
		{
			return;
		}

		auto* net = ServiceLocator::TryGet<Network>();
		if (net)
		{
			net->SetPendingSceneChange(std::string(msg->ScenePath));
			CH_CORE_INFO("Network: Received scene change -> {}", msg->ScenePath);
		}
	}

	static bool IsLobbyOrMenuScene(const Scene* scene)
	{
		if (!scene)
		{
			return true;
		}
		if (scene->GetSettings().Type == SceneType::UI)
		{
			return true;
		}
		std::string path = scene->GetSettings().ScenePath;
		for (char& c : path)
		{
			if (c == '\\')
			{
				c = '/';
			}
			c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
		}
		return path.find("lobby") != std::string::npos || path.find("menu") != std::string::npos;
	}

	static bool AreScenePathsMatching(const std::string& pathA, const std::string& pathB)
	{
		if (pathA.empty() || pathB.empty() || pathA == pathB)
		{
			return true;
		}

		auto normalize = [](std::string s) {
			for (char& c : s)
			{
				if (c == '\\')
				{
					c = '/';
				}
				c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
			}
			return s;
		};

		std::string normA = normalize(pathA);
		std::string normB = normalize(pathB);
		if (normA == normB)
		{
			return true;
		}

		if (normA.ends_with(normB) || normB.ends_with(normA))
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

	void NetworkSystem::ProcessEntitySpawnMessage(EntitySpawnMessage* msg, Scene* scene)
	{
		if (!msg || !scene)
		{
			return;
		}

		if (msg->NetworkID == 0 || msg->PrefabPath[0] == '\0')
		{
			return;
		}

		if (m_NetworkIDToEntity.find(msg->NetworkID) != m_NetworkIDToEntity.end())
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
					m_NetworkIDToEntity[msg->NetworkID] = reg.get<IDComponent>(entity).ID;
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
		uint64_t localNetID = m_LocalNetworkID;
		if (localNetID == 0 && net)
		{
			localNetID = net->GetLocalNetworkID();
			if (localNetID != 0)
			{
				m_LocalNetworkID = localNetID;
			}
		}

		if (localNetID != 0 && msg->NetworkID == localNetID)
		{
			if (avatar.HasComponent<TransformComponent>())
			{
				auto& transform = avatar.GetComponent<TransformComponent>();
				TransformSystem::SetTranslation(transform, FindSpawnPosition(scene->GetRegistry()));
			}
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

		// Mark remote entities as network-driven so physics doesn't fight interpolation.
		if (!netID.IsOwner)
		{
			if (avatar.HasComponent<RigidBodyComponent>())
			{
				avatar.GetComponent<RigidBodyComponent>().IsNetworkDriven = true;
			}
		}

		m_NetworkIDToEntity[msg->NetworkID] = avatar.GetUUID();
		CH_CORE_INFO("Network: Spawned replicated entity (netID={}, owner={}).", msg->NetworkID, netID.IsOwner);
	}

	void NetworkSystem::ProcessEntityDestroyMessage(EntityDestroyMessage* msg, Scene* scene)
	{
		if (!msg || !scene)
		{
			return;
		}

		auto it = m_NetworkIDToEntity.find(msg->NetworkID);
		if (it != m_NetworkIDToEntity.end())
		{
			Entity entity = scene->GetEntityByUUID(it->second);
			if (entity && entity.IsValid())
			{
				scene->DestroyEntity(entity);
			}
			m_NetworkIDToEntity.erase(it);
		}

		// Also search registry directly by NetworkIdentityComponent and destroy all matching entities
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

	void NetworkSystem::ProcessPlayerAssignMessage(PlayerAssignMessage* msg)
	{
		if (!msg)
		{
			return;
		}

		m_LocalNetworkID = msg->NetworkID;

		if (auto* net = ServiceLocator::TryGet<Network>())
		{
			net->SetLocalNetworkID(msg->NetworkID);
		}

		if (auto* scene = m_ReplicationScene)
		{
			entt::registry& reg = scene->GetRegistry();
			auto view = reg.view<NetworkIdentityComponent>();
			for (auto entity : view)
			{
				auto& netID = view.get<NetworkIdentityComponent>(entity);
				if (netID.NetworkID == msg->NetworkID)
				{
					netID.IsOwner = true;
					if (auto* rb = reg.try_get<RigidBodyComponent>(entity))
					{
						rb->IsNetworkDriven = false;
					}
					if (auto* tc = reg.try_get<TransformComponent>(entity))
					{
						glm::vec3 curPos = TransformSystem::GetTranslation(*tc);
						if (curPos == glm::vec3(0.0f))
						{
							TransformSystem::SetTranslation(*tc, FindSpawnPosition(reg));
						}
					}
					if (reg.all_of<IDComponent>(entity))
					{
						m_NetworkIDToEntity[msg->NetworkID] = reg.get<IDComponent>(entity).ID;
					}
				}
			}
		}

		CH_CORE_INFO("Network: Assigned local network ID {}.", msg->NetworkID);
	}

	// ---- Incoming message processing (host side) ----

	void NetworkSystem::ProcessInputStateMessage(InputStateMessage* msg, int clientIndex)
	{
		if (!msg)
		{
			return;
		}

		auto it = m_PeerToNetworkID.find(clientIndex);
		uint64_t networkID = (it != m_PeerToNetworkID.end()) ? it->second : 0;
		if (networkID == 0)
		{
			if (auto* net = ServiceLocator::TryGet<Network>())
			{
				networkID = net->GetNetworkIDForConnection(clientIndex);
				if (networkID != 0)
				{
					m_PeerToNetworkID[clientIndex] = networkID;
				}
			}
		}

		ProcessedInput input;
		input.NetworkID = networkID;
		input.MoveX = msg->MoveX;
		input.MoveZ = msg->MoveZ;
		input.ActionFlags = msg->ActionFlags;
		input.MouseX = msg->MouseX;
		input.MouseY = msg->MouseY;

		m_PendingInputs.push_back(input);
		if (networkID != 0)
		{
			m_ActiveClientInputs[networkID] = input;
			m_ActiveClientInputTimers[networkID] = 0.0f;
		}
	}

	void NetworkSystem::ProcessPlayerInfoMessage(PlayerInfoMessage* msg, int clientIndex)
	{
		if (!msg)
		{
			return;
		}

		auto* net = ServiceLocator::TryGet<Network>();
		if (!net)
		{
			return;
		}

		const uint64_t networkID = net->GetNetworkIDForConnection(clientIndex);
		if (networkID == 0)
		{
			CH_CORE_WARN("Network: PlayerInfo from client {} arrived before PlayerAssign — deferring.", clientIndex);
			m_PendingPlayerInfo[clientIndex] = {std::string(msg->Name), msg->SkinIndex};
			return;
		}

		net->UpdatePlayerInfo(networkID, msg->Name, msg->SkinIndex);
		net->BroadcastPlayerList();
	}

	void NetworkSystem::ProcessPlayerListMessage(PlayerListMessage* msg)
	{
		if (!msg)
		{
			return;
		}

		auto* net = ServiceLocator::TryGet<Network>();
		if (!net)
		{
			return;
		}

		std::vector<PlayerNetInfo> playerList;
		for (int i = 0; i < msg->Count && i < 64; ++i)
		{
			PlayerNetInfo info;
			info.NetworkID = msg->Entries[i].NetworkID;
			info.Name = msg->Entries[i].Name;
			info.SkinIndex = msg->Entries[i].SkinIndex;
			info.IsHost = msg->Entries[i].IsHost;
			info.Ping = 0;

			playerList.push_back(info);
		}

		net->SetPlayerListFromMessage(playerList);

		CH_CORE_INFO("Network: Received player list ({} players).", msg->Count);
	}

	void NetworkSystem::ProcessChatMessageMessage(ChatMessageMessage* msg)
	{
		if (!msg)
		{
			return;
		}

		ChatMessagePacket pkt;
		pkt.SenderNetworkID = msg->SenderNetworkID;
		pkt.SenderName = msg->SenderName;
		pkt.Message = msg->Message;

		if (auto* net = ServiceLocator::TryGet<Network>())
		{
			net->StorePendingChatMessage(pkt);
		}
		CH_CORE_INFO("Network: Chat from '{}': {}", pkt.SenderName, pkt.Message);
	}

	// ---- Host-side: apply inputs from remote clients ----

	void NetworkSystem::ApplyHostInputs(entt::registry& reg, Timestep ts)
	{
		m_PendingInputs.clear();
		if (m_ActiveClientInputs.empty())
		{
			return;
		}

		float dt = static_cast<float>(ts);

		for (auto it = m_ActiveClientInputs.begin(); it != m_ActiveClientInputs.end();)
		{
			uint64_t networkID = it->first;
			auto& input = it->second;
			float& timer = m_ActiveClientInputTimers[networkID];
			timer += dt;

			// If no input received for more than 250ms, drop it
			if (timer > 0.25f)
			{
				it = m_ActiveClientInputs.erase(it);
				m_ActiveClientInputTimers.erase(networkID);
				continue;
			}

			entt::entity targetEntity = entt::null;
			auto view = reg.view<NetworkIdentityComponent>();
			for (auto entity : view)
			{
				auto& netID = view.get<NetworkIdentityComponent>(entity);
				if (netID.NetworkID == networkID)
				{
					targetEntity = entity;
					break;
				}
			}

			if (targetEntity == entt::null || !reg.valid(targetEntity))
			{
				++it;
				continue;
			}

			if (reg.all_of<PlayerComponent, RigidBodyComponent>(targetEntity))
			{
				auto& player = reg.get<PlayerComponent>(targetEntity);
				auto& rb = reg.get<RigidBodyComponent>(targetEntity);

				if (rb.Handle != kInvalidPhysicsBody)
				{
					float speed = player.MovementSpeed;
					if (input.ActionFlags & InputAction_Sprint)
					{
						speed *= 2.0f;
					}

					float moveX = input.MoveX * speed;
					float moveZ = input.MoveZ * speed;
					rb.Velocity = glm::vec3(moveX, rb.Velocity.y, moveZ);

					// Rotate remote avatar to face movement direction
					if (auto* tc = reg.try_get<TransformComponent>(targetEntity))
					{
						if (std::abs(moveX) > 0.001f || std::abs(moveZ) > 0.001f)
						{
							float yaw = std::atan2(moveX, moveZ);
							TransformSystem::SetRotation(*tc, glm::vec3(0.0f, yaw, 0.0f));
						}
					}

					if ((input.ActionFlags & InputAction_Jump) && rb.IsGrounded)
					{
						rb.Velocity.y = player.JumpForce;
						rb.VelocityForced = true;
						input.ActionFlags &= ~InputAction_Jump;
					}

					m_LastActionFlags[networkID] = input.ActionFlags;
				}
			}
			else
			{
				if (m_WarnedInputNetID.insert(networkID).second)
				{
					CH_CORE_WARN(
						"Network: ApplyHostInputs — entity netID={} missing PlayerComponent/RigidBodyComponent",
						networkID);
				}
			}

			++it;
		}
	}

	// ---- Client-side: collect local input and send to server ----

	void NetworkSystem::CollectAndSendInput(Network* net, float dt)
	{
		if (!net->IsClient())
		{
			return;
		}

		InputStateMessage msg;
		msg.Tick = m_ClientTick++;
		msg.DeltaTime = dt;

		float rawX = 0.0f;
		float rawZ = 0.0f;
		if (Core::Input::IsKeyDown(KeyCode::W))
		{
			rawZ += 1.0f;
		}
		if (Core::Input::IsKeyDown(KeyCode::S))
		{
			rawZ -= 1.0f;
		}
		if (Core::Input::IsKeyDown(KeyCode::A))
		{
			rawX -= 1.0f;
		}
		if (Core::Input::IsKeyDown(KeyCode::D))
		{
			rawX += 1.0f;
		}

		float moveX = rawX;
		float moveZ = rawZ;
		if (m_ReplicationScene)
		{
			auto camView = m_ReplicationScene->GetRegistry().view<CameraComponent, TransformComponent>();
			for (auto entity : camView)
			{
				auto& cam = camView.get<CameraComponent>(entity);
				if (!cam.Primary)
				{
					continue;
				}
				auto& tc = camView.get<TransformComponent>(entity);
				glm::vec3 forward = tc.WorldTransform * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
				forward.y = 0.0f;
				float fwdLen = glm::length(forward);
				if (fwdLen > 0.001f)
				{
					forward /= fwdLen;
				}
				else
				{
					forward = glm::vec3(0.0f, 0.0f, -1.0f);
				}
				glm::vec3 right = glm::vec3(-forward.z, 0.0f, forward.x);

				moveX = rawX * right.x + rawZ * forward.x;
				moveZ = rawX * right.z + rawZ * forward.z;
				break;
			}
		}

		float len = std::sqrt(moveX * moveX + moveZ * moveZ);
		if (len > 0.001f)
		{
			moveX /= len;
			moveZ /= len;
		}

		msg.MoveX = moveX;
		msg.MoveZ = moveZ;

		uint8_t flags = 0;
		if (Core::Input::IsKeyPressed(KeyCode::Space))
		{
			flags |= InputAction_Jump;
		}
		if (Core::Input::IsKeyDown(KeyCode::LeftShift))
		{
			flags |= InputAction_Sprint;
		}
		msg.ActionFlags = flags;

		glm::vec2 mouseDelta = Core::Input::GetMouseDelta();
		msg.MouseX = mouseDelta.x;
		msg.MouseY = mouseDelta.y;

		ByteWriter w;
		msg.Encode(w);
		net->SendToServer(MessageType_InputState, w.Data().data(), w.Data().size(), false);
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

	void NetworkSystem::InterpolateEntities(entt::registry& reg, float dt)
	{
		constexpr float InterpSpeed = 25.0f;
		float t = glm::clamp(1.0f - std::exp(-InterpSpeed * dt), 0.0f, 1.0f);

		auto view = reg.view<NetworkIdentityComponent, TransformComponent>();
		for (auto entity : view)
		{
			auto& netID = view.get<NetworkIdentityComponent>(entity);
			auto& transform = view.get<TransformComponent>(entity);

			auto it = m_PendingStates.find(netID.NetworkID);
			if (it == m_PendingStates.end())
			{
				// No pending state: for non-owned entities, keep NetworkDriven so
				// physics doesn't interfere; for owned, let physics run.
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
				// Owned entity — physics runs locally, network only snap-corrects.
				if (auto* rb = reg.try_get<RigidBodyComponent>(entity))
				{
					rb->IsNetworkDriven = false;
				}

				float dist = glm::length(TransformSystem::GetTranslation(transform) - target.TargetPosition);

				if (dist > NetworkSystem::kMaxCorrectionDistance)
				{
					TransformSystem::SetTranslation(transform, target.TargetPosition);
					TransformSystem::SetRotationQuat(transform, SafeNormalizeQuat(target.TargetRotation));

					if (auto* rb = reg.try_get<RigidBodyComponent>(entity))
					{
						rb->Velocity = target.TargetVelocity;
					}
					changed = true;
				}
			}
			else
			{
				// Remote entity — fully driven by network interpolation.
				if (auto* rb = reg.try_get<RigidBodyComponent>(entity))
				{
					rb->IsNetworkDriven = true;
				}

				glm::vec3 currentPos = TransformSystem::GetTranslation(transform);
				float dist = glm::length(currentPos - target.TargetPosition);
				if (dist > NetworkSystem::kMaxCorrectionDistance)
				{
					TransformSystem::SetTranslation(transform, target.TargetPosition);
				}
				else
				{
					TransformSystem::SetTranslation(transform, glm::mix(currentPos, target.TargetPosition, t));
				}

				// Use the local RotationQuat for slerp (not WorldTransform which may be stale).
				glm::quat currentQuat = SafeNormalizeQuat(transform.RotationQuat);
				glm::quat targetQuat = SafeNormalizeQuat(target.TargetRotation);
				glm::quat blended = glm::slerp(currentQuat, targetQuat, t);
				TransformSystem::SetRotationQuat(transform, blended);

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

	// ---- Host-side: broadcast world state ----

	void NetworkSystem::BroadcastWorldState(entt::registry& reg)
	{
		auto* net = ServiceLocator::TryGet<Network>();
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
			auto flagIt = m_LastActionFlags.find(netID.NetworkID);
			s.ActionFlags = (flagIt != m_LastActionFlags.end()) ? flagIt->second : 0;
			states.push_back(s);
		}

		if (states.empty() || net->GetClientCount() == 0)
		{
			return;
		}

		++m_HostTick;
		CH_CORE_TRACE("Network: Broadcasting WorldState for {} entities (tick={}).", states.size(), m_HostTick);

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

	// ---- Host-side: broadcast an entity spawn/destroy ----

	void NetworkSystem::SendEntitySpawn(Network* net, uint64_t networkID, const std::string& prefabPath,
										int clientIndex)
	{
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

	void NetworkSystem::SendEntityDestroy(Network* net, uint64_t networkID)
	{
		net->BroadcastPacket(MessageType_EntityDestroy, true, [networkID](ByteWriter& bw) {
			EntityDestroyMessage msg;
			msg.NetworkID = networkID;
			msg.Encode(bw);
		});
	}

	void NetworkSystem::ResyncClientEntities(int clientIndex, Scene* scene)
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net || !scene)
		{
			return;
		}

		// Ensure Host identity is populated in this scene before resyncing
		EnsureHostIdentity(scene);

		uint64_t clientNetID = 0;
		auto it = m_PeerToNetworkID.find(clientIndex);
		if (it != m_PeerToNetworkID.end())
		{
			clientNetID = it->second;
		}
		else
		{
			clientNetID = net->GetNetworkIDForConnection(clientIndex);
		}

		// BUG #1 fix: if networkID not yet assigned, defer so EntitySpawn never arrives before PlayerAssign
		if (clientNetID == 0)
		{
			CH_CORE_WARN("Network: ResyncClientEntities - no networkID for client {} yet, deferring.", clientIndex);
			m_DeferredSceneLoaded[clientIndex] = scene->GetSettings().ScenePath;
			return;
		}

		// Always send PlayerAssign FIRST, before any EntitySpawn
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
			std::string prefabPath = netID.PrefabPath.empty() ? m_PlayerPrefab : netID.PrefabPath;
			SendEntitySpawn(net, netID.NetworkID, prefabPath, clientIndex);
			count++;
		}

		CH_CORE_INFO("Network: Resynced {} entities for client {} (netID={}).", count, clientIndex, clientNetID);
		net->BroadcastPlayerList();
	}

	void NetworkSystem::EnsureHostIdentity(Scene* scene)
	{
		if (!scene || IsLobbyOrMenuScene(scene))
		{
			return;
		}

		entt::registry& reg = scene->GetRegistry();

		// Check if host avatar already exists
		auto owned = reg.view<NetworkIdentityComponent>();
		for (auto entity : owned)
		{
			if (owned.get<NetworkIdentityComponent>(entity).NetworkID == kHostNetworkID)
			{
				return;
			}
		}

		// Try to tag existing entity with PlayerComponent
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
			netID.PrefabPath = m_PlayerPrefab;

			if (reg.all_of<TransformComponent>(entity))
			{
				auto& transform = reg.get<TransformComponent>(entity);
				glm::vec3 spawnPos = FindSpawnPosition(reg);
				CH_CORE_INFO("Network: Tagged host player with netID={}, spawnPos=({:.1f}, {:.1f}, {:.1f}).",
							 kHostNetworkID, spawnPos.x, spawnPos.y, spawnPos.z);
				TransformSystem::SetTranslation(transform, spawnPos);
			}
			else
			{
				CH_CORE_INFO("Network: Tagged host player with netID={} (no TransformComponent).", kHostNetworkID);
			}
			return;
		}

		// No existing PlayerComponent entity — spawn from prefab
		if (m_PlayerPrefab.empty())
		{
			CH_CORE_WARN("Network: EnsureHostIdentity — no PlayerComponent in scene and m_PlayerPrefab is empty.");
			return;
		}

		std::string path = m_PlayerPrefab;
		if (auto* am = ServiceLocator::TryGet<AssetManager>())
		{
			path = am->ResolvePath(m_PlayerPrefab);
		}

		Entity hostAvatar = PrefabSerializer::Deserialize(scene, path);
		if (!hostAvatar)
		{
			CH_CORE_ERROR("Network: EnsureHostIdentity — failed to deserialize prefab '{}'.", m_PlayerPrefab);
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
		netID.PrefabPath = m_PlayerPrefab;

		bool hasRB = hostAvatar.HasComponent<RigidBodyComponent>();
		bool hasCollider = hostAvatar.HasComponent<ColliderComponent>();
		int rbType = hasRB ? (int)hostAvatar.GetComponent<RigidBodyComponent>().Type : -1;
		CH_CORE_INFO("Network: Dynamically spawned host avatar (netID={}, entity={}, rb={}, collider={}, rbType={})",
					 kHostNetworkID, (uint32_t)hostAvatar, hasRB, hasCollider, rbType);
	}

	// ---- Host-side: spawn/despawn an avatar per connected peer ----

	void NetworkSystem::SyncPeerAvatars(Scene* scene, Network* net)
	{
		if (!scene || IsLobbyOrMenuScene(scene))
		{
			return;
		}

		if (m_PlayerPrefab.empty())
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

			if (m_PeerToAvatar.find(clientIndex) != m_PeerToAvatar.end())
			{
				continue;
			}

			uint64_t networkID = net->GetNetworkIDForConnection(clientIndex);
			if (networkID == 0)
			{
				continue;
			}
			m_PeerToNetworkID[clientIndex] = networkID;

			// If host is running an active gameplay scene, synchronize it to the newly connected client
			const std::string& currentScenePath = scene->GetSettings().ScenePath;
			if (!currentScenePath.empty())
			{
				SceneChangeMessage sceneMsg;
				std::strncpy(sceneMsg.ScenePath, currentScenePath.c_str(), sizeof(sceneMsg.ScenePath) - 1);
				sceneMsg.ScenePath[sizeof(sceneMsg.ScenePath) - 1] = '\0';
				ByteWriter sw;
				sceneMsg.Encode(sw);
				net->SendPacket(clientIndex, MessageType_SceneChange, sw.Data().data(), sw.Data().size(), true);
				CH_CORE_INFO("Network: Sent active scene '{}' to newly connected client {}.", currentScenePath,
							 clientIndex);
			}

			std::string path = m_PlayerPrefab;
			if (auto* am = ServiceLocator::TryGet<AssetManager>())
			{
				path = am->ResolvePath(m_PlayerPrefab);
			}

			Entity avatar = PrefabSerializer::Deserialize(scene, path);
			if (!avatar)
			{
				CH_CORE_ERROR("Network: Failed to spawn player prefab '{}' for client {}.", m_PlayerPrefab,
							  clientIndex);
				m_PeerToAvatar[clientIndex] = UUID(0);
				continue;
			}

			if (avatar.HasComponent<TransformComponent>())
			{
				auto& transform = avatar.GetComponent<TransformComponent>();
				glm::vec3 spawnPos = FindSpawnPosition(scene->GetRegistry());
				spawnPos.x += static_cast<float>(networkID - 1) * 1.5f;
				TransformSystem::SetTranslation(transform, spawnPos);
			}

			auto& netID = avatar.AddOrReplaceComponent<NetworkIdentityComponent>();
			netID.NetworkID = networkID;
			netID.IsOwner = false;
			netID.PrefabPath = m_PlayerPrefab;

			m_PeerToAvatar[clientIndex] = avatar.GetUUID();
			CH_CORE_INFO("Network: Spawned avatar (netID={}) for client {}.", networkID, clientIndex);

			for (const auto& [existingClient, uuid] : m_PeerToAvatar)
			{
				if (existingClient == clientIndex || uuid == UUID(0))
				{
					continue;
				}
				auto netIdIt = m_PeerToNetworkID.find(existingClient);
				if (netIdIt != m_PeerToNetworkID.end())
				{
					SendEntitySpawn(net, netIdIt->second, m_PlayerPrefab, clientIndex);
				}
				SendEntitySpawn(net, networkID, m_PlayerPrefab, existingClient);
			}
			SendEntitySpawn(net, kHostNetworkID, m_PlayerPrefab, clientIndex);
			SendEntitySpawn(net, networkID, m_PlayerPrefab, clientIndex);
		}

		std::unordered_set<int> allKnownClients;
		for (const auto& [c, _] : m_PeerToAvatar)
		{
			allKnownClients.insert(c);
		}
		for (const auto& [c, _] : m_PeerToNetworkID)
		{
			allKnownClients.insert(c);
		}

		std::vector<int> disconnectedClients;
		for (int clientIndex : allKnownClients)
		{
			if (!net->IsClientConnected(clientIndex))
			{
				disconnectedClients.push_back(clientIndex);
			}
		}

		for (int clientIndex : disconnectedClients)
		{
			uint64_t netIDToDestroy = 0;
			auto idIt = m_PeerToNetworkID.find(clientIndex);
			if (idIt != m_PeerToNetworkID.end())
			{
				netIDToDestroy = idIt->second;
			}

			auto it = m_PeerToAvatar.find(clientIndex);
			if (it != m_PeerToAvatar.end())
			{
				if (it->second != UUID(0))
				{
					Entity avatar = scene->GetEntityByUUID(it->second);
					if (avatar && avatar.IsValid())
					{
						scene->DestroyEntity(avatar);
					}
				}
				m_PeerToAvatar.erase(it);
			}

			// Also clean up by NetworkIdentityComponent directly in the scene registry
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

			UnregisterPeer(clientIndex);
			CH_CORE_INFO("Network: Despawned avatar for client {} (netID={}).", clientIndex, netIDToDestroy);
		}
	}

	void NetworkSystem::InstallPacketCallback()
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net)
		{
			return;
		}
		// Re-install whenever the role has changed (including Offline→Host on each new session)
		if (m_CallbackRole == net->GetRole() && net->GetRole() != Role::Offline)
		{
			return;
		}

		m_CallbackRole = net->GetRole();
		net->SetPacketCallback([this, net](int clientIndex, MessageType type, const uint8_t* data, size_t len) {
			ByteReader r(data, len);

			if (net->GetRole() == Role::Host)
			{
				switch (type)
				{
				case MessageType_InputState: {
					InputStateMessage msg;
					if (msg.Decode(r))
					{
						ProcessInputStateMessage(&msg, clientIndex);
					}
					break;
				}
				case MessageType_PlayerInfo: {
					PlayerInfoMessage msg;
					if (msg.Decode(r))
					{
						ProcessPlayerInfoMessage(&msg, clientIndex);
					}
					break;
				}
				case MessageType_ChatMessage: {
					ChatMessageMessage msg;
					if (msg.Decode(r))
					{
						ProcessChatMessageMessage(&msg);
						if (net->IsHost())
						{
							net->BroadcastPacket(MessageType_ChatMessage, true,
												 [&msg](ByteWriter& bw) { msg.Encode(bw); });
						}
					}
					break;
				}
				case MessageType_SceneLoaded: {
					SceneLoadedMessage msg;
					if (msg.Decode(r))
					{
						std::string clientScene = msg.ScenePath;

						// When host is still transitioning (m_ReplicationScene == nullptr),
						// AreScenePathsMatching returns true for empty path — which causes
						// ResyncClientEntities to be called with nullptr and silently drop all spawns.
						// Always defer in this case so the resync fires after the host finishes loading.
						if (!m_ReplicationScene)
						{
							CH_CORE_INFO(
								"Network: Client {} loaded scene '{}' — host still transitioning, deferring resync.",
								clientIndex, clientScene);
							m_DeferredSceneLoaded[clientIndex] = clientScene;
						}
						else
						{
							std::string hostScene = m_ReplicationScene->GetSettings().ScenePath;
							if (AreScenePathsMatching(hostScene, clientScene))
							{
								CH_CORE_INFO("Network: Client {} loaded scene '{}' — resyncing entities.", clientIndex,
											 clientScene);
								ResyncClientEntities(clientIndex, m_ReplicationScene);
							}
							else
							{
								CH_CORE_INFO(
									"Network: Client {} loaded scene '{}', but host is on '{}' — deferring resync.",
									clientIndex, clientScene, hostScene);
								m_DeferredSceneLoaded[clientIndex] = clientScene;
							}
						}
					}
					break;
				}
				default:
					break;
				}
			}
			else if (net->GetRole() == Role::Client)
			{
				switch (type)
				{
				case MessageType_WorldState: {
					WorldStateMessage msg;
					if (msg.Decode(r))
					{
						ProcessWorldStateMessage(&msg);
					}
					break;
				}
				case MessageType_SceneChange: {
					SceneChangeMessage msg;
					if (msg.Decode(r))
					{
						ProcessSceneChangeMessage(&msg);
					}
					break;
				}
				case MessageType_PlayerList: {
					PlayerListMessage msg;
					if (msg.Decode(r))
					{
						ProcessPlayerListMessage(&msg);
					}
					break;
				}
				case MessageType_PlayerAssign: {
					PlayerAssignMessage msg;
					if (msg.Decode(r))
					{
						ProcessPlayerAssignMessage(&msg);
					}
					break;
				}
				case MessageType_EntitySpawn: {
					EntitySpawnMessage spawnMsg;
					if (spawnMsg.Decode(r))
					{
						if (m_ReplicationScene)
						{
							ProcessEntitySpawnMessage(&spawnMsg, m_ReplicationScene);
						}
						else
						{
							m_PendingEntitySpawns.push_back(spawnMsg);
						}
					}
					break;
				}
				case MessageType_EntityDestroy: {
					EntityDestroyMessage destroyMsg;
					if (destroyMsg.Decode(r))
					{
						ProcessEntityDestroyMessage(&destroyMsg, m_ReplicationScene);
					}
					break;
				}
				case MessageType_ChatMessage: {
					ChatMessageMessage msg;
					if (msg.Decode(r))
					{
						ProcessChatMessageMessage(&msg);
					}
					break;
				}
				case MessageType_Heartbeat: {
					// Heartbeat received from server — connection is alive.
					// Reset reconnect timer if we were in reconnect flow.
					if (net->IsClient())
					{
						net->SendToServer(MessageType_Heartbeat, nullptr, 0, false);
					}
					break;
				}
				default:
					break;
				}
			}
		});
	}

	// ---- Early identity setup (runs BEFORE scripts) ----

	void NetworkSystem::EnsureLocalIdentity(Scene* scene)
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net || net->GetRole() == Role::Offline || !scene)
		{
			return;
		}

		if (net->IsHost())
		{
			EnsureHostIdentity(scene);
		}
		else if (net->IsClient())
		{
			if (m_LocalNetworkID == 0)
			{
				m_LocalNetworkID = net->GetLocalNetworkID();
			}

			if (m_LocalNetworkID != 0)
			{
				entt::registry& reg = scene->GetRegistry();
				auto view = reg.view<NetworkIdentityComponent>();
				for (auto entity : view)
				{
					auto& netID = view.get<NetworkIdentityComponent>(entity);
					if (netID.NetworkID == m_LocalNetworkID)
					{
						if (!netID.IsOwner)
						{
							netID.IsOwner = true;
						}
						if (auto* rb = reg.try_get<RigidBodyComponent>(entity))
						{
							rb->IsNetworkDriven = false;
						}
					}
				}
			}
		}
	}

	// ---- Per-session reset ----

	void NetworkSystem::Reset()
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (net && net->IsClient() && net->GetLocalNetworkID() != 0)
		{
			m_LocalNetworkID = net->GetLocalNetworkID();
		}
		else
		{
			m_LocalNetworkID = 0;
		}

		m_ClientTick = 0;
		m_HostTick = 0;
		m_CallbackRole = Role::Offline;
		m_PeerToNetworkID.clear();
		m_PeerToAvatar.clear();
		m_PendingStates.clear();
		m_PendingInputs.clear();
		m_ActiveClientInputs.clear();
		m_ActiveClientInputTimers.clear();
		m_PendingPlayerInfo.clear();
		m_NetworkIDToEntity.clear();
		m_LastActionFlags.clear();
		m_WarnedInputNetID.clear();
		m_DeferredSceneLoaded.clear();
		m_PendingEntitySpawns.clear();
		m_ReplicationScene = nullptr;
		m_PrefabWarnedOnce = false;
		m_SceneLoadedPending = false;

		if (net)
		{
			net->ClearPendingSceneChange();
		}

		CH_CORE_INFO("NetworkSystem: session state reset (localNetID={}).", m_LocalNetworkID);
	}

	// ---- Main update ----

	void NetworkSystem::CheckAndPropagateSceneChange(Scene* scene)
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net || !scene)
		{
			return;
		}
		if (!net->HasPendingSceneChange())
		{
			return;
		}

		std::string path = net->GetPendingSceneChange();
		net->ClearPendingSceneChange();

		std::string currentPath = scene->GetSettings().ScenePath;
		std::filesystem::path currentP(currentPath);
		std::filesystem::path newP(path);

		// If client is already on this scene, do not reload
		if (!currentPath.empty() && (currentPath == path || currentP.filename() == newP.filename()))
		{
			CH_CORE_INFO("CheckAndPropagateSceneChange: already on scene '{}', skipping reload.", path);
			if (net->IsClient())
			{
				m_SceneLoadedPending = true;
			}
			return;
		}

		CH_CORE_INFO("CheckAndPropagateSceneChange: propagating '{}' to Application", path);
		SceneChangeRequestEvent e(path);
		Application::Get().OnEvent(e);
	}

	void NetworkSystem::PollNetwork(Scene* scene, Timestep ts)
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net || net->GetRole() == Role::Offline || !scene)
		{
			return;
		}

		entt::registry& reg = scene->GetRegistry();

		// Scene pointer changed → new Play Mode session, reset lookup tables
		if (m_ReplicationScene != scene)
		{
			m_ReplicationScene = scene;
			m_NetworkIDToEntity.clear();
			m_PeerToAvatar.clear();
			m_PendingStates.clear();
			if (net->IsClient())
			{
				m_SceneLoadedPending = true;

				// BUG #2 fix: flush EntitySpawn messages that arrived while scene was loading
				for (auto& spawn : m_PendingEntitySpawns)
				{
					ProcessEntitySpawnMessage(&spawn, m_ReplicationScene);
				}
				m_PendingEntitySpawns.clear();
			}
		}

		InstallPacketCallback();

		if (m_SceneLoadedPending && net->IsClient() && net->IsConnected())
		{
			m_SceneLoadedPending = false;
			SceneLoadedMessage msg;
			std::strncpy(msg.ScenePath, scene->GetSettings().ScenePath.c_str(), sizeof(msg.ScenePath) - 1);
			msg.ScenePath[sizeof(msg.ScenePath) - 1] = '\0';
			ByteWriter w;
			msg.Encode(w);
			net->SendToServer(MessageType_SceneLoaded, w.Data().data(), w.Data().size(), true);
			CH_CORE_INFO("Network: Sent SceneLoaded to host.");
		}

		EnsureLocalIdentity(scene); // BUG4 fix: must run before scripts

		if (net->IsHost())
		{
			// Process any deferred SceneLoaded messages that now match the host's scene
			std::string currentScene = scene->GetSettings().ScenePath;
			std::vector<int> readyDeferredScenes;
			for (const auto& [clientIndex, deferredScene] : m_DeferredSceneLoaded)
			{
				if (AreScenePathsMatching(deferredScene, currentScene))
				{
					readyDeferredScenes.push_back(clientIndex);
				}
			}
			for (int clientIndex : readyDeferredScenes)
			{
				CH_CORE_INFO("Network: Processing deferred SceneLoaded for client {} (scene='{}')", clientIndex,
							 currentScene);
				m_DeferredSceneLoaded.erase(clientIndex);
				ResyncClientEntities(clientIndex, scene);
			}

			// Flush any deferred PlayerInfoMessages (race: PlayerInfo arrived before PlayerAssign).
			std::vector<std::pair<int, std::pair<std::string, uint8_t>>> readyPlayerInfo;
			for (const auto& [clientIndex, info] : m_PendingPlayerInfo)
			{
				uint64_t netId = net->GetNetworkIDForConnection(clientIndex);
				if (netId != 0)
				{
					readyPlayerInfo.emplace_back(clientIndex, info);
				}
			}
			for (const auto& [clientIndex, info] : readyPlayerInfo)
			{
				m_PendingPlayerInfo.erase(clientIndex);
				PlayerInfoMessage deferredMsg;
				std::strncpy(deferredMsg.Name, info.first.c_str(), sizeof(deferredMsg.Name) - 1);
				deferredMsg.Name[sizeof(deferredMsg.Name) - 1] = '\0';
				deferredMsg.SkinIndex = info.second;
				ProcessPlayerInfoMessage(&deferredMsg, clientIndex);
			}

			EnsureHostIdentity(scene);
			SyncPeerAvatars(scene, net);
		}

		float dt = static_cast<float>(ts);

		net->Update(dt);

		// Check for pending scene change AFTER processing ENet packets.
		// This avoids the C# polling timing issue where OnUpdate runs before
		// net->Update processes incoming SceneChangeMessage packets.
		if (net->IsClient())
		{
			CheckAndPropagateSceneChange(scene);
		}
	}

	void NetworkSystem::FinalizeFrame(Scene* scene, Timestep ts)
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net || net->GetRole() == Role::Offline || !scene)
		{
			return;
		}

		m_NetworkTickAccumulator += static_cast<float>(ts);
		if (m_NetworkTickAccumulator >= kNetworkTickInterval)
		{
			if (m_NetworkTickAccumulator > kNetworkTickInterval * 4.0f)
			{
				m_NetworkTickAccumulator = kNetworkTickInterval;
			}
			m_NetworkTickAccumulator -= kNetworkTickInterval;

			entt::registry& reg = scene->GetRegistry();

			if (net->IsHost())
			{
				BroadcastWorldState(reg);
			}
			else if (net->IsClient())
			{
				float dt = static_cast<float>(ts);
				CollectAndSendInput(net, dt);
			}
		}
	}

	const std::vector<ProcessedInput>& NetworkSystem::GetPendingInputs()
	{
		return m_PendingInputs;
	}

	void NetworkSystem::ClearPendingInputs()
	{
		m_PendingInputs.clear();
	}

} // namespace Chained
