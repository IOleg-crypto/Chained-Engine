#ifndef CH_EDITOR_SCENE_MANAGER_H
#define CH_EDITOR_SCENE_MANAGER_H

#include "engine/scene/scene.h"
#include "engine/scene/scene_events.h"
#include "editor/project/editor_settings.h"
#include <filesystem>
#include <future>
#include <memory>
#include <string>

namespace Chained
{

	class EditorSceneManager
	{
	public:
		EditorSceneManager() = default;
		~EditorSceneManager() = default;

		void NewScene();
		void OpenScene();
		void OpenScene(const std::filesystem::path& path);
		void OpenSceneInPlayMode(const std::filesystem::path& path);
		void SaveScene();
		void SaveSceneAs();
		void AutoSave(float interval, float ts);

		void SetScene(const std::shared_ptr<Scene>& scene);
		void SetSceneState(SceneState state);
		SceneState GetSceneState() const;
		std::shared_ptr<Scene> GetActiveScene() const;
		std::shared_ptr<Scene> GetRuntimeScene() const
		{
			return m_RuntimeScene;
		}
		std::shared_ptr<Scene> GetEditorScene() const
		{
			return m_EditorScene;
		}

		bool IsSceneDirty() const
		{
			return m_SceneDirty;
		}
		void MarkSceneDirty()
		{
			m_SceneDirty = true;
		}
		void ClearSceneDirty()
		{
			m_SceneDirty = false;
		}

		bool IsConfirmPending() const
		{
			return m_PendingNewScene || m_PendingOpenScene;
		}
		void ConfirmPendingAction();
		void CancelPendingAction();

		void OnUpdate(Timestep ts);
		void OnViewportResize(uint32_t width, uint32_t height);

		bool IsLoading() const
		{
			return m_Transition.state != TransitionState::None;
		}
		bool IsTransitioning() const
		{
			return IsLoading();
		}
		const std::string& GetLoadingStatus() const
		{
			return m_LoadingStatus;
		}

		bool OnSceneOpened(SceneOpenedEvent& e);

	private:
		enum class TransitionState
		{
			None,
			PlayStarting,
			SceneLoading,
			Finalizing
		};

		struct SceneLoadResult
		{
			std::shared_ptr<Scene> scene;
			std::string error;
		};

		struct TransitionData
		{
			TransitionState state = TransitionState::None;
			SceneState targetState = SceneState::Edit;
			bool forPlayMode = false;

			std::future<SceneLoadResult> future;
			std::filesystem::path targetPath;
			bool sceneReady = false;
		};

		void StartSceneLoad(const std::filesystem::path& path, bool forPlayMode);
		void UpdateSceneLoading();
		void UpdateFinalizing();
		void FinalizeTransition();
		void CancelTransition();

		std::shared_ptr<Scene> m_EditorScene;
		std::shared_ptr<Scene> m_RuntimeScene;

		TransitionData m_Transition;
		std::string m_LoadingStatus;
		float m_AutoSaveTimer = 0.0f;
		float m_AssetWaitLogTimer = 0.0f;

		bool m_SceneDirty = false;

		bool m_PendingNewScene = false;
		bool m_PendingOpenScene = false;
		std::filesystem::path m_PendingOpenPath;
	};

} // namespace Chained

#endif // CH_EDITOR_SCENE_MANAGER_H
