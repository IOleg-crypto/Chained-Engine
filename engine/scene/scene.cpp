#include "engine/scene/scene.h"
#include "engine/core/profiler.h"
#include "engine/core/service_locator.h"
#include "engine/graphics/pipeline/renderer.h"
#include "engine/ui/ui_data_components.h"
#include "engine/ui/widget_renderer.h"
#include "engine/physics/physics.h"
#include "engine/scene/components/animation/animation_component.h"
#include "engine/scene/components/core/hierarchy_component.h"
#include "engine/scene/components/ui/scene_transition_component.h"
#include "engine/scene/scene_events.h"
#include "engine/scene/systems/hierarchy_system.h"
#include "engine/scene/systems/animation_system.h"
#include "engine/scene/systems/physics_body_system.h"
#include "engine/scene/systems/asset_resolution_system.h"
#include "engine/scene/systems/audio_system.h"
#include "engine/scene/systems/scene_transition_system.h"
#include "engine/scene/systems/camera_auto_select_system.h"
#include "engine/scene/systems/network_system.h"
#include "engine/scene/systems/transform_system.h"
#include "engine/scene/component_serializer.h"
#include "scene_scripting_manager.h"
#include "engine/scripting/scriptengine.h"
#include "engine/scene/prefab_serializer.h"
#include "engine/app/application.h"

namespace Chained
{

	Scene::Scene()
	{
		m_Registry = std::make_unique<entt::registry>();
		auto& reg = *m_Registry;

		reg.ctx().emplace<Scene*>(this);
		reg.ctx().emplace<EntityUUIDMap>();
		reg.ctx().emplace<PhysicsBodySystem::WarnState>();

		reg.on_construct<IDComponent>().connect<&Scene::OnIDConstruct>(this);
		reg.on_destroy<IDComponent>().connect<&Scene::OnIDDestroy>(this);

		reg.on_construct<HierarchyComponent>().connect<&Scene::OnHierarchyConstruct>(this);
		reg.on_destroy<HierarchyComponent>().connect<&Scene::OnHierarchyDestroy>(this);

		AssetResolutionSystem::RegisterObservers(reg);

		m_Settings.Environment = std::make_shared<EnvironmentAsset>();
		m_ScriptingManager = std::make_unique<SceneScriptingManager>(this);
	}

	Scene::~Scene()
	{
		auto& reg = *m_Registry;
		reg.on_destroy<IDComponent>().disconnect();
		reg.on_destroy<HierarchyComponent>().disconnect();
		reg.on_construct<IDComponent>().disconnect();
		reg.on_construct<HierarchyComponent>().disconnect();
		reg.ctx().erase<Scene*>();
		GetRegistry().clear();
	}

	std::shared_ptr<Scene> Scene::CreateDefault()
	{
		auto scene = std::make_shared<Scene>();

		Entity camera = scene->CreateEntity("Main Camera");
		auto& cameraEntity = camera.AddComponent<CameraComponent>();
		cameraEntity.Primary = true;
		TransformSystem::SetTranslation(camera.GetComponent<TransformComponent>(), {0, 5, 10});

		return scene;
	}

