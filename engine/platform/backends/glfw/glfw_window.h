#ifndef CH_GLFW_WINDOW_H
#define CH_GLFW_WINDOW_H

#include "engine/core/window.h"
#include <string>

struct GLFWwindow;

namespace Chained
{

	// GLFW-backed window implementation that owns the native window, callbacks, and display settings.
	class GlfwWindow : public Window
	{
	public:
		GlfwWindow(const WindowProperties& properties);
		virtual ~GlfwWindow();

		GlfwWindow(const GlfwWindow&) = delete;
		GlfwWindow& operator=(const GlfwWindow&) = delete;

		virtual void BeginFrame() override;
		virtual void EndFrame() override;

		virtual bool ShouldClose() const override;

		virtual int GetWidth() const override
		{
			return m_Width;
		}
		virtual int GetHeight() const override
		{
			return m_Height;
		}

		int GetFramebufferWidth() const
		{
			return m_FramebufferWidth;
		}
		int GetFramebufferHeight() const
		{
			return m_FramebufferHeight;
		}

		virtual void SetTitle(const std::string& title) override;
		virtual void SetSize(int width, int height) override;
		virtual void SetSizeDirect(int width, int height) override;

		virtual void ToggleFullscreen() override;
		virtual void SetFullscreen(bool enabled) override;

		virtual void SetVSync(bool enabled) override;
		virtual bool GetVSync() const override
		{
			return m_VSync;
		}
		virtual void SetAntialiasing(bool enabled) override;
		virtual bool IsFullscreen() const override
		{
			return m_IsFullscreen;
		}
		virtual void SetTargetFramesPerSecond(int framesPerSecond) override;
		virtual int GetTargetFramesPerSecond() const override
		{
			return m_TargetFPS;
		}
		virtual void SetWindowIcon(const std::string& path) override;
		virtual void SetWindowIconFromMemory(const uint8_t* data, size_t size) override;
		virtual void SetCursorMode(CursorMode mode) override;
		virtual bool IsFocused() const override;

		virtual void SetClipboardText(const std::string& text) override;
		virtual std::string GetClipboardText() const override;

		virtual void SetEventCallback(const EventCallbackFn& callback) override
		{
			m_EventCallback = callback;
		}

		virtual void* GetNativeWindow() const override
		{
			return m_WindowHandle;
		}

	private:
		void Init(const WindowProperties& properties);
		void Shutdown();

	private:
		GLFWwindow* m_WindowHandle = nullptr;

		int m_Width = 0;
		int m_Height = 0;

		int m_FramebufferWidth = 0;
		int m_FramebufferHeight = 0;

		std::string m_Title;
		bool m_VSync = true;
		bool m_IsFullscreen = false;
		int m_WindowedX = 0, m_WindowedY = 0, m_WindowedWidth = 0, m_WindowedHeight = 0;
		int m_TargetFPS = 60;
		int m_Samples = 0; // MSAA sample count the window/framebuffer was actually created with (0 = none)
		bool m_ForwardToImGui = true;
		EventCallbackFn m_EventCallback;
	};

} // namespace Chained

#endif // CH_GLFW_WINDOW_H