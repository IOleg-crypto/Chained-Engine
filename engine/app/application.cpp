#include "engine/app/application.h"
#include "engine/graphics/api/graphics_device.h"
#include "engine/core/profiler.h"
#include "engine/core/platform.h"
#include "engine/imgui/imgui_layer.h"
#include "engine/core/events/window_events.h"
#include "engine/core/service_locator.h"
#include "engine/scene/component_registry.h"
#include "engine/project/project.h"
#include "engine/common/thread_pool.h"
#include "engine/assets/asset_manager.h"
#include "engine/audio/audio.h"
#include "engine/graphics/pipeline/renderer.h"
#include "engine/ui/widget_renderer.h"
#include "engine/ui/ui_font_registry.h"
#include "engine/graphics/pipeline/debug_renderer.h"
#include "engine/physics/physics.h"
#include "engine/core/input.h"
#include "engine/scripting/scriptengine.h"
#include "engine/networking/network_service.h"
#include "engine/scene/systems/network_system.h"

namespace Chained
{
	std::filesystem::path Application::GetExecutableDirectory()
	{
		return Platform::GetExecutableDirectory();
	}

	Application::Application(const ApplicationSpecification& spec)
		: m_Specification(spec)
	{
		CH_ASSERT(!s_Instance);
		s_Instance = this;

		Log::Init();
		ComponentRegistry::RegisterEngineComponents();

		if (!m_Specification.WorkingDirectory.empty())
		{
			std::filesystem::current_path(m_Specification.WorkingDirectory);
		}

		InitializePlatform();
		RegisterCoreServices();
		RegisterRuntimeServices();
		RegisterGameplayServices();

		// Freeze the locator, then initialize all modules.
		ServiceLocator::Lock();
		ServiceLocator::InitializeModule();

		if (m_Window)
		{
			if (auto* renderer = ServiceLocator::TryGet<Renderer>())
			{
				renderer->SetViewportSize(m_Window->GetWidth(), m_Window->GetHeight());
			}
		}

		m_LayerStack = std::make_unique<LayerStack>();
		m_Timer.LastFrameTime = Platform::GetTime();
		m_Running = true;

		if (!m_Specification.Headless)
		{
			auto imguiLayer = std::make_unique<ImGuiLayer>();
			m_ImGuiLayer = imguiLayer.get();
			PushOverlay(std::move(imguiLayer));
		}
	}

	void Application::InitializePlatform()
	{
		const bool isHeadless = m_Specification.Headless;
		if (!isHeadless)
		{
			m_Window = Window::Create(m_Specification.Window);
			m_Window->SetEventCallback(CH_BIND_EVENT_FN(Application::OnEvent));
		}
		else
		{
			GraphicsDevice::SetAPI(GraphicsDevice::API::None);
		}
	}

	void Application::RegisterCoreServices()
	{
		unsigned int threads = std::thread::hardware_concurrency();
		if (threads == 0)
		{
			threads = 1;
		}
		unsigned int workerCount = (threads > 1) ? (threads - 1) : 1;

		std::filesystem::path resourcesDir;
		if (!m_Specification.ResourcesDir.empty() && std::filesystem::exists(m_Specification.ResourcesDir))
		{
			resourcesDir = m_Specification.ResourcesDir;
		}

		// 1. Input — must be first (window callbacks fire during creation)
		ServiceLocator::Provide<Core::Input>([] { return std::make_unique<Core::Input>(); });

		// 1b. Instrumentor — profiler (must be early for CH_PROFILE_* macros)
		ServiceLocator::Provide<Instrumentor>([] { return std::make_unique<Instrumentor>(); });

		// 2. ThreadPool
		ServiceLocator::Provide<ThreadPool>([=] { return std::make_unique<ThreadPool>(workerCount); });

		// 3. AssetManager — override asset directory only when explicitly provided
		ServiceLocator::Provide<AssetManager>([&, resourcesDir] {
			auto am = std::make_unique<AssetManager>();
			am->SetEngineRoot(m_Specification.EngineRoot);
			if (!resourcesDir.empty())
			{
				am->SetAssetDirectory(resourcesDir);
			}
			// Source directories for dev mode (runtime reads from source tree)
			if (!m_Specification.SourceResourcesDir.empty())
			{
				am->SetSourceResourcesDir(m_Specification.SourceResourcesDir);
			}
			if (!m_Specification.SourceAssetsDir.empty())
			{
				am->SetSourceAssetsDir(m_Specification.SourceAssetsDir);
			}
			return am;
		});
	}

	void Application::RegisterRuntimeServices()
	{
		const bool isHeadless = m_Specification.Headless;
		if (!isHeadless)
		{
			ServiceLocator::Provide<Renderer>([] { return std::make_unique<Renderer>(); });
			ServiceLocator::Provide<UIFontRegistry>([] { return std::make_unique<UIFontRegistry>(); });
			ServiceLocator::Provide<WidgetRenderer>([] { return std::make_unique<WidgetRenderer>(); });
			ServiceLocator::Provide<DebugRenderer>([] { return std::make_unique<DebugRenderer>(); });
		}
	}