	std::shared_ptr<Scene> Scene::Copy(std::shared_ptr<Scene> other)
	{
		CH_PROFILE_FUNCTION();
		CH_CORE_INFO("Scene::Copy - Starting copy of scene '{}'", other->m_Settings.Name);

		std::shared_ptr<Scene> newScene = std::make_shared<Scene>();
		newScene->m_Settings = other->m_Settings;

		auto& srcRegistry = other->GetRegistry();
		auto& dstRegistry = newScene->GetRegistry();

		std::unordered_map<entt::entity, entt::entity> entityMap;

		{
			CH_PROFILE_SCOPE("Scene::Copy::CopyEntities_Pass1");
			for (auto entityHandle : srcRegistry.view<IDComponent>())
			{
				auto& id = srcRegistry.get<IDComponent>(entityHandle);
				Entity srcEntity = {entityHandle, other->m_Registry.get()};
				Entity dstEntity = newScene->CreateEntityWithUUID(id.ID);
				entityMap[entityHandle] = (entt::entity)dstEntity;

				ComponentSerializer::CopyAll(srcEntity, dstEntity);

				if (dstEntity.HasComponent<RigidBodyComponent>())
				{
					dstEntity.GetComponent<RigidBodyComponent>().Handle = kInvalidPhysicsBody;
				}
			}
		}

		{
			CH_PROFILE_SCOPE("Scene::Copy::CopyEntities_Pass2");
			srcRegistry.view<HierarchyComponent>().each([&](auto entityHandle, auto& srcHC) {
				if (!entityMap.contains(entityHandle))
				{
					return;
				}

				entt::entity dstHandle = entityMap[entityHandle];
				auto& dstHC = dstRegistry.get_or_emplace<HierarchyComponent>(dstHandle);

				if (srcHC.Parent != entt::null && entityMap.count(srcHC.Parent))
				{
					dstHC.Parent = entityMap[srcHC.Parent];
				}
				else
				{
					dstHC.Parent = entt::null;
				}

				dstHC.Children.clear();
				for (auto child : srcHC.Children)
				{
					if (entityMap.count(child))
					{
						dstHC.Children.push_back(entityMap[child]);
					}
				}
			});
		}

		newScene->m_RootsDirty = true;
		return newScene;
	}

	void Scene::OnHierarchyConstruct()
	{
		m_RootsDirty = true;
	}

	void Scene::OnHierarchyDestroy(entt::registry& reg, entt::entity entity)
	{
		m_RootsDirty = true;

		if (!reg.valid(entity) || !reg.all_of<HierarchyComponent>(entity))
		{
			return;
		}

		auto& hc = reg.get<HierarchyComponent>(entity);

		if (hc.Parent != entt::null && reg.valid(hc.Parent) && reg.all_of<HierarchyComponent>(hc.Parent))
		{
			auto& phc = reg.get<HierarchyComponent>(hc.Parent);
			auto it = std::find(phc.Children.begin(), phc.Children.end(), entity);
			if (it != phc.Children.end())
			{
				phc.Children.erase(it);
			}
		}
	}

	void Scene::OnEvent(Event& e)
	{
		if (m_State == SceneState::Play && !m_IsStartingUp)
		{
			m_ScriptingManager->OnEvent(e);
		}
	}

	void Scene::OnRenderUI()
	{
		if (m_State == SceneState::Play && !m_IsStartingUp)
		{
			m_ScriptingManager->OnRenderUI();
		}
	}

	void Scene::OnRuntimeStart()
	{
		CH_CORE_INFO("Scene::OnRuntimeStart - Starting activation for state: {}", (int)m_State);
		m_IsStartingUp = true;
		m_PhysicsStartupInitialized = false;
	}

	void Scene::FinishRuntimeStart()
	{
		if (auto* scripting = ServiceLocator::TryGet<ScriptEngine>())
		{
			scripting->SetContextScene(this);
		}

		if (m_State == SceneState::Play)
		{
			if (m_ScriptingManager)
			{
				m_ScriptingManager->OnRuntimeStart();
			}
			else
			{
				CH_CORE_ERROR("Scene::OnRuntimeStart - m_ScriptingManager is NULL!");
			}
		}

		AudioSystem::OnRuntimeStart(*m_Registry);

		CameraAutoSelectSystem::OnRuntimeStart(*m_Registry);

		auto transitionView = m_Registry->view<SceneTransitionComponent>();
		for (auto entity : transitionView)
		{
			transitionView.get<SceneTransitionComponent>(entity).Triggered = false;
		}

		auto view = m_Registry->view<AnimationComponent>();
		for (auto entity : view)
		{
			auto& anim = view.get<AnimationComponent>(entity);
			if (anim.PlayOnStart)
			{
				anim.IsPlaying = true;
			}
		}
	}

