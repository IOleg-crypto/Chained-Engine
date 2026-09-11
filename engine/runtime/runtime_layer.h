#ifndef CH_RUNTIME_LAYER_H
#define CH_RUNTIME_LAYER_H

#include "engine/core/layer.h"
#include "engine/graphics/api/framebuffer.h"
#include "engine/scene/scene.h"
#include "engine/graphics/pipeline/scene_renderer.h"
#include "engine/graphics/pipeline/renderer.h"
#include "engine/assets/asset_manager.h"
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Chained
{

	enum class RuntimeLoadState : uint8_t
	{
		Idle,
		LoadingScene,
		ReadyToPlay,
		Running
	};

	struct LoadingState
	{
		RuntimeLoadState State = RuntimeLoadState::Idle;
		float OverlayElapsed = 0.0f;
		float MinOverlayDuration = 0.35f;
		bool SuppressNextUIInput = false;
	};

	// Runs the game/runtime experience, including scene loading and the scene renderer.
	class RuntimeLayer : public Layer
	{
	public:
		RuntimeLayer(const std::string& projectPath);

		~RuntimeLayer() override;

		void OnAttach() override;
		void OnDetach() override;
		void OnUpdate(Timestep ts) override;
		void OnRender(Timestep ts) override;
		void OnImGuiRender() override;
		void OnEvent(Event& e) override;

		// Loads a scene by project-relative or absolute path.
		void LoadScene(const std::string& path);

		bool IsRunning() const
		{
			return m_LoadState.State == RuntimeLoadState::Running;
		}

	private:
		bool InitProject(const std::string& projectPath);
		bool DiscoverAndLoadProject(const std::string& projectPath);
		void ApplyWindowConfiguration();
		void SetupBrandingAndIcon();
		void LoadInitialScene();
		bool TransitionToScene(const std::filesystem::path& scenePath);
		void StopCurrentScene();
		void PreloadSceneFonts(bool allowRuntimeMutation);
		std::vector<std::pair<std::string, float>> CollectSceneFontRequests() const;
		void EnsureRuntimeFramebuffer(uint32_t width, uint32_t height);
		bool IsSceneReadyToStart() const;
		void DrawLoadingOverlay();

		std::optional<Camera3D> GetActiveCamera();
		glm::vec4 CalculateBackgroundColor() const;

		void TickFontRebuild(Timestep ts);
		void TickSceneLoading(Timestep ts);
		void TickRunning(Timestep ts);

		bool SetupNewScene(const std::filesystem::path& scenePath);
		void ResetUIState();
		void BeginSceneLoading();
		void AppendFontRequest(const struct TextStyle& style, std::vector<std::pair<std::string, float>>& out,
							   std::unordered_set<std::string>& dedupe) const;

	private:
		std::shared_ptr<Scene> m_Scene;
		std::unique_ptr<SceneRenderer> m_SceneRenderer;
		Renderer* m_Renderer = nullptr;
		AssetManager* m_AssetManager = nullptr;

	private:
		std::string m_ProjectPath;
		LoadingState m_LoadState;

		std::string m_PendingScenePath;
		std::shared_ptr<Framebuffer> m_HDRFramebuffer;
		uint32_t m_MSAAFramebufferSamples = 0;
	};
} // namespace Chained

#endif // CH_RUNTIME_LAYER_H
