#ifndef SCRIPT_GLUE_SYSTEM_H
#define SCRIPT_GLUE_SYSTEM_H
#include "script_glue_internal.h"
#include "engine/app/application.h"
#include "engine/scene/scene.h"
#include "engine/scripting/scriptengine.h"

namespace Chained
{

	// ── Logging ──────────────────────────────────────────────────────────
	CH_SCRIPT_FUNC void Log_Info(const Coral::UCChar* message);

	CH_SCRIPT_FUNC void Log_Warn(const Coral::UCChar* message);

	CH_SCRIPT_FUNC void Log_Error(const Coral::UCChar* message);

	// ── Application / Window ─────────────────────────────────────────────
	CH_SCRIPT_FUNC void Application_Close();

	CH_SCRIPT_FUNC int Application_GetFPS();

	CH_SCRIPT_FUNC float Application_GetFrameTime();

	CH_SCRIPT_FUNC void Window_SetSize(int w, int h);

	CH_SCRIPT_FUNC void Window_SetFullscreen(uint8_t enabled);

	CH_SCRIPT_FUNC void Window_SetVSync(uint8_t enabled);

	CH_SCRIPT_FUNC void Window_SetAntialiasing(uint8_t enabled);

	CH_SCRIPT_FUNC void Window_SetAntiAliasingSamples(int samples);

	CH_SCRIPT_FUNC int Window_GetAntiAliasingSamples();

	CH_SCRIPT_FUNC void Window_SetEnableShadows(uint8_t enabled);

	CH_SCRIPT_FUNC uint8_t Window_GetEnableShadows();

	CH_SCRIPT_FUNC void Window_SetShadowResolution(int resolution);

	CH_SCRIPT_FUNC int Window_GetShadowResolution();

	CH_SCRIPT_FUNC uint8_t Window_GetVSync();

	CH_SCRIPT_FUNC uint8_t Window_GetFullscreen();

	CH_SCRIPT_FUNC int Window_GetWidth();

	CH_SCRIPT_FUNC int Window_GetHeight();

	CH_SCRIPT_FUNC const Coral::UCChar* Window_GetSupportedResolution();

	// ── Audio (global volume) ─────────────────────────────────────────
	CH_SCRIPT_FUNC float Audio_GetMasterVolume();

	CH_SCRIPT_FUNC void Audio_SetMasterVolume(float volume);

	CH_SCRIPT_FUNC float Audio_GetMusicVolume();

	CH_SCRIPT_FUNC void Audio_SetMusicVolume(float volume);

	CH_SCRIPT_FUNC float Audio_GetSFXVolume();

	CH_SCRIPT_FUNC void Audio_SetSFXVolume(float volume);

	// ── Physics ────────────────────────────────────────────────────────
	CH_SCRIPT_FUNC float Physics_GetGravity();

	// ── Clipboard ─────────────────────────────────────────────────────
	CH_SCRIPT_FUNC void Clipboard_SetText(const Coral::UCChar* text);

} // namespace Chained
#endif