	void Scene::OnRuntimeStop()
	{
		CH_CORE_INFO("Scene::OnRuntimeStop - Stopping lifecycle...");

		m_IsStartingUp = false;
		m_PhysicsStartupInitialized = false;

		if (m_ScriptingManager)
		{
			m_ScriptingManager->OnRuntimeStop();
		}

		AudioSystem::OnRuntimeStop(*m_Registry);

		if (auto* netSys = ServiceLocator::TryGet<NetworkSystem>())
		{
			netSys->Reset();
		}

		if (auto* physics = ServiceLocator::TryGet<Physics>())
		{
			physics->ClearContext(this);
		}

		if (auto* scripting = ServiceLocator::TryGet<ScriptEngine>())
		{
			scripting->SetContextScene(nullptr);
		}
	}

	void Scene::TickCommonSystems(Timestep ts)
	{
		Hierarchy::UpdateWorldTransforms(*m_Registry, GetRootEntities());
		AssetResolutionSystem::Update(*m_Registry);
		AnimationSystem::Update(*m_Registry, ts);
		AudioSystem::Update(*m_Registry);
		m_Dispatcher.update();
	}

	void Scene::OnUpdateRuntime(Timestep ts)
	{
		CH_PROFILE_FUNCTION();

		if (m_IsStartingUp)
		{
			InitializePhysicsStartup();
			return;
		}

		auto* netSys = ServiceLocator::TryGet<NetworkSystem>();
		if (netSys)
		{
			netSys->PollNetwork(this, ts);
		}

		// Interpolate remote entities BEFORE scripts so PlayerController
		// reads the correct velocity/grounded values for animation.
		if (auto* net = ServiceLocator::TryGet<Network>())
		{
			if (net->IsClient() && netSys)
			{
				float dt = static_cast<float>(ts);
				netSys->InterpolateEntities(*m_Registry, dt);
			}
		}

		if (m_ScriptingManager)
		{
			m_ScriptingManager->OnUpdate(ts);
		}

		TickCommonSystems(ts);

		PhysicsBodySystem::Update(*m_Registry);
		if (netSys)
		{
			netSys->ApplyHostInputs(*m_Registry, ts);
		}

		if (auto* physics = ServiceLocator::TryGet<Physics>())
		{
			physics->Update(this, ts, true);
		}

		Hierarchy::UpdateWorldTransforms(*m_Registry, GetRootEntities());
		if (netSys)
		{
			netSys->FinalizeFrame(this, ts);
		}

		if (auto target = SceneTransitionSystem::Update(*m_Registry))
		{
			SceneChangeRequestEvent ev(*target);
			Application::Get().OnEvent(ev);
		}
	}

	void Scene::OnUpdateSimulation(Timestep ts)
	{
		CH_PROFILE_FUNCTION();

		if (m_IsStartingUp)
		{
			InitializePhysicsStartup();
			return;
		}

		TickCommonSystems(ts);

		PhysicsBodySystem::Update(*m_Registry);

		if (auto* physics = ServiceLocator::TryGet<Physics>())
		{
			physics->Update(this, ts, true);
		}
	}

	void Scene::InitializePhysicsStartup()
	{
		if (auto* physics = ServiceLocator::TryGet<Physics>())
		{
			if (!m_PhysicsStartupInitialized)
			{
				physics->ResetWorld(this);
				physics->ResetAccumulator(this);
				if (!m_Registry->ctx().contains<Physics*>())
				{
					m_Registry->ctx().emplace<Physics*>(physics);
				}
				m_PhysicsStartupInitialized = true;
			}

			PhysicsBodySystem::Update(*m_Registry);

			if (!PhysicsBodySystem::IsStartupComplete(*m_Registry, physics->GetWorld()))
			{
				return;
			}
		}
		m_IsStartingUp = false;
		FinishRuntimeStart();
	}

	void Scene::OnUpdateEditor(Timestep timestep)
	{
		CH_PROFILE_FUNCTION();

		CameraAutoSelectSystem::OnSceneUpdate(*m_Registry);

		if (auto* physics = ServiceLocator::TryGet<Physics>())
		{
			physics->Update(this, timestep, false);
		}

		TickCommonSystems(timestep);
	}

	void Scene::OnViewportResize(uint32_t width, uint32_t height)
	{
		auto& reg = GetRegistry();
		auto view = reg.view<CameraComponent>();
		for (auto entity : view)
		{
			auto& cameraComponent = view.get<CameraComponent>(entity);
			if (!cameraComponent.FixedAspectRatio)
			{
				cameraComponent.Camera.SetViewportSize(width, height);
			}
		}
	}

