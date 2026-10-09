#include "engine/scene/systems/network_system.h"
#include "engine/core/application_event_proxy.h"
#include "engine/core/log.h"
#include "engine/core/service_locator.h"
#include "engine/project/project.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_events.h"
#include "engine/scene/components/core/transform_component.h"
#include "engine/scene/components/gameplay/network_identity_component.h"
#include "engine/scene/components/physics/physics_component.h"
#include "engine/scene/systems/transform_system.h"
#include <filesystem>
#include <cstring>

namespace Chained
{
	namespace NetworkSystem
	{

		static bool AreScenePathsMatching(const std::string& a, const std::string& b)
		{
			if (a.empty() && b.empty())
			{
				return true;
			}
			return std::filesystem::path(a).filename() == std::filesystem::path(b).filename();
		}

		SceneNetworkContext& GetOrCreateContext(Scene* scene)
		{
			CH_ASSERT(scene, "Scene cannot be null in NetworkSystem::GetOrCreateContext");
			entt::registry& reg = scene->GetRegistry();
			if (!reg.ctx().contains<SceneNetworkContext>())
			{
				return reg.ctx().emplace<SceneNetworkContext>();
			}
			return reg.ctx().get<SceneNetworkContext>();
		}

		SceneNetworkContext* TryGetContext(Scene* scene)
		{
			if (!scene)
			{
				return nullptr;
			}
			return scene->GetRegistry().ctx().find<SceneNetworkContext>();
		}

		NetworkSessionTracker* GetSessionTracker(Scene* scene)
		{
			auto* ctx = TryGetContext(scene);
			return ctx ? &ctx->Session : nullptr;
		}

		NetworkInputController* GetInputController(Scene* scene)
		{
			auto* ctx = TryGetContext(scene);
			return ctx ? &ctx->Input : nullptr;
		}

		NetworkReplicationManager* GetReplicationManager(Scene* scene)
		{
			auto* ctx = TryGetContext(scene);
			return ctx ? &ctx->Replication : nullptr;
		}

		void SetPlayerPrefab(Scene* scene, const std::string& path)
		{
			if (scene)
			{
				GetOrCreateContext(scene).Session.SetPlayerPrefab(path);
			}
		}

		const std::string& GetPlayerPrefab(Scene* scene)
		{
			static const std::string s_DefaultPrefab = "prefab/player.chprefab";
			if (scene)
			{
				if (auto* ctx = TryGetContext(scene))
				{
					return ctx->Session.GetPlayerPrefab();
				}
			}
			return s_DefaultPrefab;
		}

		void RegisterPeerEntity(Scene* scene, int peer, uint64_t networkID)
		{
			if (scene)
			{
				GetOrCreateContext(scene).Session.RegisterPeer(peer, networkID);
			}
		}

		void UnregisterPeer(Scene* scene, int peer)
		{
			if (scene)
			{
				if (auto* ctx = TryGetContext(scene))
				{
					ctx->Session.UnregisterPeer(peer);
				}
			}
		}

		const std::vector<ProcessedInput>& GetPendingInputs(Scene* scene)
		{
			static const std::vector<ProcessedInput> s_EmptyInputs;
			if (scene)
			{
				if (auto* ctx = TryGetContext(scene))
				{
					return ctx->Input.GetPendingInputs();
				}
			}
			return s_EmptyInputs;
		}

		void ClearPendingInputs(Scene* scene)
		{
			if (scene)
			{
				if (auto* ctx = TryGetContext(scene))
				{
					ctx->Input.ClearPendingInputs();
				}
			}
		}

		void ApplyHostInputs(entt::registry& reg, Timestep ts)
		{
			if (auto* ctx = reg.ctx().find<SceneNetworkContext>())
			{
				ctx->Input.ApplyHostInputs(reg, ts);
			}
		}

		void InterpolateEntities(entt::registry& reg, float dt)
		{
			if (auto* ctx = reg.ctx().find<SceneNetworkContext>())
			{
				ctx->Replication.InterpolateEntities(reg, dt);
			}
		}

