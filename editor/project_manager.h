#ifndef CH_EDITOR_PROJECT_MANAGER_H
#define CH_EDITOR_PROJECT_MANAGER_H

#include "editor/project/editor_settings.h"
#include "engine/scene/scene_events.h"
#include <filesystem>
#include <string>

#include <functional>

namespace Chained
{
	struct EditorConfig;
	class EditorSceneManager;

	class EditorProjectManager
	{
	public:
		EditorProjectManager(EditorConfig* config = nullptr, EditorSceneManager* sceneManager = nullptr,
							 std::function<void()> reloadFontsCallback = nullptr,
							 std::function<void()> saveConfigCallback = nullptr);
		~EditorProjectManager() = default;

		void SetDependencies(EditorConfig* config, EditorSceneManager* sceneManager,
							 std::function<void()> reloadFontsCallback, std::function<void()> saveConfigCallback)
		{
			m_Config = config;
			m_SceneManager = sceneManager;
			m_ReloadFontsCallback = reloadFontsCallback;
			m_SaveConfigCallback = saveConfigCallback;
		}

		void NewProject();
		void NewProject(const std::string& name, const std::string& path);
		void OpenProject();
		void OpenProject(const std::filesystem::path& path);
		void SaveProject();

	public:
		bool OnProjectOpened(ProjectOpenedEvent& e);

		// Runs the deferred part of project opening (font atlas rebuild, scene load).
		// Must be called outside the ImGui frame — see EditorLayer::OnUpdate().
		void ProcessPendingProjectOpen();

		const std::string& GetLastProjectPath() const;
		void RestoreLastProjectPath(const std::string& path);

		// Returns the pending project path and clears it (consume-once).
		std::string ConsumePendingProjectPath();

	private:
		std::string m_LastProjectPath;
		std::string m_PendingOpenedProjectPath;

		EditorConfig* m_Config = nullptr;
		EditorSceneManager* m_SceneManager = nullptr;
		std::function<void()> m_ReloadFontsCallback;
		std::function<void()> m_SaveConfigCallback;
	};

} // namespace Chained

#endif // CH_EDITOR_PROJECT_MANAGER_H