	void Scene::OnIDConstruct(entt::registry& reg, entt::entity entity)
	{
		auto& id = reg.get<IDComponent>(entity);
		auto& mapStruct = reg.ctx().get<EntityUUIDMap>();
		mapStruct.Map[id.ID] = entity;
	}

	void Scene::OnIDDestroy(entt::registry& reg, entt::entity entity)
	{
		auto& id = reg.get<IDComponent>(entity);
		auto& mapStruct = reg.ctx().get<EntityUUIDMap>();
		mapStruct.Map.erase(id.ID);
	}

	entt::registry* Scene::GetRegistryPtr()
	{
		return m_Registry.get();
	}

	const entt::registry& Scene::GetRegistry() const
	{
		return *m_Registry;
	}

	entt::registry& Scene::GetRegistry()
	{
		return *m_Registry;
	}

	const std::vector<entt::entity>& Scene::GetRootEntities() const
	{
		if (m_RootsDirty)
		{
			RebuildRootCache();
		}
		return m_CachedRoots;
	}

	void Scene::RebuildRootCache() const
	{
		m_CachedRoots.clear();
		auto& reg = GetRegistry();

		auto view = reg.view<TransformComponent>();
		for (auto entity : view)
		{
			if (reg.all_of<HierarchyComponent>(entity))
			{
				auto& hc = reg.get<HierarchyComponent>(entity);
				if (hc.Parent == entt::null)
				{
					m_CachedRoots.push_back(entity);
				}
			}
			else
			{
				m_CachedRoots.push_back(entity);
			}
		}
		m_RootsDirty = false;
	}

	bool Scene::IsSimulationRunning() const
	{
		return m_State == SceneState::Play || m_State == SceneState::Simulate;
	}

	void Scene::DestroyEntity(Entity entity)
	{
		entity.Destroy();
	}

	entt::entity Scene::CopyEntity(entt::entity copyEntity)
	{
		return (entt::entity)CopyEntityInternal(copyEntity, entt::null);
	}

	Entity Scene::CopyEntityInternal(entt::entity copyEntity, entt::entity parentEntity)
	{
		Entity srcEntity(copyEntity, m_Registry.get());
		std::string copyName = srcEntity.GetName() + (parentEntity == entt::null ? "_copy" : "");
		Entity dstEntity = CreateEntity(copyName);

		ComponentSerializer::CopyAll(srcEntity, dstEntity);
		dstEntity.GetComponent<TagComponent>().Tag = copyName;

		dstEntity.RemoveComponent<IDComponent>();
		dstEntity.AddComponent<IDComponent>();

		if (dstEntity.HasComponent<RigidBodyComponent>())
		{
			dstEntity.GetComponent<RigidBodyComponent>().Handle = kInvalidPhysicsBody;
		}

		if (srcEntity.HasComponent<HierarchyComponent>())
		{
			auto srcHC = srcEntity.GetComponent<HierarchyComponent>();

			entt::entity targetParent = parentEntity;
			if (targetParent == entt::null && srcHC.Parent != entt::null)
			{
				targetParent = srcHC.Parent;
			}

			if (targetParent != entt::null)
			{
				auto& dstHC = dstEntity.AddOrReplaceComponent<HierarchyComponent>();
				dstHC.Parent = targetParent;
				dstHC.Children.clear();

				Entity parent(targetParent, m_Registry.get());
				if (parent.HasComponent<HierarchyComponent>())
				{
					parent.GetComponent<HierarchyComponent>().Children.push_back(dstEntity);
				}
			}
			else
			{
				if (dstEntity.HasComponent<HierarchyComponent>())
				{
					dstEntity.GetComponent<HierarchyComponent>().Parent = entt::null;
					dstEntity.GetComponent<HierarchyComponent>().Children.clear();
				}
			}

			for (auto child : srcHC.Children)
			{
				CopyEntityInternal(child, dstEntity);
			}
		}

		return dstEntity;
	}

