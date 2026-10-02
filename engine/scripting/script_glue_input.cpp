#include "script_glue_input.h"
#include "script_glue_registry.h"
#include "engine/core/input.h"
#include "engine/core/service_locator.h"

namespace Chained
{
	static void Input_GetMouseScroll(float* outX, float* outY)
	{
		if (auto* input = ServiceLocator::TryGet<Core::Input>())
		{
			auto val = input->GetMouseScroll();
			if (outX)
			{
				*outX = val.x;
			}
			if (outY)
			{
				*outY = val.y;
			}
		}
	}

	static void Input_GetMouseDelta(float* outX, float* outY)
	{
		if (auto* input = ServiceLocator::TryGet<Core::Input>())
		{
			auto val = input->GetMouseDelta();
			if (outX)
			{
				*outX = val.x;
			}
			if (outY)
			{
				*outY = val.y;
			}
		}
	}

	static void Input_GetMousePosition(float* outX, float* outY)
	{
		if (auto* input = ServiceLocator::TryGet<Core::Input>())
		{
			auto val = input->GetMousePosition();
			if (outX)
			{
				*outX = val.x;
			}
			if (outY)
			{
				*outY = val.y;
			}
		}
	}

	void ScriptGlue_RegisterInput(Coral::ManagedAssembly& assembly)
	{
		CH_BIND_SERVICE_METHOD(assembly, "Chained.Input", "Input_IsKeyDown_Ptr", &Core::Input::IsKeyDown);
		CH_BIND_SERVICE_METHOD(assembly, "Chained.Input", "Input_IsKeyPressed_Ptr", &Core::Input::IsKeyPressed);
		CH_BIND_SERVICE_METHOD(assembly, "Chained.Input", "Input_IsKeyReleased_Ptr", &Core::Input::IsKeyReleased);
		CH_BIND_SERVICE_METHOD(assembly, "Chained.Input", "Input_IsMouseButtonDown_Ptr",
							   &Core::Input::IsMouseButtonDown);
		CH_BIND_SERVICE_METHOD(assembly, "Chained.Input", "Input_IsMouseButtonPressed_Ptr",
							   &Core::Input::IsMouseButtonPressed);
		CH_BIND_SERVICE_METHOD(assembly, "Chained.Input", "Input_GetMouseWheelMove_Ptr",
							   &Core::Input::GetMouseWheelMove);
		CH_BIND_SERVICE_METHOD(assembly, "Chained.Input", "Input_GetMouseWheelHMove_Ptr",
							   &Core::Input::GetMouseWheelHMove);

		assembly.AddInternalCall("Chained.Input", "Input_GetMouseScroll_Ptr", (void*)&Input_GetMouseScroll);
		assembly.AddInternalCall("Chained.Input", "Input_GetMouseDelta_Ptr", (void*)&Input_GetMouseDelta);
		assembly.AddInternalCall("Chained.Input", "Input_GetMousePosition_Ptr", (void*)&Input_GetMousePosition);
	}
} // namespace Chained
