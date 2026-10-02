#include "scene_manager.h"
#include "engine/audio/audio.h"
#include "engine/assets/asset_manager.h"
#include "engine/common/thread_pool.h"
#include "engine/core/service_locator.h"
#include "engine/ui/widget_renderer.h"
#include "engine/platform/dialogs/dialogs.h"
#include "engine/project/project.h"
#include "engine/scene/scene.h"
#include "engine/scene/scene_events.h"
#include "engine/scene/scene_serializer.h"
#include "layer.h"

namespace Chained
{

	void EditorSceneManager::NewScene()
	{
		auto& cfg = EditorLayer::Get().GetConfig();
		if (cfg.ConfirmOnSceneClose && m_SceneDirty)
		{
			m_PendingNewScene = true;
			return;
		}
		SetScene(Scene::CreateDefault());
	}

	void EditorSceneManager::OpenScene()
	{
		std::vector<DialogFilter> filters = {{"Chained Scene", "chscene"}};
		auto result = Chained::Dialogs::OpenFile(filters);
		if (result)
		{
			OpenScene(*result);
		}
	}

	void EditorSceneManager::OpenScene(const std::filesystem::path& path)
	{
		auto& cfg = EditorLayer::Get().GetConfig();
		if (cfg.ConfirmOnSceneClose && m_SceneDirty)
		{
			m_PendingOpenScene = true;
			m_PendingOpenPath = path;
			return;
		}

		if (m_Transition.state != TransitionState::None)
		{
			CH_CORE_WARN("EditorSceneManager: Transition to '{}' ignored - already in progress", path.string());
			return;
		}

		bool forPlayMode = GetSceneState() == SceneState::Play;
		StartSceneLoad(path, forPlayMode);
	}

	void EditorSceneManager::SaveScene()
	{
		auto scene = GetActiveScene();
		if (!scene)
		{
			return;
		}

		if (scene->GetSettings().ScenePath.empty())
		{
			SaveSceneAs();
			return;
		}

		SceneSerializer serializer(scene.get());
		serializer.Serialize(scene->GetSettings().ScenePath);
		m_SceneDirty = false;
		CH_INFO("Scene saved to {0}", scene->GetSettings().ScenePath);
	}

	void EditorSceneManager::SaveSceneAs()
	{
		std::vector<DialogFilter> filters = {{"Chained Scene", "chscene"}};
		auto result = Chained::Dialogs::SaveFile(filters);
		if (result)
		{
			auto scene = GetActiveScene();
			if (!scene)
			{
				return;
			}

			if (result->extension().empty())
			{
				result->replace_extension(".chscene");
			}

			scene->SetScenePath(result->string());
			SceneSerializer serializer(scene.get());
			serializer.Serialize(result->string());
		}
	}

	void EditorSceneManager::AutoSave(float interval, float ts)
	{
		auto scene = GetActiveScene();
		if (!scene || scene->GetSettings().ScenePath.empty())
		{
			return;
		}

		m_AutoSaveTimer += ts;
		if (m_AutoSaveTimer < interval)
		{
			return;
		}

		m_AutoSaveTimer = 0.0f;
		SceneSerializer serializer(scene.get());
		serializer.Serialize(scene->GetSettings().ScenePath);
		CH_TRACE("Scene auto-saved to {0}", scene->GetSettings().ScenePath);
	}

	void EditorSceneManager::SetScene(const std::shared_ptr<Scene>& scene)
	{
		CancelTransition();

		if (auto* audio = ServiceLocator::TryGet<Audio>())
		{
			audio->StopAll();
		}

		EditorLayer::Get().GetEditorState().SelectedEntity = {};

		m_EditorScene = scene;
		if (m_EditorScene)
		{
			m_EditorScene->TransitionToState(SceneState::Edit);
		}
	}

	SceneState EditorSceneManager::GetSceneState() const
	{
		auto activeScene = GetActiveScene();
		return activeScene ? activeScene->GetSceneState() : SceneState::Edit;
	}

	std::shared_ptr<Scene> EditorSceneManager::GetActiveScene() const
	{
		if (m_RuntimeScene)
		{
			return m_RuntimeScene;
		}
		return m_EditorScene;
	}

