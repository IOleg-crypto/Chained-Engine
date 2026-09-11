#include "script_glue_system.h"
#include "engine/project/project.h"
#include <GLFW/glfw3.h>
#include <set>

namespace Chained
{
	void Log_Info(const Coral::UCChar* message)
	{
		CH_CORE_INFO("[C#] {}", ch_u16_to_string(message));
	}
	void Log_Warn(const Coral::UCChar* message)
	{
		CH_CORE_WARN("[C#] {}", ch_u16_to_string(message));
	}
	void Log_Error(const Coral::UCChar* message)
	{
		CH_CORE_ERROR("[C#] {}", ch_u16_to_string(message));
	}
	void Application_Close()
	{
		Application::Get().Close();
	}
	int Application_GetFPS()
	{
		float dt = Application::Get().GetFrameTime();
		return dt > 0.0f ? (int)(1.0f / dt) : 0;
	}
	float Application_GetFrameTime()
	{
		return Application::Get().GetFrameTime();
	}
	void Window_SetSize(int w, int h)
	{
		Application::Get().GetWindow().SetSize(w, h);
	}
	void Window_SetFullscreen(uint8_t enabled)
	{
		Application::Get().GetWindow().SetFullscreen(enabled);
	}
	void Window_SetVSync(uint8_t enabled)
	{
		Application::Get().GetWindow().SetVSync(enabled);
	}
	void Window_SetAntialiasing(uint8_t enabled)
	{
		Application::Get().GetWindow().SetAntialiasing(enabled);
	}
	void Window_SetAntiAliasingSamples(int samples)
	{
		if (auto project = Project::GetActive())
		{
			project->GetConfig().Render.AntiAliasingSamples = samples;
		}
	}

	int Window_GetAntiAliasingSamples()
	{
		if (auto project = Project::GetActive())
		{
			return project->GetConfig().Render.AntiAliasingSamples;
		}
		return 4;
	}

	void Window_SetEnableShadows(uint8_t enabled)
	{
		if (auto project = Project::GetActive())
		{
			project->GetConfig().Render.EnableShadows = (enabled != 0);
		}
	}

	uint8_t Window_GetEnableShadows()
	{
		if (auto project = Project::GetActive())
		{
			return project->GetConfig().Render.EnableShadows ? 1 : 0;
		}
		return 1;
	}

	void Window_SetShadowResolution(int resolution)
	{
		if (auto project = Project::GetActive())
		{
			project->GetConfig().Render.ShadowResolution = resolution;
		}
	}

	int Window_GetShadowResolution()
	{
		if (auto project = Project::GetActive())
		{
			return project->GetConfig().Render.ShadowResolution;
		}
		return 2048;
	}

	uint8_t Window_GetVSync()
	{
		return Application::Get().GetWindow().GetVSync() ? 1 : 0;
	}

	uint8_t Window_GetFullscreen()
	{
		return Application::Get().GetWindow().IsFullscreen() ? 1 : 0;
	}

	int Window_GetWidth()
	{
		return Application::Get().GetWindow().GetWidth();
	}

	int Window_GetHeight()
	{
		return Application::Get().GetWindow().GetHeight();
	}

	float Audio_GetMasterVolume()
	{
		if (auto project = Project::GetActive())
		{
			return project->GetConfig().Audio.MasterVolume;
		}
		return 1.0f;
	}

	void Audio_SetMasterVolume(float volume)
	{
		if (auto project = Project::GetActive())
		{
			project->SetMasterVolume(volume);
		}
	}

	float Audio_GetMusicVolume()
	{
		if (auto project = Project::GetActive())
		{
			return project->GetConfig().Audio.MusicVolume;
		}
		return 1.0f;
	}

	void Audio_SetMusicVolume(float volume)
	{
		if (auto project = Project::GetActive())
		{
			project->SetMusicVolume(volume);
		}
	}

	float Audio_GetSFXVolume()
	{
		if (auto project = Project::GetActive())
		{
			return project->GetConfig().Audio.SFXVolume;
		}
		return 1.0f;
	}

	void Audio_SetSFXVolume(float volume)
	{
		if (auto project = Project::GetActive())
		{
			project->SetSFXVolume(volume);
		}
	}

	const Coral::UCChar* Window_GetSupportedResolution()
	{
		// The returned pointer is handed to managed code, so it must outlive this
		// call. Return a pointer into a persistent buffer, never .c_str() on a
		// temporary UCString (that would dangle the moment this function returns).
		static thread_local Coral::UCString s_ResolutionBuffer;

		GLFWwindow* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
		if (!window)
		{
			s_ResolutionBuffer = ToWide("");
			return s_ResolutionBuffer.c_str();
		}

		GLFWmonitor* monitor = glfwGetWindowMonitor(window);
		if (!monitor)
		{
			monitor = glfwGetPrimaryMonitor();
		}
		if (!monitor)
		{
			s_ResolutionBuffer = ToWide("");
			return s_ResolutionBuffer.c_str();
		}

		int count = 0;
		const GLFWvidmode* modes = glfwGetVideoModes(monitor, &count);

		std::set<std::pair<int, int>> seen;
		std::ostringstream oss;
		for (int i = 0; i < count; i++)
		{
			auto key = std::make_pair(modes[i].width, modes[i].height);
			if (seen.insert(key).second)
			{
				if (!oss.str().empty())
				{
					oss << ";";
				}
				oss << modes[i].width << "x" << modes[i].height;
			}
		}

		s_ResolutionBuffer = ToWide(oss.str());
		return s_ResolutionBuffer.c_str();
	}

	float Physics_GetGravity()
	{
		if (auto project = Project::GetActive())
		{
			return project->GetConfig().Physics.Gravity;
		}
		return 20.0f;
	}

	void Clipboard_SetText(const Coral::UCChar* text)
	{
		std::string utf8 = ch_u16_to_string(text);
		Application::Get().GetWindow().SetClipboardText(utf8);
	}

} // namespace Chained
