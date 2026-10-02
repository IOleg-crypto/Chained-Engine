#ifndef CH_VIEWPORT_RENDERER_H
#define CH_VIEWPORT_RENDERER_H

#include <cstdint>
#include <memory>

namespace Chained
{
	class Framebuffer;
	class Scene;
	class SceneRenderer;
	struct Camera3D;

	// Manages the two-pass HDR render pipeline for the editor viewport:
	//   1) Scene → HDR FBO (RGBA16F + MSAA)
	//   2) Resolve MSAA → post-process into viewport FBO (RGBA8)
	// The viewport FBO texture is then displayed as an ImGui image.
	class ViewportRenderer
	{
	public:
		ViewportRenderer();
		~ViewportRenderer() = default;

		// Creates the initial framebuffers at the given dimensions.
		void Init(uint32_t width, uint32_t height);

		// Resizes both framebuffers and recreates them if they became invalid
		// or if the project's MSAA sample count changed.
		void Resize(uint32_t width, uint32_t height, bool forceRecreate = false);

		// Full two-pass render: HDR pass → resolve → post-process into viewport FBO.
		void RenderScene(Scene* scene, const Camera3D& camera);

		// Clears the HDR framebuffer with the scene's background color/mode.
		void ClearBackground(Scene* scene);

		// Call each frame to detect MSAA sample count changes and recreate the HDR FBO.
		void CheckMSAAChange();

		// Renders billboard editor icons (cameras, lights, spawns, audio) into the
		// currently-bound HDR framebuffer. Called during the HDR pass.
		void RenderEditorIcons(entt::registry& registry, const Camera3D& camera);

		// Accessors
		std::shared_ptr<Framebuffer> GetViewportFramebuffer() const
		{
			return m_ViewportFramebuffer;
		}
		std::shared_ptr<Framebuffer> GetHDRFramebuffer() const
		{
			return m_HDRFramebuffer;
		}
		bool IsValid() const;
		SceneRenderer* GetSceneRenderer() const
		{
			return m_SceneRenderer.get();
		}
		uint32_t GetMSAASamples() const
		{
			return m_MSAAFramebufferSamples;
		}

	private:
		std::shared_ptr<Framebuffer> m_ViewportFramebuffer;
		std::shared_ptr<Framebuffer> m_HDRFramebuffer;
		uint32_t m_MSAAFramebufferSamples = 1;
		std::unique_ptr<SceneRenderer> m_SceneRenderer;
	};

} // namespace Chained

#endif // CH_VIEWPORT_RENDERER_H
