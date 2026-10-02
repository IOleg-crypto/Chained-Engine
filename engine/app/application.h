#ifndef CH_APPLICATION_H
#define CH_APPLICATION_H

#include "application_types.h"
#include "engine/core/events/events.h"
#include "engine/core/layer_stack.h"
#include "engine/core/window.h"
#include "engine/common/base.h"
#include "engine/common/timestep.h"
#include "engine/common/engine_assert.h"
#include <filesystem>
#include <memory>

namespace Chained
{
	class ImGuiLayer;
	class Layer;

	class CH_API Application
	{
	public:
		explicit Application(const ApplicationSpecification& specification);
		virtual ~Application();

		static Application& Get()
		{
			CH_CORE_ASSERT(s_Instance, "Application instance does not exist!");
			return *s_Instance;
		}

		void Run();
		void Close()
		{
			m_Running = false;
		}

		void OnEvent(Event& e);

		Window& GetWindow()
		{
			return *m_Window;
		}
		const Window& GetWindow() const
		{
			return *m_Window;
		}
		ImGuiLayer* GetImGuiLayer()
		{
			return m_ImGuiLayer;
		}
		const ApplicationSpecification& GetSpecification() const
		{
			return m_Specification;
		}
		LayerStack& GetLayerStack()
		{
			return *m_LayerStack;
		}
		void PushLayer(std::unique_ptr<Layer> layer) const;
		void PushOverlay(std::unique_ptr<Layer> overlay) const;
		Timestep GetFrameTime() const
		{
			return m_Timer.DeltaTime;
		}

		static std::filesystem::path GetExecutableDirectory();

	private:
		void InitializePlatform();
		void RegisterCoreServices();
		void RegisterRuntimeServices();
		void RegisterGameplayServices();

	private:
		ApplicationSpecification m_Specification;
		std::unique_ptr<Window> m_Window;
		std::unique_ptr<LayerStack> m_LayerStack;
		ImGuiLayer* m_ImGuiLayer = nullptr;
		bool m_Running;

		struct
		{
			float Accumulator = 0.0f;
			float FixedStepCount = 1.0f / 120.0f;
			Timestep DeltaTime = 0.0f;
			float LastFrameTime = 0.0f;
		} m_Timer;

	private:
		inline static Application* s_Instance = nullptr;
	};

	Application* CreateApplication(ApplicationCommandLineArgs args);
} // namespace Chained

#endif // CH_APPLICATION_H