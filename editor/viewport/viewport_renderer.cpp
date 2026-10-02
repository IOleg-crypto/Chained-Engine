#include "viewport_renderer.h"
#include "viewport_icons.h"
#include "editor/layer.h"
#include "engine/app/application.h"
#include "engine/graphics/api/framebuffer.h"
#include "engine/graphics/api/graphics_device.h"
#include "engine/graphics/pipeline/renderer.h"
#include "engine/graphics/pipeline/scene_renderer.h"
#include "engine/core/service_locator.h"
#include "engine/project/project.h"
#include "engine/scene/scene.h"

namespace Chained
{

	static uint32_t GetConfiguredMSAASamples()
	{
		auto project = Project::GetActive();
		int samples = project ? project->GetConfig().Render.AntiAliasingSamples : 4;
		return samples > 1 ? (uint32_t)samples : 1u;
	}

	ViewportRenderer::ViewportRenderer()
	{
		m_SceneRenderer = std::make_unique<SceneRenderer>();
	}

	void ViewportRenderer::Init(uint32_t width, uint32_t height)
	{
		if (width == 0)
		{
			width = 1280;
		}
		if (height == 0)
		{
			height = 720;
		}

		FramebufferSpecification spec;
		spec.Width = width;
		spec.Height = height;
		spec.ColorFormat = FramebufferColorFormat::RGBA8;
		m_ViewportFramebuffer = Framebuffer::Create(spec);

		FramebufferSpecification hdrSpec = spec;
		hdrSpec.ColorFormat = FramebufferColorFormat::RGBA16F;
		hdrSpec.Samples = GetConfiguredMSAASamples();
		m_MSAAFramebufferSamples = hdrSpec.Samples;
		m_HDRFramebuffer = Framebuffer::Create(hdrSpec);
	}

	void ViewportRenderer::Resize(uint32_t width, uint32_t height, bool forceRecreate)
	{
		if (width == 0 || height == 0)
		{
			return;
		}

		if (m_ViewportFramebuffer)
		{
			m_ViewportFramebuffer->Resize(width, height);
		}
		if (m_HDRFramebuffer)
		{
			m_HDRFramebuffer->Resize(width, height);
		}

		if (auto* renderer = ServiceLocator::TryGet<Renderer>())
		{
			renderer->SetViewportSize(width, height);
		}

		if (forceRecreate)
		{
			if (m_ViewportFramebuffer)
			{
				m_ViewportFramebuffer.reset();
			}
			if (m_HDRFramebuffer)
			{
				m_HDRFramebuffer.reset();
			}
		}

		uint32_t configuredSamples = GetConfiguredMSAASamples();
		if (m_HDRFramebuffer && configuredSamples != m_MSAAFramebufferSamples)
		{
			m_HDRFramebuffer.reset();
		}

		if (!m_ViewportFramebuffer || !m_ViewportFramebuffer->IsValid())
		{
			FramebufferSpecification spec;
			spec.Width = width;
			spec.Height = height;
			spec.ColorFormat = FramebufferColorFormat::RGBA8;
			m_ViewportFramebuffer = Framebuffer::Create(spec);
		}
		if (!m_HDRFramebuffer || !m_HDRFramebuffer->IsValid())
		{
			FramebufferSpecification hdrSpec;
			hdrSpec.Width = width;
			hdrSpec.Height = height;
			hdrSpec.ColorFormat = FramebufferColorFormat::RGBA16F;
			hdrSpec.Samples = configuredSamples;
			m_MSAAFramebufferSamples = configuredSamples;
			m_HDRFramebuffer = Framebuffer::Create(hdrSpec);
		}
	}

	void ViewportRenderer::ClearBackground(Scene* scene)
	{
		auto mode = scene->GetSettings().Mode;
		if (mode == BackgroundMode::Color)
		{
			GraphicsDevice::Get().Clear(scene->GetSettings().BackgroundColor);
		}
		else if (mode == BackgroundMode::Texture)
		{
			auto& path = scene->GetSettings().BackgroundTexturePath;
			if (!path.empty())
			{
				GraphicsDevice::Get().Clear(scene->GetSettings().BackgroundColor);
			}
		}
		else if (mode == BackgroundMode::Environment3D)
		{
			GraphicsDevice::Get().Clear({0, 0, 0, 255});
		}
	}

	void ViewportRenderer::CheckMSAAChange()
	{
		uint32_t configuredSamples = GetConfiguredMSAASamples();
		if (m_HDRFramebuffer && configuredSamples != m_MSAAFramebufferSamples)
		{
			m_HDRFramebuffer.reset();
		}
	}

	void ViewportRenderer::RenderScene(Scene* scene, const Camera3D& camera)
	{
		if (!m_HDRFramebuffer || !m_HDRFramebuffer->IsValid())
		{
			return;
		}

		m_HDRFramebuffer->Bind();
		ClearBackground(scene);

		if (!scene)
		{
			m_HDRFramebuffer->Unbind();
			return;
		}

		Camera3D cam = camera;
		if (glm::distance(glm::vec3(cam.Position), glm::vec3(cam.Target)) < 0.001f)
		{
			cam.Position.z += 1.0f;
			cam.ViewMatrix = glm::lookAt(glm::vec3(cam.Position), glm::vec3(cam.Target), glm::vec3(cam.Up));
		}

		SceneRenderOptions options;
		auto& currentDebugFlags = scene->GetSettings().DebugFlags;
		options.DrawGrid = currentDebugFlags.DrawGrid;
		options.ShowDebugColliders = currentDebugFlags.DrawColliders;
		options.ShowDebugSpawnZones = currentDebugFlags.DrawSpawnZones;
		options.SetCollisionWireframeMode = currentDebugFlags.SetCollisionWireframeMode;
		m_SceneRenderer->RenderScene(scene->GetRegistry(), scene->GetSettings(), cam, options);

		if (EditorLayer::Get().GetSceneManager().GetSceneState() != SceneState::Play &&
			EditorLayer::Get().GetConfig().ShowEditorIcons)
		{
			RenderEditorIcons(scene->GetRegistry(), cam);
		}

		m_HDRFramebuffer->Unbind();
		m_HDRFramebuffer->Resolve();

		if (!m_ViewportFramebuffer || !m_ViewportFramebuffer->IsValid())
		{
			return;
		}

		m_ViewportFramebuffer->Bind();
		GraphicsDevice::Get().Clear({0, 0, 0, 255});

		if (auto* renderer = ServiceLocator::TryGet<Renderer>())
		{
			renderer->ApplyPostProcessing(m_HDRFramebuffer->GetColorAttachmentRendererID(),
										  m_HDRFramebuffer->GetDepthAttachmentRendererID(), cam, nullptr, {});
		}

		m_ViewportFramebuffer->Unbind();
	}

	void ViewportRenderer::RenderEditorIcons(entt::registry& registry, const Camera3D& camera)
	{
		ViewportIcons::RenderAll(registry, camera);
	}

	bool ViewportRenderer::IsValid() const
	{
		return m_ViewportFramebuffer && m_ViewportFramebuffer->IsValid();
	}

} // namespace Chained
