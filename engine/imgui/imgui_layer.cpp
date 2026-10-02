#include "imgui_layer.h"
#include "engine/core/profiler.h"
#include "engine/core/window.h"

#include "ImGuizmo.h"
#include "backends/imgui_impl_opengl3.h"
#include "engine/app/application.h"
#include "imgui.h"

#include "backends/imgui_impl_glfw.h"
#include <GLFW/glfw3.h>

namespace Chained
{
	static constexpr const char* kGLSLVersion = "#version 430";

	void ImGuiLayer::SetContext(ImGuiContext* context)
	{
		ImGui::SetCurrentContext(context);
	}

	void ImGuiLayer::OnAttach()
	{
		CH_PROFILE_FUNCTION();

		// Validate native window before creating the ImGui context to avoid leaks on failure.
		GLFWwindow* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		if (!window)
		{
			CH_CORE_ERROR("ImGuiLayer: Failed to get native window handle!");
			return;
		}

		// Setup ImGui context
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();

		// Docking and viewports are always enabled
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;
		io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;

		// Setup style
		ImGui::StyleColorsDark();

		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		ImGui_ImplGlfw_InitForOpenGL(window, false);
		ImGui_ImplOpenGL3_Init(kGLSLVersion);
	}

	void ImGuiLayer::OnDetach()
	{
		CH_PROFILE_FUNCTION();

		ImGui::DestroyPlatformWindows();
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();
	}

	void ImGuiLayer::Begin()
	{
		CH_PROFILE_FUNCTION();

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}

	void ImGuiLayer::End()
	{
		CH_PROFILE_FUNCTION();

		ImGuiIO& io = ImGui::GetIO();
		io.DisplaySize =
			ImVec2((float)Application::Get().GetWindow().GetWidth(), (float)Application::Get().GetWindow().GetHeight());

		// 1. Render main window
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// 2. Render additional windows (Viewports)
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			GLFWwindow* backup_current_context = glfwGetCurrentContext();
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
			glfwMakeContextCurrent(backup_current_context);
		}

		// === SUPER SAFE ZONE: FRAME FULLY COMPLETED ===
		if (!m_DeferredTasks.empty())
		{
			for (const auto& task : m_DeferredTasks)
			{
				if (task)
				{
					task();
				}
			}
			m_DeferredTasks.clear();
		}
	}

	bool ImGuiLayer::RefreshFontAtlasTexture()
	{
		if (!ImGui::GetCurrentContext())
		{
			CH_CORE_WARN("ImGuiLayer: Cannot refresh font atlas without an active ImGui context.");
			return false;
		}

		// Build the atlas explicitly so that CreateDeviceObjects() always uploads
		// a valid texture (ImGui_ImplOpenGL3_CreateFontsTexture checks IsBuilt(),
		// but after ClearFonts() + AddFontFromFile() the atlas is dirty/unbuilt).
		ImGui::GetIO().Fonts->Build();

		ImGui_ImplOpenGL3_DestroyDeviceObjects();
		if (!ImGui_ImplOpenGL3_CreateDeviceObjects())
		{
			CH_CORE_ERROR("ImGuiLayer: Failed to recreate OpenGL device objects for font atlas refresh.");
			return false;
		}

		CH_CORE_INFO("ImGuiLayer: Refreshed font atlas texture.");
		return true;
	}

	void ImGuiLayer::OnEvent(Event& e)
	{
		if (m_BlockEvents && !e.Handled)
		{
			ImGuiIO& io = ImGui::GetIO();
			e.Handled |= e.IsInCategory(EventCategoryKeyboard) && io.WantCaptureKeyboard;
		}
	}

	void* ImGuiLayer::AddFontFromFile(const std::string& path, float size, const void* config, const void* ranges)
	{
		if (!ImGui::GetCurrentContext())
		{
			return nullptr;
		}

		ImGuiIO& io = ImGui::GetIO();

		// ImGui внутрішньо робить глибоку копію конфігу, якщо ми передаємо його через AddFontFromFileTTF.
		// Але він НЕ копіює масив ranges, тому static масив у EditorLayer — це єдиний порятунок від крашу.
		ImFont* font =
			io.Fonts->AddFontFromFileTTF(path.c_str(), size, (const ImFontConfig*)config, (const ImWchar*)ranges);

		if (!font)
		{
			CH_CORE_ERROR("ImGuiLayer: Failed to load font from '{}'", path);
		}
		return font;
	}

	void ImGuiLayer::ClearFonts()
	{
		if (!ImGui::GetCurrentContext())
		{
			CH_CORE_WARN("ImGuiLayer: Cannot clear fonts without an active ImGui context.");
			return;
		}
		ImGuiIO& io = ImGui::GetIO();
		io.Fonts->Clear();
		// Reset FontDefault to avoid a dangling pointer — it pointed into the
		// now-freed atlas data.  ImGui will pick up the first font in the
		// rebuilt atlas on the next frame.
		io.FontDefault = nullptr;
	}
} // namespace Chained
