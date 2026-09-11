#include "engine/platform/backends/glfw/glfw_window.h"
#include "engine/common/engine_assert.h"
#include "engine/common/platform_detection.h"
#include "engine/core/events/window_events.h"
#include "engine/core/input.h"

#include "engine/platform/backends/glfw/glfw_input_mapper.h"
#include "imgui_impl_glfw.h"

#include <GLFW/glfw3.h>
#include <glad/gl.h>
#include <stb_image.h>

namespace Chained
{

	static bool s_GLFWInitialized = false;

	std::unique_ptr<Window> Window::Create(const WindowProperties& properties)
	{

		return std::make_unique<GlfwWindow>(properties);
	}

	static void GLFWErrorCallback(int error, const char* description)
	{
		CH_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
	}

	GlfwWindow::GlfwWindow(const WindowProperties& properties)
	{
		Init(properties);
	}

	GlfwWindow::~GlfwWindow()
	{
		Shutdown();
	}

	void GlfwWindow::Init(const WindowProperties& properties)
	{
		int initialWidth = properties.Width;
		int initialHeight = properties.Height;
		m_Title = properties.Title;
		m_VSync = properties.VSync;
		m_TargetFPS = properties.TargetFramesPerSecond;
		m_ForwardToImGui = true;

		if (!s_GLFWInitialized)
		{
			int success = glfwInit();
			CH_CORE_ASSERT(success, "Could not initialize GLFW!");
			glfwSetErrorCallback(GLFWErrorCallback);
			s_GLFWInitialized = true;
		}

		if (initialWidth <= 0 || initialHeight <= 0)
		{
			GLFWmonitor* monitor = glfwGetPrimaryMonitor();
			if (monitor)
			{
				int workX = 0, workY = 0, workW = 0, workH = 0;
				glfwGetMonitorWorkarea(monitor, &workX, &workY, &workW, &workH);
				initialWidth = (workW > 0) ? workW : 1280;
				initialHeight = (workH > 0) ? workH : 720;
			}
			else
			{
				initialWidth = 1280;
				initialHeight = 720;
			}
		}

		constexpr int minimumWidth = 800;
		constexpr int minimumHeight = 600;
		initialWidth = (initialWidth < minimumWidth) ? minimumWidth : initialWidth;
		initialHeight = (initialHeight < minimumHeight) ? minimumHeight : initialHeight;

		m_Width = (uint32_t)initialWidth;
		m_Height = (uint32_t)initialHeight;
		CH_CORE_INFO("Initializing Glfw Window: {} ({}x{})", m_Title, m_Width, m_Height);

		glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

		m_Samples = properties.Samples;
		glfwWindowHint(GLFW_SAMPLES, m_Samples > 0 ? m_Samples : 0);

#if CH_PLATFORM_MACOS
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
		glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
		glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
		glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
#endif

		m_WindowHandle = glfwCreateWindow((int)m_Width, (int)m_Height, m_Title.c_str(), nullptr, nullptr);
		CH_CORE_ASSERT(m_WindowHandle, "Failed to create GLFW window!");
		glfwSetWindowSizeLimits(m_WindowHandle, minimumWidth, minimumHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);

		glfwMakeContextCurrent(m_WindowHandle);
		glfwSetWindowUserPointer(m_WindowHandle, this);

		int fbWidth, fbHeight;
		glfwGetFramebufferSize(m_WindowHandle, &fbWidth, &fbHeight);
		m_FramebufferWidth = (uint32_t)fbWidth;
		m_FramebufferHeight = (uint32_t)fbHeight;

		glfwSetFramebufferSizeCallback(m_WindowHandle, [](GLFWwindow* window, int width, int height) {
			auto& glWindow = *(GlfwWindow*)glfwGetWindowUserPointer(window);

			glWindow.m_FramebufferWidth = (uint32_t)width;
			glWindow.m_FramebufferHeight = (uint32_t)height;

			int winWidth, winHeight;
			glfwGetWindowSize(window, &winWidth, &winHeight);
			glWindow.SetSizeDirect(winWidth, winHeight);

			// Keep the event on logical window size; framebuffer dimensions are used only for OpenGL viewport state.
			WindowResizeEvent event(winWidth, winHeight);
			if (glWindow.m_EventCallback)
			{
				glWindow.m_EventCallback(event);
			}

			glViewport(0, 0, width, height);
		});
		glfwSetWindowCloseCallback(m_WindowHandle, [](GLFWwindow* window) {
			auto& glWindow = *(GlfwWindow*)glfwGetWindowUserPointer(window);
			WindowCloseEvent event;
			if (glWindow.m_EventCallback)
			{
				glWindow.m_EventCallback(event);
			}
		});

		glfwSetScrollCallback(m_WindowHandle, [](GLFWwindow* window, double xOffset, double yOffset) {
			Core::Input::OnMouseScroll((float)xOffset, (float)yOffset);
			auto* userPtr = glfwGetWindowUserPointer(window);
			if (userPtr && static_cast<GlfwWindow*>(userPtr)->m_ForwardToImGui)
			{
				ImGui_ImplGlfw_ScrollCallback(window, xOffset, yOffset);
			}
		});

		glfwSetKeyCallback(m_WindowHandle, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
			Core::Input::OnKey(GlfwInputMapper::MapKey(key), action != GLFW_RELEASE);
			auto* userPtr = glfwGetWindowUserPointer(window);
			if (userPtr && static_cast<GlfwWindow*>(userPtr)->m_ForwardToImGui)
			{
				ImGui_ImplGlfw_KeyCallback(window, key, scancode, action, mods);
			}
		});

