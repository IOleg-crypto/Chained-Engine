#ifndef SCRIPT_GLUE_INPUT_H
#define SCRIPT_GLUE_INPUT_H
#include "engine/core/input.h"
#include "script_glue_internal.h"

#include <cstdint>

namespace Chained
{

	CH_SCRIPT_FUNC int Input_IsKeyDown(int keyCode);

	CH_SCRIPT_FUNC int Input_IsKeyPressed(int keyCode);

	CH_SCRIPT_FUNC int Input_IsKeyReleased(int keyCode);

	CH_SCRIPT_FUNC int Input_IsMouseButtonDown(int button);

	CH_SCRIPT_FUNC int Input_IsMouseButtonPressed(int button);

	CH_SCRIPT_FUNC void Input_GetMouseDelta(float* outX, float* outY);

	CH_SCRIPT_FUNC float Input_GetMouseWheelMove();

	CH_SCRIPT_FUNC float Input_GetMouseWheelHMove();

	CH_SCRIPT_FUNC void Input_GetMouseScroll(float* outX, float* outY);

} // namespace Chained
#endif