	void EditorSceneManager::SetSceneState(SceneState state)
	{
		SceneState current = GetSceneState();

		if (state == SceneState::Play || state == SceneState::Simulate)
		{
			if (m_Transition.state != TransitionState::None || current == SceneState::Play ||
				current == SceneState::Simulate)
			{
				return;
			}

			if (!m_EditorScene)
			{
				CH_CORE_WARN("EditorSceneManager::SetSceneState - No editor scene available.");
				return;
			}

			CH_CORE_INFO("Editor: {} mode requested.", state == SceneState::Play ? "Play" : "Simulate");

			CancelTransition();

			m_Transition = {};
			m_Transition.state = TransitionState::PlayStarting;
			m_Transition.targetState = state;
			m_Transition.forPlayMode = true;
			m_Transition.targetPath = m_EditorScene->GetSettings().ScenePath;
			m_LoadingStatus = (state == SceneState::Play) ? "Preparing Play Mode..." : "Preparing Simulation...";

			auto editorScene = m_EditorScene;
			if (auto* tp = ServiceLocator::TryGet<ThreadPool>())
			{
				m_Transition.future = tp->Enqueue([editorScene]() -> SceneLoadResult {
					try
					{
						auto runtimeScene = Scene::Copy(editorScene);
						if (!runtimeScene)
						{
							return SceneLoadResult{nullptr, "Failed to copy scene for play mode."};
						}
						return SceneLoadResult{runtimeScene, {}};
					} catch (const std::exception& e)
					{
						return SceneLoadResult{nullptr, e.what()};
					}
				});
			}
			else
			{
				try
				{
					auto runtimeScene = Scene::Copy(editorScene);
					m_RuntimeScene = runtimeScene;
					m_Transition.state = TransitionState::Finalizing;
					m_Transition.sceneReady = true;
				} catch (const std::exception& e)
				{
					CH_CORE_ERROR("Editor: Exception copying scene: {}", e.what());
					CancelTransition();
				}
			}
		}
		else
		{
			if (m_Transition.state != TransitionState::None)
			{
				CancelTransition();
			}

			if (current == SceneState::Edit)
			{
				return;
			}

			// Map SelectedEntity back to the editor scene
			auto& editorState = EditorLayer::Get().GetEditorState();
			if (editorState.SelectedEntity)
			{
				UUID uuid = editorState.SelectedEntity.GetUUID();
				Entity editorEntity = m_EditorScene ? m_EditorScene->GetEntityByUUID(uuid) : Entity{};
				editorState.SelectedEntity = editorEntity;
			}

			CH_CORE_INFO("Editor: Play Mode Stopped");
			if (auto* audio = ServiceLocator::TryGet<Audio>())
			{
				audio->StopAll();
			}

			if (m_RuntimeScene)
			{
				CH_CORE_INFO("Editor: Cleaning up runtime scene...");
				m_RuntimeScene->OnRuntimeStop();
				m_RuntimeScene.reset();
			}

			if (m_EditorScene)
			{
				m_EditorScene->TransitionToState(SceneState::Edit);
			}
		}
	}

	void EditorSceneManager::OnUpdate(Timestep ts)
	{
		switch (m_Transition.state)
		{
		case TransitionState::None:
			break;
		case TransitionState::PlayStarting:
		case TransitionState::SceneLoading:
			UpdateSceneLoading();
			break;
		case TransitionState::Finalizing:
			UpdateFinalizing();
			break;
		}
	}

	void EditorSceneManager::OnViewportResize(uint32_t width, uint32_t height)
	{
		if (m_EditorScene)
		{
			m_EditorScene->OnViewportResize(width, height);
		}

		if (m_RuntimeScene)
		{
			m_RuntimeScene->OnViewportResize(width, height);
		}
	}

	// --- Transition helpers ---

