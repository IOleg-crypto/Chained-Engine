#include "input.h"

#include "engine/core/service_locator.h"

namespace Chained::Core
{
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
		m_KeyStates.fill(false);
		m_LastKeyStates.fill(false);
		m_MouseStates.fill(false);
		m_LastMouseStates.fill(false);
		m_MousePosition = {0.0f, 0.0f};
		m_LastMousePosition = {0.0f, 0.0f};
		m_MouseWheelAccumulator = 0.0f;
		m_CurrentMouseWheelDelta = 0.0f;
		m_MouseWheelHAccumulator = 0.0f;
		m_CurrentMouseWheelHDelta = 0.0f;
		m_FirstMouseUpdate = true;
	}

	void Input::Update(Timestep ts)
	{
		m_LastKeyStates = m_KeyStates;
		m_LastMouseStates = m_MouseStates;
		m_LastMousePosition = m_MousePosition;
		m_CurrentMouseWheelDelta = m_MouseWheelAccumulator;
		m_MouseWheelAccumulator = 0.0f;
		m_CurrentMouseWheelHDelta = m_MouseWheelHAccumulator;
		m_MouseWheelHAccumulator = 0.0f;
	}

	bool Input::IsKeyPressed(KeyCode key)
	{
		auto code = static_cast<size_t>(key);
		if (code >= m_KeyStates.size())
		{
			return false;
		}
		return m_KeyStates[code] && !m_LastKeyStates[code];
	}

	bool Input::IsKeyDown(KeyCode key)
	{
		auto code = static_cast<size_t>(key);
		if (code >= m_KeyStates.size())
		{
			return false;
		}
		return m_KeyStates[code];
	}

	bool Input::IsKeyReleased(KeyCode key)
	{
		auto code = static_cast<size_t>(key);
		if (code >= m_KeyStates.size())
		{
			return false;
		}
		return !m_KeyStates[code] && m_LastKeyStates[code];
	}

	bool Input::IsKeyUp(KeyCode key)
	{
		auto code = static_cast<size_t>(key);
		if (code >= m_KeyStates.size())
		{
			return true;
		}
		return !m_KeyStates[code];
	}

	bool Input::IsMouseButtonPressed(MouseCode button)
	{
		auto code = static_cast<size_t>(button);
		if (code >= m_MouseStates.size())
		{
			return false;
		}
		return m_MouseStates[code] && !m_LastMouseStates[code];
	}

	bool Input::IsMouseButtonDown(MouseCode button)
	{
		auto code = static_cast<size_t>(button);
		if (code >= m_MouseStates.size())
		{
			return false;
		}
		return m_MouseStates[code];
	}

	bool Input::IsMouseButtonReleased(MouseCode button)
	{
		auto code = static_cast<size_t>(button);
		if (code >= m_MouseStates.size())
		{
			return false;
		}
		return !m_MouseStates[code] && m_LastMouseStates[code];
	}

	bool Input::IsMouseButtonUp(MouseCode button)
	{
		auto code = static_cast<size_t>(button);
		if (code >= m_MouseStates.size())
		{
			return true;
		}
		return !m_MouseStates[code];
	}

	glm::vec2 Input::GetMousePosition()
	{
		return m_MousePosition;
	}

	glm::vec2 Input::GetMouseDelta()
	{
		if (m_FirstMouseUpdate)
		{
			return {0.0f, 0.0f};
		}
		return m_MousePosition - m_LastMousePosition;
	}

	float Input::GetMouseWheelMove()
	{
		return m_CurrentMouseWheelDelta;
	}

	float Input::GetMouseWheelHMove()
	{
		return m_CurrentMouseWheelHDelta;
	}

	glm::vec2 Input::GetMouseScroll()
	{
		return glm::vec2(m_CurrentMouseWheelHDelta, m_CurrentMouseWheelDelta);
	}

	void Input::OnKey(KeyCode key, bool pressed)
	{
		auto code = static_cast<size_t>(key);
		if (code < m_KeyStates.size())
		{
			m_KeyStates[code] = pressed;
		}
	}

	void Input::OnMouseButton(MouseCode button, bool pressed)
	{
		auto code = static_cast<size_t>(button);
		if (code < m_MouseStates.size())
		{
			m_MouseStates[code] = pressed;
		}
	}

	void Input::OnMouseMove(float x, float y)
	{
		if (m_FirstMouseUpdate)
		{
			m_LastMousePosition = {x, y};
			m_FirstMouseUpdate = false;
		}
		m_MousePosition = {x, y};
	}

	void Input::OnMouseScroll(float xOffset, float yOffset)
	{
		m_MouseWheelAccumulator += yOffset;
		m_MouseWheelHAccumulator += xOffset;
	}

} // namespace Chained::Core