		glfwSetMouseButtonCallback(m_WindowHandle, [](GLFWwindow* window, int button, int action, int mods) {
			Core::Input::OnMouseButton(GlfwInputMapper::MapMouseButton(button), action != GLFW_RELEASE);
			auto* userPtr = glfwGetWindowUserPointer(window);
			if (userPtr && static_cast<GlfwWindow*>(userPtr)->m_ForwardToImGui)
			{
				ImGui_ImplGlfw_MouseButtonCallback(window, button, action, mods);
			}
		});

		glfwSetCursorPosCallback(m_WindowHandle, [](GLFWwindow* window, double xpos, double ypos) {
			Core::Input::OnMouseMove((float)xpos, (float)ypos);
			auto* userPtr = glfwGetWindowUserPointer(window);
			if (userPtr && static_cast<GlfwWindow*>(userPtr)->m_ForwardToImGui)
			{
				ImGui_ImplGlfw_CursorPosCallback(window, xpos, ypos);
			}
		});

		glfwSetCharCallback(m_WindowHandle, [](GLFWwindow* window, unsigned int c) {
			auto* userPtr = glfwGetWindowUserPointer(window);
			if (userPtr && static_cast<GlfwWindow*>(userPtr)->m_ForwardToImGui)
			{
				ImGui_ImplGlfw_CharCallback(window, c);
			}
		});

		glfwSetWindowFocusCallback(m_WindowHandle, [](GLFWwindow* window, int focused) {
			if (!focused)
			{
				Core::Input::ResetAll();
			}
		});

		int status = gladLoadGL((GLADloadfunc)glfwGetProcAddress);
		CH_CORE_ASSERT(status, "Failed to initialize Glad!");

		CH_CORE_INFO("OpenGL Info:");
		CH_CORE_INFO("  Vendor: {0}", (const char*)glGetString(GL_VENDOR));
		CH_CORE_INFO("  Renderer: {0}", (const char*)glGetString(GL_RENDERER));
		CH_CORE_INFO("  Version: {0}", (const char*)glGetString(GL_VERSION));