		static void ProcessPlayerAssignMessage(PlayerAssignMessage* msg, Scene* scene, SceneNetworkContext& ctx,
											   Network* net = nullptr)
		{
			if (!msg || !scene)
			{
				return;
			}
			auto* netSvc = net ? net : ServiceLocator::TryGet<Network>();
			if (!netSvc)
			{
				return;
			}

			netSvc->SetLocalNetworkID(msg->NetworkID);
			ctx.Session.SetLocalNetworkID(msg->NetworkID);
			CH_CORE_INFO("Network: Received PlayerAssign — local network ID is {}.", msg->NetworkID);

			auto& reg = scene->GetRegistry();
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
					CH_CORE_INFO("Network: Claimed ownership of entity {} with network ID {}.", (uint32_t)entity,
								 msg->NetworkID);
				}
				else
				{
					netID.IsOwner = false;
					if (auto* rb = reg.try_get<RigidBodyComponent>(entity))
					{
						rb->IsNetworkDriven = true;
					}
				}
			}
		}

		static void ProcessPlayerInfoMessage(PlayerInfoMessage* msg, int clientIndex, Scene* scene,
											 SceneNetworkContext& ctx, Network* net = nullptr)
		{
			if (!msg || !scene)
			{
				return;
			}
			auto* netSvc = net ? net : ServiceLocator::TryGet<Network>();
			if (!netSvc)
			{
				return;
			}

			uint64_t networkID = netSvc->GetNetworkIDForConnection(clientIndex);
			if (networkID == 0)
			{
				CH_CORE_WARN("Network: Received PlayerInfo for client {} but network ID is 0. Deferring.", clientIndex);
				ctx.Session.AddPendingPlayerInfo(clientIndex, msg->Name, msg->SkinIndex);
				return;
			}

			netSvc->UpdatePlayerInfo(networkID, msg->Name, msg->SkinIndex);
			netSvc->BroadcastPlayerList();
			CH_CORE_INFO("Network: Updated player info for client {} (netID={}): name='{}', skin={}.", clientIndex,
						 networkID, msg->Name, msg->SkinIndex);
		}

		static void ProcessPlayerListMessage(PlayerListMessage* msg, Network* net = nullptr)
		{
			if (!msg)
			{
				return;
			}
			auto* netSvc = net ? net : ServiceLocator::TryGet<Network>();
			if (!netSvc)
			{
				return;
			}

			std::vector<PlayerNetInfo> playerList;
			for (uint32_t i = 0; i < msg->Count; ++i)
			{
				PlayerNetInfo info;
				info.NetworkID = msg->Entries[i].NetworkID;
				info.Name = msg->Entries[i].Name;
				info.SkinIndex = msg->Entries[i].SkinIndex;
				info.IsHost = msg->Entries[i].IsHost;
				info.Ping = msg->Entries[i].Ping;
				playerList.push_back(info);
			}

			netSvc->SetPlayerListFromMessage(playerList);
			CH_CORE_INFO("Network: Received PlayerList ({} players).", playerList.size());
		}

		static void ProcessChatMessageMessage(Chained::ChatMessageMessage* msg, Network* net = nullptr)
		{
			if (!msg)
			{
				return;
			}
			ChatMessagePacket pkt;
			pkt.SenderNetworkID = msg->SenderNetworkID;
			pkt.SenderName = msg->SenderName;
			pkt.Message = msg->Message;

			auto* netSvc = net ? net : ServiceLocator::TryGet<Network>();
			if (netSvc)
			{
				netSvc->StorePendingChatMessage(pkt);
			}
			CH_CORE_INFO("Network: Chat from '{}': {}", pkt.SenderName, pkt.Message);
		}

		static void InstallPacketCallback(Scene* scene, SceneNetworkContext& ctx, Network* net = nullptr)
		{
			Network* netSvc = net ? net : ServiceLocator::TryGet<Network>();
			if (!netSvc)
			{
				return;
			}

			if (ctx.CallbackRole == netSvc->GetRole() && netSvc->GetRole() != Role::Offline)
			{
				return;
			}

			ctx.CallbackRole = netSvc->GetRole();
			std::weak_ptr<Scene> weakScene = scene->weak_from_this();

			netSvc->SetPacketCallback([netSvc, weakScene](int clientIndex, MessageType type, const uint8_t* data,
														  size_t len) {
				ByteReader r(data, len);

				// Global messages that do not require an active scene
				if (type == MessageType_ChatMessage)
				{
					ChatMessageMessage msg;
					if (msg.Decode(r))
					{
						ProcessChatMessageMessage(&msg);
						if (netSvc->IsHost())
						{
							netSvc->BroadcastPacket(MessageType_ChatMessage, true,
													[&msg](ByteWriter& bw) { msg.Encode(bw); });
						}
					}
					return;
				}

				Scene* activeScene = weakScene.lock().get();

				if (type == MessageType_PlayerList)
				{
					PlayerListMessage msg;
					if (msg.Decode(r))
					{
						ProcessPlayerListMessage(&msg);
					}
					return;
				}

				if (type == MessageType_SceneChange)
				{
					SceneChangeMessage msg;
					if (msg.Decode(r))
					{
						std::string currentPath = activeScene ? activeScene->GetSettings().ScenePath : "";
						std::string newPath = msg.ScenePath;
						if (!AreScenePathsMatching(currentPath, newPath))
						{
							CH_CORE_INFO("Network: Received SceneChange message to '{}'", newPath);
							netSvc->SetPendingSceneChange(newPath);
						}
					}
					return;
				}

				if (type == MessageType_PlayerAssign)
				{
					PlayerAssignMessage msg;
					if (msg.Decode(r))
					{
						netSvc->SetLocalNetworkID(msg.NetworkID);
						if (activeScene)
						{
							if (auto* sceneCtx = TryGetContext(activeScene))
							{
								ProcessPlayerAssignMessage(&msg, activeScene, *sceneCtx, netSvc);
							}
						}
					}
					return;
				}

				if (type == MessageType_EntitySpawn)
				{
					EntitySpawnMessage msg;
					if (msg.Decode(r))
					{
						if (activeScene)
						{
							if (auto* sceneCtx = TryGetContext(activeScene))
							{
								sceneCtx->Replication.ProcessEntitySpawnMessage(&msg, activeScene, sceneCtx->Session);
								return;
							}
						}
						// If scene is transitioning/loading, queue the spawn in the context of the active scene
						if (activeScene)
						{
							if (auto* sceneCtx = TryGetContext(activeScene))
							{
								sceneCtx->PendingSpawns.push_back(msg);
								CH_CORE_INFO(
									"Network: Queued EntitySpawn for netID={} while scene is transitioning/loading.",
									msg.NetworkID);
							}
						}
					}
					return;
				}

				if (type == MessageType_EntityDestroy)
				{
					EntityDestroyMessage msg;
					if (msg.Decode(r))
					{
						if (activeScene)
						{
							if (auto* sceneCtx = TryGetContext(activeScene))
							{
								sceneCtx->Replication.ProcessEntityDestroyMessage(&msg, activeScene, sceneCtx->Session);
							}
						}
					}
					return;
				}

				if (!activeScene)
				{
					return;
				}

				auto* sceneCtx = TryGetContext(activeScene);
				if (!sceneCtx)
				{
					return;
				}

				if (netSvc->GetRole() == Role::Host)
				{
					switch (type)
					{
					case MessageType_InputState: {
						InputStateMessage msg;
						if (msg.Decode(r))
						{
							uint64_t netID = sceneCtx->Session.GetNetworkIDForPeer(clientIndex);
							if (netID == 0)
							{
								netID = netSvc->GetNetworkIDForConnection(clientIndex);
							}
							sceneCtx->Input.ProcessInputStateMessage(&msg, netID);
						}
						break;
					}
					case MessageType_PlayerInfo: {
						PlayerInfoMessage msg;
						if (msg.Decode(r))
						{
							ProcessPlayerInfoMessage(&msg, clientIndex, activeScene, *sceneCtx, netSvc);
						}
						break;
					}
					case MessageType_SceneLoaded: {
						SceneLoadedMessage msg;
						if (msg.Decode(r))
						{
							std::string clientScene = msg.ScenePath;
							auto& deferredMap = sceneCtx->Replication.GetDeferredSceneLoaded();

							std::string hostScene = activeScene->GetSettings().ScenePath;
							if (AreScenePathsMatching(hostScene, clientScene))
							{
								CH_CORE_INFO(
									"Network: Client {} confirmed scene '{}' loaded matching host '{}', sync avatars.",
									clientIndex, clientScene, hostScene);
								sceneCtx->Replication.ResyncClientEntities(clientIndex, activeScene, netSvc,
																		   sceneCtx->Session);
							}
							else
							{
								CH_CORE_INFO("Network: Client {} loaded scene '{}' != host '{}', deferring sync.",
											 clientIndex, clientScene, hostScene);
								deferredMap[clientIndex] = clientScene;
							}
						}
						break;
					}
					default:
						break;
					}
				}
				else if (netSvc->GetRole() == Role::Client)
				{
					switch (type)
					{
					case MessageType_WorldState: {
						WorldStateMessage msg;
						if (msg.Decode(r))
						{
							sceneCtx->Replication.ProcessWorldStateMessage(&msg);
						}
						break;
					}
					default:
						break;
					}
				}
			});
		}

		void EnsureLocalIdentity(Scene* scene, Network* net)
		{
			Network* netSvc = net ? net : ServiceLocator::TryGet<Network>();
			if (!netSvc || netSvc->GetRole() == Role::Offline || !scene)
			{
				return;
			}

			auto& ctx = GetOrCreateContext(scene);
			if (netSvc->IsHost())
			{
				ctx.Replication.EnsureHostIdentity(scene, ctx.Session);
			}
			else if (netSvc->IsClient())
			{
				uint64_t localNetID = ctx.Session.GetLocalNetworkID();
				if (localNetID == 0)
				{
					localNetID = netSvc->GetLocalNetworkID();
					if (localNetID != 0)
					{
						ctx.Session.SetLocalNetworkID(localNetID);
					}
				}

				if (localNetID != 0)
				{
					entt::registry& reg = scene->GetRegistry();
					auto view = reg.view<NetworkIdentityComponent>();
					for (auto entity : view)
					{
						auto& netID = view.get<NetworkIdentityComponent>(entity);
						if (netID.NetworkID == localNetID)
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

		void Reset(Scene* scene, Network* net)
		{
			Network* netSvc = net ? net : ServiceLocator::TryGet<Network>();
			if (scene)
			{
				if (auto* ctx = TryGetContext(scene))
				{
					ctx->Session.Reset();
					ctx->Input.Reset();
					ctx->Replication.Reset();
					if (netSvc && netSvc->IsClient() && netSvc->GetLocalNetworkID() != 0)
					{
						ctx->Session.SetLocalNetworkID(netSvc->GetLocalNetworkID());
					}
					ctx->CallbackRole = Role::Offline;
					ctx->SceneLoadedPending = false;
					ctx->NetworkTickAccumulator = 0.0f;
				}
			}

			if (netSvc && scene == nullptr)
			{
				netSvc->ClearPendingSceneChange();
			}

			CH_CORE_INFO("NetworkSystem: session state reset.");
		}

		void CheckAndPropagateSceneChange(Scene* scene, Network* net)
		{
			Network* netSvc = net ? net : ServiceLocator::TryGet<Network>();
			if (!netSvc || !scene)
			{
				return;
			}
			if (!netSvc->HasPendingSceneChange())
			{
				return;
			}

			std::string path = netSvc->GetPendingSceneChange();
			netSvc->ClearPendingSceneChange();

			std::string currentPath = scene->GetSettings().ScenePath;
			std::filesystem::path currentP(currentPath);
			std::filesystem::path newP(path);

			if (!currentPath.empty() && (currentPath == path || currentP.filename() == newP.filename()))
			{
				CH_CORE_INFO("CheckAndPropagateSceneChange: already on scene '{}', skipping reload.", path);
				if (netSvc->IsClient())
				{
					GetOrCreateContext(scene).SceneLoadedPending = true;
				}
				return;
			}

			CH_CORE_INFO("CheckAndPropagateSceneChange: propagating '{}' to Application", path);
			SceneChangeRequestEvent e(path);
			ApplicationEventProxy::Dispatch(e);
		}

		void PollNetwork(Scene* scene, Timestep ts, Network* net)
		{
			Network* netSvc = net ? net : ServiceLocator::TryGet<Network>();
			if (!netSvc || netSvc->GetRole() == Role::Offline || !scene)
			{
				return;
			}

			auto& ctx = GetOrCreateContext(scene);

			if (ctx.IsFirstPollForScene)
			{
				ctx.IsFirstPollForScene = false;
				ctx.Replication.ClearPendingStates();
				if (netSvc->IsClient())
				{
					ctx.SceneLoadedPending = true;
					for (auto& spawnMsg : ctx.PendingSpawns)
					{
						ctx.Replication.ProcessEntitySpawnMessage(&spawnMsg, scene, ctx.Session);
					}
					ctx.PendingSpawns.clear();
					ctx.Replication.FlushPendingSpawns(scene, ctx.Session);
				}
			}

			InstallPacketCallback(scene, ctx, netSvc);

			if (ctx.SceneLoadedPending && netSvc->IsClient() && netSvc->IsConnected())
			{
				ctx.SceneLoadedPending = false;
				std::string relScenePath = scene->GetSettings().ScenePath;
				if (Project::GetActive())
				{
					relScenePath = Project::GetActive()->GetRelativePath(relScenePath);
				}
				SceneLoadedMessage msg;
				std::strncpy(msg.ScenePath, relScenePath.c_str(), sizeof(msg.ScenePath) - 1);
				msg.ScenePath[sizeof(msg.ScenePath) - 1] = '\0';
				ByteWriter w;
				msg.Encode(w);
				netSvc->SendToServer(MessageType_SceneLoaded, w.Data().data(), w.Data().size(), true);
				CH_CORE_INFO("Network: Sent SceneLoaded ('{}') to host.", relScenePath);
			}

			EnsureLocalIdentity(scene, netSvc);

			if (netSvc->IsHost())
			{
				std::string currentScene = scene->GetSettings().ScenePath;
				std::vector<int> readyDeferredScenes;
				auto& deferredMap = ctx.Replication.GetDeferredSceneLoaded();
				for (const auto& [clientIndex, deferredScene] : deferredMap)
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
					deferredMap.erase(clientIndex);
					ctx.Replication.ResyncClientEntities(clientIndex, scene, netSvc, ctx.Session);
				}

				std::vector<std::pair<int, std::pair<std::string, uint8_t>>> readyPlayerInfo;
				for (const auto& [clientIndex, info] : ctx.Session.GetPendingPlayerInfo())
				{
					uint64_t netId = netSvc->GetNetworkIDForConnection(clientIndex);
					if (netId != 0)
					{
						readyPlayerInfo.emplace_back(clientIndex, info);
					}
				}
				for (const auto& [clientIndex, info] : readyPlayerInfo)
				{
					ctx.Session.RemovePendingPlayerInfo(clientIndex);
					PlayerInfoMessage deferredMsg;
					std::strncpy(deferredMsg.Name, info.first.c_str(), sizeof(deferredMsg.Name) - 1);
					deferredMsg.Name[sizeof(deferredMsg.Name) - 1] = '\0';
					deferredMsg.SkinIndex = info.second;
					ProcessPlayerInfoMessage(&deferredMsg, clientIndex, scene, ctx);
				}

				ctx.Replication.EnsureHostIdentity(scene, ctx.Session);
				ctx.Replication.SyncPeerAvatars(scene, netSvc, ctx.Session);
			}

			float dt = static_cast<float>(ts);
			netSvc->Update(dt);

			if (netSvc->IsClient())
			{
				CheckAndPropagateSceneChange(scene, netSvc);
			}
		}

		void FinalizeFrame(Scene* scene, Timestep ts, Network* net, Core::Input* input)
		{
			Network* netSvc = net ? net : ServiceLocator::TryGet<Network>();
			if (!netSvc || netSvc->GetRole() == Role::Offline || !scene)
			{
				return;
			}

			auto& ctx = GetOrCreateContext(scene);

			ctx.NetworkTickAccumulator += static_cast<float>(ts);
			if (ctx.NetworkTickAccumulator >= NetworkConstants::kNetworkTickInterval)
			{
				if (ctx.NetworkTickAccumulator > NetworkConstants::kNetworkTickInterval * 4.0f)
				{
					ctx.NetworkTickAccumulator = NetworkConstants::kNetworkTickInterval;
				}
				ctx.NetworkTickAccumulator -= NetworkConstants::kNetworkTickInterval;

				entt::registry& reg = scene->GetRegistry();

				if (netSvc->IsHost())
				{
					std::unordered_map<uint64_t, uint8_t> actionFlagsMap;
					ctx.Replication.BroadcastWorldState(reg, netSvc, actionFlagsMap);
				}
				else if (netSvc->IsClient())
				{
					float dt = static_cast<float>(ts);
					ctx.Input.CollectAndSendInput(netSvc, dt, scene, input);
				}
			}
		}
	} // namespace NetworkSystem

} // namespace Chained
