#ifndef CH_INPUT_H
#define CH_INPUT_H

#include "engine/common/base.h"
#include "engine/common/timestep.h"
#include "engine/core/service.h"
#include "key_codes.h"
#include "mouse_codes.h"
#include <array>
#include <glm/vec2.hpp>

namespace Chained::Core
{
	class CH_API Input : public Service
	{
	public:
		Input();
		~Input() override;

		void Initialize() override;
		void Shutdown() override;

		// Static API — primary interface for engine-wide access
		void Update(Timestep ts);
		void ResetAll();

		bool IsKeyPressed(KeyCode key);
		bool IsKeyDown(KeyCode key);
		bool IsKeyReleased(KeyCode key);
		bool IsKeyUp(KeyCode key);

		bool IsMouseButtonPressed(MouseCode button);
		bool IsMouseButtonDown(MouseCode button);
		bool IsMouseButtonReleased(MouseCode button);
		bool IsMouseButtonUp(MouseCode button);

		glm::vec2 GetMousePosition();
		glm::vec2 GetMouseDelta();
		float GetMouseWheelMove();
		float GetMouseWheelHMove();
		glm::vec2 GetMouseScroll();

		void OnKey(KeyCode key, bool pressed);
		void OnMouseButton(MouseCode button, bool pressed);
		void OnMouseMove(float x, float y);
		void OnMouseScroll(float xOffset, float yOffset);

	private:
		std::array<bool, 512> m_KeyStates{};
		std::array<bool, 512> m_LastKeyStates{};
		std::array<bool, 16> m_MouseStates{};
		std::array<bool, 16> m_LastMouseStates{};

		glm::vec2 m_MousePosition{0.0f, 0.0f};
		glm::vec2 m_LastMousePosition{0.0f, 0.0f};
		float m_MouseWheelAccumulator = 0.0f;
		float m_CurrentMouseWheelDelta = 0.0f;
		float m_MouseWheelHAccumulator = 0.0f;
		float m_CurrentMouseWheelHDelta = 0.0f;
		bool m_FirstMouseUpdate = true;
	};
} // namespace Chained::Core

#endif // CH_INPUT_H