	Entity Scene::CreateUIEntity(WidgetType type, const std::string& name)
	{
		Entity entity = CreateEntity(name.empty() ? WidgetTypeName(type) : name);
		entity.AddComponent<ControlComponent>();
		auto& comp = entity.AddComponent<UIControlComponent>();
		comp.Data = CreateDefaultWidgetData(type);
		return entity;
	}

	Entity Scene::CreateEntityWithUUID(UUID uuid, const std::string& name)
	{
		Entity entity(m_Registry->create(), m_Registry.get());
		entity.AddComponent<IDComponent>(uuid);
		entity.AddComponent<TagComponent>(name.empty() ? "Entity" : name);
		entity.AddComponent<TransformComponent>();
		m_RootsDirty = true;
		return entity;
	}

	Entity Scene::CreateEntity(const std::string& name)
	{
		Entity entity(m_Registry->create(), m_Registry.get());
		entity.AddComponent<IDComponent>();
		entity.AddComponent<TagComponent>(name.empty() ? "Entity" : name);
		entity.AddComponent<TransformComponent>();
		m_RootsDirty = true;
		return entity;
	}

	Entity Scene::FindEntityByTag(const std::string& tag)
	{
		auto view = m_Registry->view<TagComponent>();
		for (auto entity : view)
		{
			const auto& tagComp = view.get<TagComponent>(entity);
			if (tagComp.Tag == tag)
			{
				return {entity, m_Registry.get()};
			}
		}
		return {};
	}

	Entity Scene::GetEntityByUUID(UUID uuid)
	{
		auto* mapStruct = m_Registry->ctx().find<EntityUUIDMap>();
		if (mapStruct && mapStruct->Map.find(uuid) != mapStruct->Map.end())
		{
			return {mapStruct->Map.at(uuid), m_Registry.get()};
		}
		return {};
	}

	void Scene::TransitionToState(SceneState newState)
	{
		if (m_State == newState)
		{
			return;
		}

		OnStateExit(m_State);
		SceneState oldState = m_State;
		m_State = newState;
		OnStateEnter(m_State);

		CH_CORE_TRACE("Scene: Transitioned state from {} to {}", (int)oldState, (int)newState);
	}

	void Scene::OnStateEnter(SceneState state)
	{
		switch (state)
		{
		case SceneState::Play:
		case SceneState::Simulate:
			OnRuntimeStart();
			break;
		case SceneState::Edit:
			break;
		}
	}

	void Scene::OnStateExit(SceneState state)
	{
		switch (state)
		{
		case SceneState::Play:
		case SceneState::Simulate:
			OnRuntimeStop();
			break;
		case SceneState::Edit:
			break;
		}
	}

	void Scene::OnUpdate(Timestep timestep)
	{
		switch (m_State)
		{
		case SceneState::Edit:
			OnUpdateEditor(timestep);
			break;
		case SceneState::Play:
			OnUpdateRuntime(timestep);
			break;
		case SceneState::Simulate:
			OnUpdateSimulation(timestep);
			break;
		}
	}

	void Scene::SwapScene(std::shared_ptr<Scene> newScene)
	{
		if (!newScene)
		{
			CH_CORE_ERROR("Scene::SwapScene: newScene is null");
			return;
		}

		// Swap ownership explicitly instead of stealing fields from another scene object.
		// This keeps lifecycle ownership clear and avoids leaving a partially-moved scene behind.
		std::swap(m_Registry, newScene->m_Registry);
		std::swap(m_ScriptingManager, newScene->m_ScriptingManager);
		std::swap(m_Settings, newScene->m_Settings);
		std::swap(m_State, newScene->m_State);
		std::swap(m_IsStartingUp, newScene->m_IsStartingUp);
		std::swap(m_PhysicsStartupInitialized, newScene->m_PhysicsStartupInitialized);
		std::swap(m_CachedRoots, newScene->m_CachedRoots);
		std::swap(m_RootsDirty, newScene->m_RootsDirty);

		CH_CORE_INFO("Scene: Swapped to '{}'", m_Settings.Name);
	}

} // namespace Chained
