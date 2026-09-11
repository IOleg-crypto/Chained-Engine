#include "input.h"

#include "engine/core/service_locator.h"

namespace Chained::Core
{
	Input* Input::GetInstance()
	{
		return ServiceLocator::Get<Core::Input>();
	}

	Input::Input() = default;

	Input::~Input() = default;

	void Input::Initialize()
	{
		ResetAll();
	}

	void Input::Shutdown()
	{
		ResetAll();
	}

	void Input::ResetAll()
	{
		auto* inst = GetInstance();
		if (!inst)
		{
			return;
		}
		inst->m_KeyStates.fill(false);
		inst->m_LastKeyStates.fill(false);
		inst->m_MouseStates.fill(false);
		inst->m_LastMouseStates.fill(false);
		inst->m_MousePosition = {0.0f, 0.0f};
		inst->m_LastMousePosition = {0.0f, 0.0f};
		inst->m_MouseWheelAccumulator = 0.0f;
		inst->m_CurrentMouseWheelDelta = 0.0f;
		inst->m_FirstMouseUpdate = true;
	}

	void Input::Update(Timestep ts)
	{
		auto* inst = GetInstance();
		if (!inst)
		{
			return;
		}
		inst->m_LastKeyStates = inst->m_KeyStates;
		inst->m_LastMouseStates = inst->m_MouseStates;
		inst->m_LastMousePosition = inst->m_MousePosition;
		inst->m_CurrentMouseWheelDelta = inst->m_MouseWheelAccumulator;
		inst->m_MouseWheelAccumulator = 0.0f;
	}

	bool Input::IsKeyPressed(KeyCode key)
	{
		auto* inst = GetInstance();
		if (!inst)
		{
			return false;
		}
		auto code = static_cast<size_t>(key);
		if (code >= inst->m_KeyStates.size())
		{
			return false;
		}
		return inst->m_KeyStates[code] && !inst->m_LastKeyStates[code];
	}

	bool Input::IsKeyDown(KeyCode key)
	{
		auto* inst = GetInstance();
		if (!inst)
		{
			return false;
		}
		auto code = static_cast<size_t>(key);
		if (code >= inst->m_KeyStates.size())
		{
			return false;
		}
		return inst->m_KeyStates[code];
	}

	bool Input::IsKeyReleased(KeyCode key)
	{
		auto* inst = GetInstance();
		if (!inst)
		{
			return false;
		}
		auto code = static_cast<size_t>(key);
		if (code >= inst->m_KeyStates.size())
		{
			return false;
		}
		return !inst->m_KeyStates[code] && inst->m_LastKeyStates[code];
	}

	bool Input::IsKeyUp(KeyCode key)
	{
		auto* inst = GetInstance();
		if (!inst)
		{
			return true;
		}
		auto code = static_cast<size_t>(key);
		if (code >= inst->m_KeyStates.size())
		{
			return true;
		}
		return !inst->m_KeyStates[code];
	}

	bool Input::IsMouseButtonPressed(MouseCode button)
	{
		auto* inst = GetInstance();
		if (!inst)
		{
			return false;
		}
		auto code = static_cast<size_t>(button);
		if (code >= inst->m_MouseStates.size())
		{
			return false;
		}
		return inst->m_MouseStates[code] && !inst->m_LastMouseStates[code];
	}

	bool Input::IsMouseButtonDown(MouseCode button)
	{
		auto* inst = GetInstance();
		if (!inst)
		{
			return false;
		}
		auto code = static_cast<size_t>(button);
		if (code >= inst->m_MouseStates.size())
		{
			return false;
		}
		return inst->m_MouseStates[code];
	}

	bool Input::IsMouseButtonReleased(MouseCode button)
	{
		auto* inst = GetInstance();
		if (!inst)
		{
			return false;
		}
		auto code = static_cast<size_t>(button);
		if (code >= inst->m_MouseStates.size())
		{
			return false;
		}
		return !inst->m_MouseStates[code] && inst->m_LastMouseStates[code];
	}

	bool Input::IsMouseButtonUp(MouseCode button)
	{
		auto* inst = GetInstance();
		if (!inst)
		{
			return true;
		}
		auto code = static_cast<size_t>(button);
		if (code >= inst->m_MouseStates.size())
		{
			return true;
		}
		return !inst->m_MouseStates[code];
	}

	glm::vec2 Input::GetMousePosition()
	{
		auto* inst = GetInstance();
		return inst ? inst->m_MousePosition : glm::vec2{0.0f, 0.0f};
	}

	glm::vec2 Input::GetMouseDelta()
	{
		auto* inst = GetInstance();
		if (!inst)
		{
			return {0.0f, 0.0f};
		}
		if (inst->m_FirstMouseUpdate)
		{
			return {0.0f, 0.0f};
		}
		return inst->m_MousePosition - inst->m_LastMousePosition;
	}

	float Input::GetMouseWheelMove()
	{
		auto* inst = GetInstance();
		return inst ? inst->m_CurrentMouseWheelDelta : 0.0f;
	}

	void Input::OnKey(KeyCode key, bool pressed)
	{
		auto* inst = GetInstance();
		if (!inst)
		{
			return;
		}
		auto code = static_cast<size_t>(key);
		if (code < inst->m_KeyStates.size())
		{
			inst->m_KeyStates[code] = pressed;
		}
	}

	void Input::OnMouseButton(MouseCode button, bool pressed)
	{
		auto* inst = GetInstance();
		if (!inst)
		{
			return;
		}
		auto code = static_cast<size_t>(button);
		if (code < inst->m_MouseStates.size())
		{
			inst->m_MouseStates[code] = pressed;
		}
	}

	void Input::OnMouseMove(float x, float y)
	{
		auto* inst = GetInstance();
		if (!inst)
		{
			return;
		}
		if (inst->m_FirstMouseUpdate)
		{
			inst->m_LastMousePosition = {x, y};
			inst->m_FirstMouseUpdate = false;
		}
		inst->m_MousePosition = {x, y};
	}

	void Input::OnMouseScroll(float xOffset, float yOffset)
	{
		auto* inst = GetInstance();
		if (!inst)
		{
			return;
		}
		inst->m_MouseWheelAccumulator += yOffset;
	}

} // namespace Chained::Core