	void Application::RegisterGameplayServices()
	{
		ServiceLocator::Provide<Audio>([] { return std::make_unique<Audio>(); });
		ServiceLocator::Provide<Physics>([] { return std::make_unique<Physics>(); });
		if (m_Specification.EnableScripting)
		{
			ServiceLocator::Provide<ScriptEngine>(
				[=] { return std::make_unique<ScriptEngine>(m_Specification.EnableScripting); });
		}
		ServiceLocator::Provide<Network>([] { return std::make_unique<Network>(); });
		ServiceLocator::Provide<NetworkSystem>([] { return std::make_unique<NetworkSystem>(); });
	}

	Application::~Application()
	{

		m_LayerStack.reset();

		// WARNING: OpenGL resources held by the Project (like Environment maps)
		// MUST be released before the OpenGL context is destroyed (via m_Window.reset()).
		// Setting Active Project to nullptr invokes destructors cleanly.
		Project::SetActive(nullptr);

		ServiceLocator::Shutdown();
		GraphicsDevice::ProcessResourceDeletions();
		m_Window.reset();
		// Log::Shutdown() MUST come after m_Window.reset(): ~GlfwWindow::Shutdown() emits
		// CH_CORE_INFO("Glfw Window Closed"). If the core logger were reset first, that log
		// call dereferences a null spdlog::logger and segfaults (access violation reading the
		// atomic log level). Logging outlives every subsystem that can log during teardown.
		Log::Shutdown();
		s_Instance = nullptr;
	}

	void Application::Run()
	{
		while (m_Running && (!m_Window || !m_Window->ShouldClose()))
		{
			float time = Platform::GetTime();
			float rawDelta = time - m_Timer.LastFrameTime;
			m_Timer.LastFrameTime = time;
			float clampedDelta = (rawDelta > 0.0f) ? std::min(rawDelta, 0.1f) : 0.0f;
			m_Timer.DeltaTime = Timestep(clampedDelta);

			if (auto* inst = Instrumentor::TryGet())
			{
				inst->BeginFrame();
			}

			if (m_Window)
			{
				m_Window->BeginFrame();
			}

			// Process any GPU resource deletions queued from worker threads
			GraphicsDevice::ProcessResourceDeletions();

			if (m_Window && m_Window->GetWidth() > 0 && m_Window->GetHeight() > 0)
			{
				if (auto* audio = ServiceLocator::TryGet<Audio>())
				{
					audio->Update(m_Timer.DeltaTime);
				}
				if (auto* am = ServiceLocator::TryGet<AssetManager>())
				{
					am->Update(m_Timer.DeltaTime);
				}
				m_Timer.Accumulator += (float)m_Timer.DeltaTime;
				m_Timer.Accumulator = std::min(m_Timer.Accumulator, 0.1f);
				while (m_Timer.Accumulator >= m_Timer.FixedStepCount)
				{
					for (auto& layer : *m_LayerStack)
					{
						layer->OnFixedUpdate(Timestep(m_Timer.FixedStepCount));
					}

					m_Timer.Accumulator -= m_Timer.FixedStepCount;
				}

				for (auto& layer : *m_LayerStack)
				{
					layer->OnUpdate(m_Timer.DeltaTime);
				}

				Core::Input::Update(m_Timer.DeltaTime);

				for (auto& layer : *m_LayerStack)
				{
					layer->OnRender(m_Timer.DeltaTime);
				}

				if (m_ImGuiLayer)
				{
					m_ImGuiLayer->Begin();
					for (auto& layer : *m_LayerStack)
					{
						layer->OnImGuiRender();
					}
					m_ImGuiLayer->End();
				}

				m_Window->EndFrame();
			}
		}
	}

	void Application::OnEvent(Event& e)
	{
		if (e.GetEventType() == EventType::WindowResize)
		{
			auto& re = static_cast<WindowResizeEvent&>(e);
			if (auto* renderer = ServiceLocator::TryGet<Renderer>())
			{
				renderer->SetViewportSize(re.GetWidth(), re.GetHeight());
			}
		}

		auto& layers = m_LayerStack->GetLayers();
		for (int i = static_cast<int>(layers.size()) - 1; i >= 0; --i)
		{
			if (e.Handled)
			{
				break;
			}
			if (i < static_cast<int>(layers.size()) && layers[i])
			{
				layers[i]->OnEvent(e);
			}
		}
	}

	void Application::PushLayer(std::unique_ptr<Layer> layer) const
	{
		Layer* raw = layer.get();
		m_LayerStack->PushLayer(std::move(layer));
		raw->OnAttach();
	}

	void Application::PushOverlay(std::unique_ptr<Layer> overlay) const
	{
		Layer* raw = overlay.get();
		m_LayerStack->PushOverlay(std::move(overlay));
		raw->OnAttach();
	}

} // namespace Chained