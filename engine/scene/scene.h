#ifndef CH_SCENE_H
#define CH_SCENE_H

#include <entt/entt.hpp>
#include <memory>
#include <string>
#include <vector>

#include "engine/core/events/events.h"
#include "engine/common/base.h"
#include "engine/common/timestep.h"
#include "engine/scene/entity.h"
#include "engine/scene/scene_settings.h"
#include "engine/scene/scene_state.h"

namespace Chained
{
	enum WidgetType : int;
}

namespace Chained
{
	class SceneScriptingManager;
	class Event;

	class CH_API Scene
	{
	public:
		Scene();
		~Scene();

	public:
		static std::shared_ptr<Scene> CreateDefault();
		static std::shared_ptr<Scene> Copy(std::shared_ptr<Scene> other);

		virtual void OnEvent(Event& e);
		virtual void OnRenderUI();

	public: // Scene State Management
		void TransitionToState(SceneState newState);
		SceneState GetSceneState() const
		{
			return m_State;
		}

		void OnUpdate(Timestep timestep);
		void OnViewportResize(uint32_t width, uint32_t height);

	public:
	public: // Entity Management
		Entity CreateEntity(const std::string& name = std::string());
		Entity CreateEntityWithUUID(UUID uuid, const std::string& name = std::string());
		Entity CreateUIEntity(WidgetType type, const std::string& name = std::string());

	public:
		entt::entity CopyEntity(entt::entity copyEntity);
		void DestroyEntity(Entity entity);
		Entity FindEntityByTag(const std::string& tag);
		Entity GetEntityByUUID(UUID uuid);

	public:
		SceneSettings& GetSettings()
		{
			return m_Settings;
		}
		const SceneSettings& GetSettings() const
		{
			return m_Settings;
		}
		void SetSettings(const SceneSettings& settings)
		{
			m_Settings = settings;
		}
		bool IsSimulationRunning() const;
		const std::vector<entt::entity>& GetRootEntities() const;
		bool IsStartingUp() const
		{
			return m_IsStartingUp;
		}

		// Validation setters — prefer these over direct GetSettings() mutation
		void SetSceneName(const std::string& name)
		{
			m_Settings.Name = name;
		}
		void SetScenePath(const std::string& path)
		{
			m_Settings.ScenePath = path;
		}
		void SetEnvironment(std::shared_ptr<EnvironmentAsset> env)
		{
			m_Settings.Environment = std::move(env);
		}
		void SetBackgroundMode(BackgroundMode mode)
		{
			m_Settings.Mode = mode;
		}
		void SetBackgroundColor(const Color& color)
		{
			m_Settings.BackgroundColor = color;
		}
		void SetDebugFlags(const DebugRenderFlags& flags)
		{
			m_Settings.DebugFlags = flags;
		}
		void SetDiagnosticMode(float mode)
		{
			m_Settings.DiagnosticMode = std::clamp(mode, 0.0f, 2.0f);
		}
		void SetGridSlices(int slices)
		{
			m_Settings.Grid.Slices = std::max(1, slices);
		}
		void SetGridSpacing(float spacing)
		{
			m_Settings.Grid.Spacing = std::max(0.01f, spacing);
		}

	public: // Systems & Tools
		/// Returns the underlying EnTT registry for direct access.
		/// @deprecated Prefer typed methods below or the Entity API for component access.
		entt::registry& GetRegistry();
		const entt::registry& GetRegistry() const;
		entt::registry* GetRegistryPtr();

		// Registry Facade — prefer these over raw GetRegistry() access
		Entity GetEntity(entt::entity handle);
		const Entity GetEntity(entt::entity handle) const;

		template <typename... Components> auto View()
		{
			return m_Registry->view<Components...>();
		}

		template <typename... Components> auto View() const
		{
			return m_Registry->view<Components...>();
		}

		bool IsValid(entt::entity handle) const
		{
			return m_Registry && m_Registry->valid(handle);
		}

		template <typename T> T& GetContext()
		{
			return m_Registry->ctx().get<T>();
		}

		template <typename T> const T& GetContext() const
		{
			return m_Registry->ctx().get<T>();
		}

		template <typename T> T* TryGetContext()
		{
			return m_Registry->ctx().find<T>();
		}

		template <typename T> const T* TryGetContext() const
		{
			return m_Registry->ctx().find<T>();
		}

		void OnRuntimeStop();
		void OnRuntimeStart();
		void OnUpdateSimulation(Timestep timestep);
		void OnUpdateEditor(Timestep timestep);
		void OnUpdateRuntime(Timestep timestep);

		void SwapScene(std::shared_ptr<Scene> newScene);

		// EnTT Dispatcher for Zero-Allocation Publish/Subscribe
		entt::dispatcher& GetDispatcher()
		{
			return m_Dispatcher;
		}
		const entt::dispatcher& GetDispatcher() const
		{
			return m_Dispatcher;
		}

	private:
		void TickCommonSystems(Timestep ts);
		void InitializePhysicsStartup();
		void RebuildRootCache() const;

	private:
		void OnStateEnter(SceneState state);
		void OnStateExit(SceneState state);
		void FinishRuntimeStart();

	private:
		SceneState m_State = SceneState::Edit;

		std::unique_ptr<entt::registry> m_Registry;
		entt::dispatcher m_Dispatcher;
		SceneSettings m_Settings;

		std::unique_ptr<SceneScriptingManager> m_ScriptingManager;

		bool m_IsStartingUp = false;
		bool m_PhysicsStartupInitialized = false;

		mutable std::vector<entt::entity> m_CachedRoots;
		mutable bool m_RootsDirty = true;

	private:
		void OnIDConstruct(entt::registry& registry, entt::entity entity);
		void OnIDDestroy(entt::registry& registry, entt::entity entity);
		void OnHierarchyConstruct();
		void OnHierarchyDestroy(entt::registry& registry, entt::entity entity);
		Entity CopyEntityInternal(entt::entity copyEntity, entt::entity parentEntity = entt::null);
	};

} // namespace Chained

#endif // CH_SCENE_H