		SetVSync(m_VSync);
	}

	void GlfwWindow::Shutdown()
	{
		if (m_WindowHandle)
		{
			glfwSetScrollCallback(m_WindowHandle, nullptr);
			glfwSetKeyCallback(m_WindowHandle, nullptr);
			glfwSetMouseButtonCallback(m_WindowHandle, nullptr);
			glfwSetCursorPosCallback(m_WindowHandle, nullptr);
			glfwSetCharCallback(m_WindowHandle, nullptr);
			glfwSetWindowFocusCallback(m_WindowHandle, nullptr);
			glfwSetFramebufferSizeCallback(m_WindowHandle, nullptr);
			glfwSetWindowCloseCallback(m_WindowHandle, nullptr);

			glfwMakeContextCurrent(nullptr);
			glfwDestroyWindow(m_WindowHandle);
			m_WindowHandle = nullptr;
		}

		CH_CORE_INFO("Glfw Window Closed");
	}

	void GlfwWindow::BeginFrame()
	{
		glfwPollEvents();
	}

	void GlfwWindow::EndFrame()
	{
		glfwSwapBuffers(m_WindowHandle);
	}

	bool GlfwWindow::ShouldClose() const
	{
		return glfwWindowShouldClose(m_WindowHandle);
	}

	void GlfwWindow::SetTitle(const std::string& title)
	{
		m_Title = title;
		glfwSetWindowTitle(m_WindowHandle, m_Title.c_str());
	}

	void GlfwWindow::SetSize(int width, int height)
	{
		m_Width = (uint32_t)width;
		m_Height = (uint32_t)height;
		glfwSetWindowSize(m_WindowHandle, (int)m_Width, (int)m_Height);
	}

	void GlfwWindow::SetSizeDirect(int width, int height)
	{
		m_Width = (uint32_t)width;
		m_Height = (uint32_t)height;
	}

	void GlfwWindow::ToggleFullscreen()
	{
		SetFullscreen(!m_IsFullscreen);
	}

	void GlfwWindow::SetFullscreen(bool enabled)
	{
		if (m_IsFullscreen == enabled)
		{
			return;
		}

		if (enabled)
		{
			glfwGetWindowPos(m_WindowHandle, &m_WindowedX, &m_WindowedY);
			glfwGetWindowSize(m_WindowHandle, &m_WindowedWidth, &m_WindowedHeight);

			GLFWmonitor* monitor = glfwGetPrimaryMonitor();
			const GLFWvidmode* mode = glfwGetVideoMode(monitor);
			glfwSetWindowMonitor(m_WindowHandle, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
			m_IsFullscreen = true;
		}
		else
		{
			glfwSetWindowMonitor(m_WindowHandle, nullptr, m_WindowedX, m_WindowedY, m_WindowedWidth, m_WindowedHeight,
								 GLFW_DONT_CARE);
			m_IsFullscreen = false;
		}
	}

	namespace
	{
		// X11 protocol limits property sizes (_NET_WM_ICON) via XMaxRequestSize (~256KB).
		// Passing high-resolution images (512x512, 1024x1024) causes a fatal X11 BadLength
		// protocol error on X_ChangeProperty. Standard desktop window icons are 64x64 or smaller.
		// Downscale large icons to 64x64 using box-filter averaging for clean antialiasing.
		constexpr int kMaxIconDimension = 64;

		std::vector<uint8_t> ResizeIconIfNeeded(const uint8_t* src, int srcW, int srcH, int& outW, int& outH)
		{
			if (srcW <= kMaxIconDimension && srcH <= kMaxIconDimension)
			{
				outW = srcW;
				outH = srcH;
				return std::vector<uint8_t>(src, src + srcW * srcH * 4);
			}

			float scale = static_cast<float>(kMaxIconDimension) / static_cast<float>(std::max(srcW, srcH));
			outW = std::max(1, static_cast<int>(srcW * scale));
			outH = std::max(1, static_cast<int>(srcH * scale));

			std::vector<uint8_t> dst(outW * outH * 4);
			float xRatio = static_cast<float>(srcW) / static_cast<float>(outW);
			float yRatio = static_cast<float>(srcH) / static_cast<float>(outH);

			for (int y = 0; y < outH; ++y)
			{
				int y0 = static_cast<int>(y * yRatio);
				int y1 = std::min(static_cast<int>((y + 1) * yRatio), srcH);
				for (int x = 0; x < outW; ++x)
				{
					int x0 = static_cast<int>(x * xRatio);
					int x1 = std::min(static_cast<int>((x + 1) * xRatio), srcW);

					uint32_t r = 0, g = 0, b = 0, a = 0;
					int count = 0;
					for (int sy = y0; sy < y1; ++sy)
					{
						for (int sx = x0; sx < x1; ++sx)
						{
							const uint8_t* p = src + (sy * srcW + sx) * 4;
							r += p[0];
							g += p[1];
							b += p[2];
							a += p[3];
							++count;
						}
					}
					if (count > 0)
					{
						uint8_t* out = dst.data() + (y * outW + x) * 4;
						out[0] = static_cast<uint8_t>(r / count);
						out[1] = static_cast<uint8_t>(g / count);
						out[2] = static_cast<uint8_t>(b / count);
						out[3] = static_cast<uint8_t>(a / count);
					}
				}
			}
			return dst;
		}
	} // namespace

	void GlfwWindow::SetWindowIcon(const std::string& path)
	{
		int width = 0;
		int height = 0;
		stbi_uc* rawPixels = stbi_load(path.c_str(), &width, &height, nullptr, 4);

		if (rawPixels)
		{
			int iconW = 0;
			int iconH = 0;
			auto resizedPixels = ResizeIconIfNeeded(rawPixels, width, height, iconW, iconH);
			stbi_image_free(rawPixels);

			GLFWimage image{};
			image.width = iconW;
			image.height = iconH;
			image.pixels = resizedPixels.data();

			glfwSetWindowIcon(m_WindowHandle, 1, &image);
		}
		else
		{
			CH_CORE_WARN("Failed to load window icon from {}", path);
		}
	}

	void GlfwWindow::SetWindowIconFromMemory(const uint8_t* data, size_t size)
	{
		int width = 0;
		int height = 0;
		stbi_uc* rawPixels = stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(data), static_cast<int>(size),
												   &width, &height, nullptr, 4);

		if (rawPixels)
		{
			int iconW = 0;
			int iconH = 0;
			auto resizedPixels = ResizeIconIfNeeded(rawPixels, width, height, iconW, iconH);
			stbi_image_free(rawPixels);

			GLFWimage image{};
			image.width = iconW;
			image.height = iconH;
			image.pixels = resizedPixels.data();

			glfwSetWindowIcon(m_WindowHandle, 1, &image);
		}
		else
		{
			CH_CORE_WARN("Failed to decode window icon from memory");
		}
	}

	void GlfwWindow::SetVSync(bool enabled)
	{
		m_VSync = enabled;
		glfwSwapInterval(m_VSync ? 1 : 0);
	}

	void GlfwWindow::SetAntialiasing(bool enabled)
	{
		// GLFW_SAMPLES only takes effect on the *next* glfwCreateWindow call, so calling
		// glfwWindowHint here would be a silent no-op for the window that's already open.
		// If the framebuffer wasn't created with samples, GL_MULTISAMPLE has nothing to
		// multisample and this call is harmless but ineffective - warn so it's not a silent no-op.
		if (enabled && m_Samples <= 0)
		{
			CH_CORE_WARN("SetAntialiasing(true) requested but the window was created with "
						 "Samples = 0. MSAA sample count can only be changed by "
						 "recreating the window; toggling GL_MULTISAMPLE will have no visible effect.");
		}

		if (enabled)
		{
			glEnable(GL_MULTISAMPLE);
		}
		else
		{
			glDisable(GL_MULTISAMPLE);
		}
	}

	void GlfwWindow::SetTargetFramesPerSecond(int framesPerSecond)
	{
		m_TargetFPS = framesPerSecond;
	}

	void GlfwWindow::SetCursorMode(CursorMode mode)
	{
		switch (mode)
		{
		case CursorMode::Normal:
			glfwSetInputMode(m_WindowHandle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			break;
		case CursorMode::Hidden:
			glfwSetInputMode(m_WindowHandle, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
			break;
		case CursorMode::Locked:
			glfwSetInputMode(m_WindowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			break;
		}
	}

	bool GlfwWindow::IsFocused() const
	{
		return glfwGetWindowAttrib(m_WindowHandle, GLFW_FOCUSED) == GLFW_TRUE;
	}

	void GlfwWindow::SetClipboardText(const std::string& text)
	{
		glfwSetClipboardString(m_WindowHandle, text.c_str());
	}

	std::string GlfwWindow::GetClipboardText() const
	{
		const char* text = glfwGetClipboardString(m_WindowHandle);
		return text ? std::string(text) : "";
	}

} // namespace Chained