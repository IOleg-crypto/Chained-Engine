// script_interop_pointers.h
// C++ → C# callback pointers. Set once during assembly load via the
// ScriptGlue_Register*Callback internal calls (see script_glue.cpp).
// C# registers each callback directly — no struct, no round-trip.
#ifndef CH_SCRIPT_INTEROP_POINTERS_H
#define CH_SCRIPT_INTEROP_POINTERS_H

#include <cstdint>

namespace Chained
{
	// Each pointer is filled by its matching ScriptGlue_Register*Callback internal call.
	extern void (*g_ScriptOnUpdate)(float);
	extern void (*g_ScriptOnEvent)(int);
	extern void (*g_ScriptOnRenderUI)();
	extern void (*g_ScriptOnCollisionEnter)(uint64_t, uint64_t);
	extern void (*g_ScriptClearAll)();
	extern uint8_t (*g_ScriptInstantiate)(uint64_t, const char16_t*);
	extern void (*g_ScriptDestroy)(uint64_t, const char16_t*);
	extern void (*g_ScriptSetSceneEpoch)(uint64_t);

} // namespace Chained

#endif // CH_SCRIPT_INTEROP_POINTERS_H
