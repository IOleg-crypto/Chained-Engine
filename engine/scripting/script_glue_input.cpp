#include "script_glue_input.h"

namespace Chained
{
	void Input_GetMouseDelta(float* outX, float* outY)
	{
		glm::vec2 delta = Core::Input::GetMouseDelta();
		if (outX)
		{
			*outX = delta.x;
		}
		if (outY)
		{
			*outY = delta.y;
		}
	}
	float Input_GetMouseWheelMove()
	{
		if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse)
		{
			return 0.0f;
		}
		return Core::Input::GetMouseWheelMove();
	}
	float Input_GetMouseWheelHMove()
	{
		if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse)
		{
			return 0.0f;
		}
		return Core::Input::GetMouseWheelHMove();
	}
	void Input_GetMouseScroll(float* outX, float* outY)
	{
		if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse)
		{
			if (outX)
			{
				*outX = 0.0f;
			}
			if (outY)
			{
				*outY = 0.0f;
			}
			return;
		}
		glm::vec2 scroll = Core::Input::GetMouseScroll();
		if (outX)
		{
			*outX = scroll.x;
		}
		if (outY)
		{
			*outY = scroll.y;
		}
	}
	int Input_IsMouseButtonPressed(int button)
	{
		if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse)
		{
			return 0;
		}
		return Core::Input::IsMouseButtonPressed(static_cast<MouseCode>(button)) ? 1 : 0;
	}
	int Input_IsMouseButtonDown(int button)
	{
		if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse)
		{
			return 0;
		}
		return Core::Input::IsMouseButtonDown(static_cast<MouseCode>(button)) ? 1 : 0;
	}
	int Input_IsKeyReleased(int keyCode)
	{
		return Core::Input::IsKeyReleased(static_cast<KeyCode>(keyCode)) ? 1 : 0;
	}
	int Input_IsKeyPressed(int keyCode)
	{
		if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard)
		{
			return 0;
		}
		return Core::Input::IsKeyPressed(static_cast<KeyCode>(keyCode)) ? 1 : 0;
	}
	int Input_IsKeyDown(int keyCode)
	{
		if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard)
		{
			return 0;
		}
		return Core::Input::IsKeyDown(static_cast<KeyCode>(keyCode)) ? 1 : 0;
	}

} // namespace Chained