	void EditorSceneManager::StartSceneLoad(const std::filesystem::path& path, bool forPlayMode)
	{
		if (path.empty() || m_Transition.state != TransitionState::None)
		{
			return;
		}

		if (auto* audio = ServiceLocator::TryGet<Audio>())
		{
			audio->StopAll();
		}

		if (forPlayMode)
		{
			CancelTransition();
		}

		std::filesystem::path scenePath = path;
		if (scenePath.is_relative())
		{
			if (Project::GetActive())
			{
				scenePath = Project::GetActive()->GetAssetPath(scenePath);
			}
		}

		m_Transition = {};
		m_Transition.state = TransitionState::SceneLoading;
		m_Transition.targetPath = scenePath;
		m_Transition.forPlayMode = forPlayMode;
		m_Transition.targetState = forPlayMode ? SceneState::Play : SceneState::Edit;
		m_LoadingStatus = "Loading scene...";

		try
		{
			auto* threadPool = ServiceLocator::TryGet<ThreadPool>();
			if (!threadPool)
			{
				CH_CORE_ERROR("Editor: ThreadPool not available, cannot load scene");
				Dialogs::ShowError("Scene loading failed", "ThreadPool not available");
				CancelTransition();
				return;
			}
			m_Transition.future = threadPool->Enqueue([scenePath]() -> SceneLoadResult {
				auto newScene = std::make_shared<Scene>();
				SceneSerializer serializer(newScene.get());
				if (!serializer.Deserialize(scenePath.string()))
				{
					return SceneLoadResult{nullptr, serializer.GetLastError()};
				}
				return SceneLoadResult{newScene, {}};
			});

			CH_CORE_INFO("Editor: Loading scene '{}' on a worker thread.", scenePath.string());
		} catch (const std::exception& e)
		{
			CH_CORE_ERROR("Editor: Failed to start scene load: {}", e.what());
			Dialogs::ShowError("Scene loading failed",
							   "Could not start loading '" + scenePath.string() + "':\n\n" + e.what());
			CancelTransition();
		}
	}

	void EditorSceneManager::UpdateSceneLoading()
	{
		if (!m_Transition.future.valid() ||
			m_Transition.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
		{
			return;
		}

		SceneLoadResult loadResult;
		try
		{
			loadResult = m_Transition.future.get();
		} catch (const std::exception& e)
		{
			CH_CORE_ERROR("Editor: Scene load failed with exception: {}", e.what());
			Dialogs::ShowError("Scene loading failed",
							   "Failed to load scene:\n" + m_Transition.targetPath.string() + "\n\n" + e.what());
			CancelTransition();
			return;
		} catch (...)
		{
			CH_CORE_ERROR("Editor: Scene load failed with unknown exception.");
			Dialogs::ShowError("Scene loading failed", "Failed to load scene:\n" + m_Transition.targetPath.string() +
														   "\n\nAn unknown error occurred.");
			CancelTransition();
			return;
		}

		if (!loadResult.scene)
		{
			std::string reason = loadResult.error.empty() ? "The scene file is corrupt or is not a valid Chained scene."
														  : loadResult.error;
			CH_CORE_ERROR("Editor: Scene load failed for '{}': {}", m_Transition.targetPath.string(), reason);
			Dialogs::ShowError("Scene loading failed",
							   "Failed to load scene:\n" + m_Transition.targetPath.string() + "\n\n" + reason);
			CancelTransition();
			return;
		}

		if (m_Transition.forPlayMode)
		{
			if (m_RuntimeScene)
			{
				CH_CORE_INFO("Editor: Stopping current runtime scene to load '{}'.", m_Transition.targetPath.string());
				m_RuntimeScene->OnRuntimeStop();
			}
			m_RuntimeScene = loadResult.scene;

			if (auto* uiRenderer = ServiceLocator::TryGet<WidgetRenderer>())
			{
				uiRenderer->ResetButtonStates(m_RuntimeScene.get());
			}

			// Map SelectedEntity to the play/simulate scene so the Inspector shows and modifies running state!
			auto& editorState = EditorLayer::Get().GetEditorState();
			if (editorState.SelectedEntity)
			{
				UUID uuid = editorState.SelectedEntity.GetUUID();
				Entity playEntity = m_RuntimeScene->GetEntityByUUID(uuid);
				if (playEntity)
				{
					editorState.SelectedEntity = playEntity;
				}
			}
		}
		else
		{
			EditorLayer::Get().GetEditorState().SelectedEntity = {};
			m_EditorScene = loadResult.scene;
		}

		m_Transition.state = TransitionState::Finalizing;
		m_Transition.sceneReady = false;
		m_AssetWaitLogTimer = 0.0f;
	}

	void EditorSceneManager::UpdateFinalizing()
	{
		if (!m_Transition.sceneReady)
		{
			auto* assetMgr = ServiceLocator::TryGet<AssetManager>();
			if (assetMgr)
			{
				assetMgr->Update(0.016f);

				if (assetMgr->HasBackgroundWork())
				{
					m_AssetWaitLogTimer += 0.016f;
					if (m_AssetWaitLogTimer > 1.0f)
					{
						uint32_t pendingCount = assetMgr->GetPendingFinalizeCount();
						if (pendingCount > 0)
						{
							CH_CORE_INFO("Editor: Waiting for {} assets...", pendingCount);
						}
						m_AssetWaitLogTimer = 0.0f;
					}
					return;
				}
			}
			m_Transition.sceneReady = true;
		}

		FinalizeTransition();
	}

	void EditorSceneManager::FinalizeTransition()
	{
		auto targetScene = m_Transition.forPlayMode ? m_RuntimeScene : m_EditorScene;
		if (!targetScene)
		{
			CancelTransition();
			return;
		}

		auto& layer = EditorLayer::Get();

		if (auto project = Project::GetActive(); project && project->GetEnvironment())
		{
			bool hasEnvironment = targetScene->GetSettings().Environment &&
								  (!targetScene->GetSettings().Environment->GetPath().empty() ||
								   !targetScene->GetSettings().Environment->GetSettings().Skybox.TexturePath.empty());

			if (!hasEnvironment)
			{
				CH_CORE_INFO("Editor: Applying project environment to scene '{}'.",
							 targetScene->GetSettings().ScenePath);
				targetScene->SetEnvironment(project->GetEnvironment());
			}
		}

		targetScene->SetScenePath(m_Transition.targetPath.string());

		if (m_Transition.forPlayMode)
		{
			m_RuntimeScene->TransitionToState(m_Transition.targetState);
			CH_CORE_INFO("Editor: Play Mode Started Successfully");
		}
		else
		{
			m_EditorScene->TransitionToState(SceneState::Edit);

			SceneOpenedEvent e(m_Transition.targetPath.string());
			OnSceneOpened(e);
		}

		layer.GetEditorState().SelectedEntity = {};
		m_LoadingStatus = "";
		m_Transition = {};
		CH_CORE_INFO("Editor: Scene transition complete.");
	}

	void EditorSceneManager::CancelTransition()
	{
		if (m_RuntimeScene && m_Transition.forPlayMode && m_Transition.state != TransitionState::None)
		{
			CH_CORE_INFO("Editor: Stopping runtime scene during transition...");
			m_RuntimeScene->OnRuntimeStop();
			m_RuntimeScene.reset();
		}

		m_Transition = {};
		m_LoadingStatus = "";
	}

	// --- Events ---

	bool EditorSceneManager::OnSceneOpened(SceneOpenedEvent& e)
	{
		auto project = Project::GetActive();
		if (project && !e.GetPath().empty())
		{
			project->SetActiveScenePath(std::filesystem::relative(e.GetPath(), project->GetConfig().ProjectDirectory));

			auto& layer = EditorLayer::Get();
			layer.GetProjectManager().SaveProject();

			layer.GetConfig().LastScenePath = e.GetPath();
			layer.SaveConfig();
			return true;
		}
		return false;
	}

	// --- Confirm dialogs ---

	void EditorSceneManager::ConfirmPendingAction()
	{
		if (m_PendingNewScene)
		{
			m_PendingNewScene = false;
			m_SceneDirty = false;
			SetScene(Scene::CreateDefault());
		}
		else if (m_PendingOpenScene)
		{
			m_PendingOpenScene = false;
			m_SceneDirty = false;
			std::filesystem::path path = m_PendingOpenPath;
			m_PendingOpenPath.clear();

			CH_CORE_INFO("EditorSceneManager: Transition requested to '{}'", path.string());
			if (m_Transition.state == TransitionState::None)
			{
				bool forPlayMode = GetSceneState() == SceneState::Play;
				StartSceneLoad(path, forPlayMode);
			}
		}
	}

	void EditorSceneManager::CancelPendingAction()
	{
		m_PendingNewScene = false;
		m_PendingOpenScene = false;
		m_PendingOpenPath.clear();
	}

} // namespace Chained
