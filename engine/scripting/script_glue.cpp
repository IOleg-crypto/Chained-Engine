// script_glue.cpp
// Registers every C++ function as a Coral internal call.
// Coral automatically fills the matching delegate* fields on the C# side.
// Adding a new function: 1) declare in script_glue_*.h, 2) implement in script_glue_*.cpp, 3) add one AddInternalCall
// line here.
//
// ABI conventions (kept in sync with scripting/managed/src):
//  - Strings are UTF-16 (Coral::UCChar == wchar_t on Windows). C# exposes them as `char*`
//    (also 2 bytes), so no marshaling mismatch.
//  - Booleans crossing the boundary use uint8_t (C++) / byte (C#), never `bool`, to avoid
//    any reliance on marshaling semantics. Never return structs by value — use out-pointers.
#include "script_glue.h"
#include "script_interop_pointers.h"

#include "script_glue_internal.h"
#include "script_glue_system.h"
#include "script_glue_scene.h"
#include "script_glue_entity.h"
#include "script_glue_camera.h"
#include "script_glue_ui.h"
#include "script_glue_audio.h"
#include "script_glue_input.h"
#include "script_glue_network.h"
#include "generated/script_glue_generated.h"
#include <Coral/Assembly.hpp>

namespace Chained
{

	// ── C++ → C# callback pointers (filled by ScriptGlue_Register*Callback) ──
	void (*g_ScriptOnUpdate)(float) = nullptr;
	void (*g_ScriptOnEvent)(int) = nullptr;
	void (*g_ScriptOnRenderUI)() = nullptr;
	void (*g_ScriptOnCollisionEnter)(uint64_t, uint64_t) = nullptr;
	void (*g_ScriptClearAll)() = nullptr;
	uint8_t (*g_ScriptInstantiate)(uint64_t, const char16_t*) = nullptr;
	void (*g_ScriptDestroy)(uint64_t, const char16_t*) = nullptr;

	// C# registers each lifecycle callback directly — no struct, no round-trip.
	static void ScriptGlue_RegisterUpdateCallback(void (*cb)(float))
	{
		g_ScriptOnUpdate = cb;
	}
	static void ScriptGlue_RegisterEventCallback(void (*cb)(int))
	{
		g_ScriptOnEvent = cb;
	}
	static void ScriptGlue_RegisterRenderUICallback(void (*cb)())
	{
		g_ScriptOnRenderUI = cb;
	}
	static void ScriptGlue_RegisterCollisionCallback(void (*cb)(uint64_t, uint64_t))
	{
		g_ScriptOnCollisionEnter = cb;
	}
	static void ScriptGlue_RegisterClearAllCallback(void (*cb)())
	{
		g_ScriptClearAll = cb;
	}
	static void ScriptGlue_RegisterInstantiateCallback(uint8_t (*cb)(uint64_t, const char16_t*))
	{
		g_ScriptInstantiate = cb;
	}
	static void ScriptGlue_RegisterDestroyCallback(void (*cb)(uint64_t, const char16_t*))
	{
		g_ScriptDestroy = cb;
	}

	void ScriptGlue::RegisterInternalCalls(Coral::ManagedAssembly& assembly)
	{
		// ── C# → C++ lifecycle callbacks (registered directly, one per callback) ──
		assembly.AddInternalCall("Chained.Interop", "ScriptGlue_RegisterUpdateCallback_Ptr",
								 (void*)&ScriptGlue_RegisterUpdateCallback);
		assembly.AddInternalCall("Chained.Interop", "ScriptGlue_RegisterEventCallback_Ptr",
								 (void*)&ScriptGlue_RegisterEventCallback);
		assembly.AddInternalCall("Chained.Interop", "ScriptGlue_RegisterRenderUICallback_Ptr",
								 (void*)&ScriptGlue_RegisterRenderUICallback);
		assembly.AddInternalCall("Chained.Interop", "ScriptGlue_RegisterCollisionCallback_Ptr",
								 (void*)&ScriptGlue_RegisterCollisionCallback);
		assembly.AddInternalCall("Chained.Interop", "ScriptGlue_RegisterClearAllCallback_Ptr",
								 (void*)&ScriptGlue_RegisterClearAllCallback);
		assembly.AddInternalCall("Chained.Interop", "ScriptGlue_RegisterInstantiateCallback_Ptr",
								 (void*)&ScriptGlue_RegisterInstantiateCallback);
		assembly.AddInternalCall("Chained.Interop", "ScriptGlue_RegisterDestroyCallback_Ptr",
								 (void*)&ScriptGlue_RegisterDestroyCallback);

		// ── Entity ────────────────────────────────────────────────────────
		assembly.AddInternalCall("Chained.Entity", "Entity_HasComponent_Ptr", (void*)&Entity_HasComponent);
		assembly.AddInternalCall("Chained.Entity", "Entity_FindAllWithComponent_Ptr",
								 (void*)&Entity_FindAllWithComponent);
		assembly.AddInternalCall("Chained.Entity", "Entity_AddComponent_Ptr", (void*)&Entity_AddComponent);

		// ── Transform ─────────────────────────────────────────────────────
		assembly.AddInternalCall("Chained.TransformComponent", "Transform_GetTranslation_Ptr",
								 (void*)&Transform_GetTranslation);
		assembly.AddInternalCall("Chained.TransformComponent", "Transform_SetTranslation_Ptr",
								 (void*)&Transform_SetTranslation);
		assembly.AddInternalCall("Chained.TransformComponent", "Transform_GetRotation_Ptr",
								 (void*)&Transform_GetRotation);
		assembly.AddInternalCall("Chained.TransformComponent", "Transform_SetRotation_Ptr",
								 (void*)&Transform_SetRotation);
		assembly.AddInternalCall("Chained.TransformComponent", "Transform_GetScale_Ptr", (void*)&Transform_GetScale);
		assembly.AddInternalCall("Chained.TransformComponent", "Transform_SetScale_Ptr", (void*)&Transform_SetScale);

		// ── Model ─────────────────────────────────────────────────────────
		assembly.AddInternalCall("Chained.ModelComponent", "Model_GetModelPath_Ptr", (void*)&Model_GetModelPath);
		assembly.AddInternalCall("Chained.ModelComponent", "Model_SetModelPath_Ptr", (void*)&Model_SetModelPath);

		// ── RigidBody ─────────────────────────────────────────────────────
		assembly.AddInternalCall("Chained.RigidBodyComponent", "RigidBody_GetVelocity_Ptr",
								 (void*)&RigidBody_GetVelocity);
		assembly.AddInternalCall("Chained.RigidBodyComponent", "RigidBody_SetVelocity_Ptr",
								 (void*)&RigidBody_SetVelocity);
		assembly.AddInternalCall("Chained.RigidBodyComponent", "RigidBody_ForceSetVelocity_Ptr",
								 (void*)&RigidBody_ForceSetVelocity);
		assembly.AddInternalCall("Chained.RigidBodyComponent", "RigidBody_IsGrounded_Ptr",
								 (void*)&RigidBody_IsGrounded);
		assembly.AddInternalCall("Chained.RigidBodyComponent", "RigidBody_IsKinematic_Ptr",
								 (void*)&RigidBody_IsKinematic);
		assembly.AddInternalCall("Chained.RigidBodyComponent", "RigidBody_SetKinematic_Ptr",
								 (void*)&RigidBody_SetKinematic);

		// ── Tag ───────────────────────────────────────────────────────────
		assembly.AddInternalCall("Chained.TagComponent", "TagComponent_GetTag_Ptr", (void*)&TagComponent_GetTag);

		// ── Camera ────────────────────────────────────────────────────────
		assembly.AddInternalCall("Chained.CameraComponent", "Camera_GetForward_Ptr", (void*)&Camera_GetForward);
		assembly.AddInternalCall("Chained.CameraComponent", "Camera_GetRight_Ptr", (void*)&Camera_GetRight);
		assembly.AddInternalCall("Chained.CameraComponent", "Camera_GetOrbit_Ptr", (void*)&Camera_GetOrbit);
		assembly.AddInternalCall("Chained.CameraComponent", "Camera_SetOrbit_Ptr", (void*)&Camera_SetOrbit);
		assembly.AddInternalCall("Chained.CameraComponent", "Camera_GetPrimary_Ptr", (void*)&Camera_GetPrimary);
		assembly.AddInternalCall("Chained.CameraComponent", "Camera_SetPrimary_Ptr", (void*)&Camera_SetPrimary);
		assembly.AddInternalCall("Chained.CameraComponent", "Camera_GetIsOrbit_Ptr", (void*)&Camera_GetIsOrbit);
		assembly.AddInternalCall("Chained.CameraComponent", "Camera_SetIsOrbit_Ptr", (void*)&Camera_SetIsOrbit);
		assembly.AddInternalCall("Chained.CameraComponent", "Camera_GetTargetTag_Ptr", (void*)&Camera_GetTargetTag);
		assembly.AddInternalCall("Chained.CameraComponent", "Camera_SetTargetTag_Ptr", (void*)&Camera_SetTargetTag);

		// ── AudioComponent ────────────────────────────────────────────────
		assembly.AddInternalCall("Chained.AudioComponent", "AudioComponent_SetVolume_Ptr",
								 (void*)&AudioComponent_SetVolume);
		assembly.AddInternalCall("Chained.AudioComponent", "AudioComponent_SetLoop_Ptr",
								 (void*)&AudioComponent_SetLoop);
		assembly.AddInternalCall("Chained.AudioComponent", "AudioComponent_IsPlaying_Ptr",
								 (void*)&AudioComponent_IsPlaying);
		assembly.AddInternalCall("Chained.AudioComponent", "AudioComponent_GetSoundPath_Ptr",
								 (void*)&AudioComponent_GetSoundPath);
		assembly.AddInternalCall("Chained.AudioComponent", "AudioComponent_Play_Ptr", (void*)&AudioComponent_Play);
		assembly.AddInternalCall("Chained.AudioComponent", "AudioComponent_Stop_Ptr", (void*)&AudioComponent_Stop);

		// ── SpriteComponent ───────────────────────────────────────────────
		assembly.AddInternalCall("Chained.SpriteComponent", "SpriteComponent_GetTexturePath_Ptr",
								 (void*)&SpriteComponent_GetTexturePath);
		assembly.AddInternalCall("Chained.SpriteComponent", "SpriteComponent_SetTexturePath_Ptr",
								 (void*)&SpriteComponent_SetTexturePath);
		assembly.AddInternalCall("Chained.SpriteComponent", "SpriteComponent_GetTint_Ptr",
								 (void*)&SpriteComponent_GetTint);
		assembly.AddInternalCall("Chained.SpriteComponent", "SpriteComponent_SetTint_Ptr",
								 (void*)&SpriteComponent_SetTint);
		assembly.AddInternalCall("Chained.SpriteComponent", "SpriteComponent_GetFlipX_Ptr",
								 (void*)&SpriteComponent_GetFlipX);
		assembly.AddInternalCall("Chained.SpriteComponent", "SpriteComponent_SetFlipX_Ptr",
								 (void*)&SpriteComponent_SetFlipX);
		assembly.AddInternalCall("Chained.SpriteComponent", "SpriteComponent_GetFlipY_Ptr",
								 (void*)&SpriteComponent_GetFlipY);
		assembly.AddInternalCall("Chained.SpriteComponent", "SpriteComponent_SetFlipY_Ptr",
								 (void*)&SpriteComponent_SetFlipY);
		assembly.AddInternalCall("Chained.SpriteComponent", "SpriteComponent_GetZOrder_Ptr",
								 (void*)&SpriteComponent_GetZOrder);
		assembly.AddInternalCall("Chained.SpriteComponent", "SpriteComponent_SetZOrder_Ptr",
								 (void*)&SpriteComponent_SetZOrder);

		// ── UI Controls ───────────────────────────────────────────────────
		assembly.AddInternalCall("Chained.ButtonControl", "ButtonControl_IsClicked_Ptr",
								 (void*)&ButtonControl_IsClicked);
		assembly.AddInternalCall("Chained.ButtonControl", "ButtonControl_IsDown_Ptr", (void*)&ButtonControl_IsDown);
		assembly.AddInternalCall("Chained.ButtonControl", "ButtonControl_GetLabel_Ptr", (void*)&ButtonControl_GetLabel);
		assembly.AddInternalCall("Chained.ButtonControl", "ButtonControl_SetLabel_Ptr", (void*)&ButtonControl_SetLabel);

		assembly.AddInternalCall("Chained.CheckboxControl", "CheckboxControl_GetChecked_Ptr",
								 (void*)&CheckboxControl_GetChecked);
		assembly.AddInternalCall("Chained.CheckboxControl", "CheckboxControl_SetChecked_Ptr",
								 (void*)&CheckboxControl_SetChecked);

		assembly.AddInternalCall("Chained.LabelControl", "LabelControl_GetText_Ptr", (void*)&LabelControl_GetText);
		assembly.AddInternalCall("Chained.LabelControl", "LabelControl_SetText_Ptr", (void*)&LabelControl_SetText);

		assembly.AddInternalCall("Chained.SliderControl", "SliderControl_GetLabel_Ptr", (void*)&SliderControl_GetLabel);
		assembly.AddInternalCall("Chained.SliderControl", "SliderControl_SetLabel_Ptr", (void*)&SliderControl_SetLabel);
		assembly.AddInternalCall("Chained.SliderControl", "SliderControl_GetValue_Ptr", (void*)&SliderControl_GetValue);
		assembly.AddInternalCall("Chained.SliderControl", "SliderControl_SetValue_Ptr", (void*)&SliderControl_SetValue);
		assembly.AddInternalCall("Chained.SliderControl", "SliderControl_GetMin_Ptr", (void*)&SliderControl_GetMin);
		assembly.AddInternalCall("Chained.SliderControl", "SliderControl_SetMin_Ptr", (void*)&SliderControl_SetMin);
		assembly.AddInternalCall("Chained.SliderControl", "SliderControl_GetMax_Ptr", (void*)&SliderControl_GetMax);
		assembly.AddInternalCall("Chained.SliderControl", "SliderControl_SetMax_Ptr", (void*)&SliderControl_SetMax);

		assembly.AddInternalCall("Chained.ProgressBarControl", "ProgressBarControl_GetProgress_Ptr",
								 (void*)&ProgressBarControl_GetProgress);
		assembly.AddInternalCall("Chained.ProgressBarControl", "ProgressBarControl_SetProgress_Ptr",
								 (void*)&ProgressBarControl_SetProgress);
		assembly.AddInternalCall("Chained.ProgressBarControl", "ProgressBarControl_GetOverlayText_Ptr",
								 (void*)&ProgressBarControl_GetOverlayText);
		assembly.AddInternalCall("Chained.ProgressBarControl", "ProgressBarControl_SetOverlayText_Ptr",
								 (void*)&ProgressBarControl_SetOverlayText);
		assembly.AddInternalCall("Chained.ProgressBarControl", "ProgressBarControl_GetShowPercentage_Ptr",
								 (void*)&ProgressBarControl_GetShowPercentage);
		assembly.AddInternalCall("Chained.ProgressBarControl", "ProgressBarControl_SetShowPercentage_Ptr",
								 (void*)&ProgressBarControl_SetShowPercentage);

		assembly.AddInternalCall("Chained.WidgetControl", "WidgetControl_GetActive_Ptr",
								 (void*)&WidgetControl_GetActive);
		assembly.AddInternalCall("Chained.WidgetControl", "WidgetControl_SetActive_Ptr",
								 (void*)&WidgetControl_SetActive);
		assembly.AddInternalCall("Chained.WidgetControl", "WidgetControl_GetTextColor_Ptr",
								 (void*)&WidgetControl_GetTextColor);
		assembly.AddInternalCall("Chained.WidgetControl", "WidgetControl_SetTextColorRGBA_Ptr",
								 (void*)&WidgetControl_SetTextColorRGBA);

		assembly.AddInternalCall("Chained.ComboBoxControl", "ComboBoxControl_GetSelectedIndex_Ptr",
								 (void*)&ComboBoxControl_GetSelectedIndex);
		assembly.AddInternalCall("Chained.ComboBoxControl", "ComboBoxControl_SetSelectedIndex_Ptr",
								 (void*)&ComboBoxControl_SetSelectedIndex);
		assembly.AddInternalCall("Chained.ComboBoxControl", "ComboBoxControl_AddItem_Ptr",
								 (void*)&ComboBoxControl_AddItem);
		assembly.AddInternalCall("Chained.ComboBoxControl", "ComboBoxControl_ClearItems_Ptr",
								 (void*)&ComboBoxControl_ClearItems);
		assembly.AddInternalCall("Chained.ComboBoxControl", "ComboBoxControl_GetItemCount_Ptr",
								 (void*)&ComboBoxControl_GetItemCount);
		assembly.AddInternalCall("Chained.ComboBoxControl", "ComboBoxControl_GetItem_Ptr",
								 (void*)&ComboBoxControl_GetItem);

		// ── InputTextControl ──────────────────────────────────────────────
		assembly.AddInternalCall("Chained.InputTextControl", "InputTextControl_GetText_Ptr",
								 (void*)&InputTextControl_GetText);
		assembly.AddInternalCall("Chained.InputTextControl", "InputTextControl_SetText_Ptr",
								 (void*)&InputTextControl_SetText);
		assembly.AddInternalCall("Chained.InputTextControl", "InputTextControl_HasChanged_Ptr",
								 (void*)&InputTextControl_HasChanged);
		assembly.AddInternalCall("Chained.InputTextControl", "InputTextControl_ClearChanged_Ptr",
								 (void*)&InputTextControl_ClearChanged);

		// ── AnimationComponent ──────────────────────────────────────────────────
		assembly.AddInternalCall("Chained.AnimationComponent", "AnimationComponent_GetCurrentAnimationIndex_Ptr",
								 (void*)&AnimationComponent_GetCurrentAnimationIndex);
		assembly.AddInternalCall("Chained.AnimationComponent", "AnimationComponent_SetCurrentAnimationIndex_Ptr",
								 (void*)&AnimationComponent_SetCurrentAnimationIndex);
		assembly.AddInternalCall("Chained.AnimationComponent", "AnimationComponent_GetIsPlaying_Ptr",
								 (void*)&AnimationComponent_GetIsPlaying);
		assembly.AddInternalCall("Chained.AnimationComponent", "AnimationComponent_SetIsPlaying_Ptr",
								 (void*)&AnimationComponent_SetIsPlaying);
		assembly.AddInternalCall("Chained.AnimationComponent", "AnimationComponent_GetIsLooping_Ptr",
								 (void*)&AnimationComponent_GetIsLooping);
		assembly.AddInternalCall("Chained.AnimationComponent", "AnimationComponent_SetIsLooping_Ptr",
								 (void*)&AnimationComponent_SetIsLooping);
		assembly.AddInternalCall("Chained.AnimationComponent", "AnimationComponent_GetIsFinished_Ptr",
								 (void*)&AnimationComponent_GetIsFinished);
		assembly.AddInternalCall("Chained.AnimationComponent", "AnimationComponent_GetDuration_Ptr",
								 (void*)&AnimationComponent_GetDuration);
		assembly.AddInternalCall("Chained.AnimationComponent", "AnimationComponent_GetNormalizedTime_Ptr",
								 (void*)&AnimationComponent_GetNormalizedTime);
		assembly.AddInternalCall("Chained.AnimationComponent", "AnimationComponent_GetBlendDuration_Ptr",
								 (void*)&AnimationComponent_GetBlendDuration);
		assembly.AddInternalCall("Chained.AnimationComponent", "AnimationComponent_SetBlendDuration_Ptr",
								 (void*)&AnimationComponent_SetBlendDuration);
		assembly.AddInternalCall("Chained.AnimationComponent", "AnimationComponent_CrossFade_Ptr",
								 (void*)&AnimationComponent_CrossFade);

		// ── AnimationComponent graph variables ──────────────────────────────
		assembly.AddInternalCall("Chained.AnimationComponent", "AnimationComponent_SetFloat_Ptr",
								 (void*)&AnimationComponent_SetFloat);
		assembly.AddInternalCall("Chained.AnimationComponent", "AnimationComponent_SetBool_Ptr",
								 (void*)&AnimationComponent_SetBool);
		assembly.AddInternalCall("Chained.AnimationComponent", "AnimationComponent_GetFloat_Ptr",
								 (void*)&AnimationComponent_GetFloat);

		// ── Shader ────────────────────────────────────────────────────────
		assembly.AddInternalCall("Chained.ShaderComponent", "Shader_SetFloat_Ptr", (void*)&Shader_SetFloat);
		assembly.AddInternalCall("Chained.ShaderComponent", "Shader_SetVec3_Ptr", (void*)&Shader_SetVec3);
		assembly.AddInternalCall("Chained.ShaderComponent", "Shader_GetEnabled_Ptr", (void*)&Shader_GetEnabled);
		assembly.AddInternalCall("Chained.ShaderComponent", "Shader_SetEnabled_Ptr", (void*)&Shader_SetEnabled);

		// ── UI static ─────────────────────────────────────────────────────
		assembly.AddInternalCall("Chained.UI", "UI_Text_Ptr", (void*)&UI_Text);
		assembly.AddInternalCall("Chained.UI", "UI_TextColored_Ptr", (void*)&UI_TextColored);
		assembly.AddInternalCall("Chained.UI", "UI_Button_Ptr", (void*)&UI_Button);
		assembly.AddInternalCall("Chained.UI", "UI_BeginWindow_Ptr", (void*)&UI_BeginWindow);
		assembly.AddInternalCall("Chained.UI", "UI_EndWindow_Ptr", (void*)&UI_EndWindow);
		assembly.AddInternalCall("Chained.UI", "UI_InputText_Ptr", (void*)&UI_InputText);
		assembly.AddInternalCall("Chained.UI", "UI_SetKeyboardFocusHere_Ptr", (void*)&UI_SetKeyboardFocusHere);
		assembly.AddInternalCall("Chained.UI", "UI_SetScrollHereY_Ptr", (void*)&UI_SetScrollHereY);
		assembly.AddInternalCall("Chained.UI", "UI_GetDisplaySize_Ptr", (void*)&UI_GetDisplaySize);

		// ── Input ─────────────────────────────────────────────────────────
		assembly.AddInternalCall("Chained.Input", "Input_IsKeyDown_Ptr", (void*)&Input_IsKeyDown);
		assembly.AddInternalCall("Chained.Input", "Input_IsKeyPressed_Ptr", (void*)&Input_IsKeyPressed);
		assembly.AddInternalCall("Chained.Input", "Input_IsKeyReleased_Ptr", (void*)&Input_IsKeyReleased);
		assembly.AddInternalCall("Chained.Input", "Input_IsMouseButtonDown_Ptr", (void*)&Input_IsMouseButtonDown);
		assembly.AddInternalCall("Chained.Input", "Input_IsMouseButtonPressed_Ptr", (void*)&Input_IsMouseButtonPressed);
		assembly.AddInternalCall("Chained.Input", "Input_GetMouseWheelMove_Ptr", (void*)&Input_GetMouseWheelMove);
		assembly.AddInternalCall("Chained.Input", "Input_GetMouseWheelHMove_Ptr", (void*)&Input_GetMouseWheelHMove);
		assembly.AddInternalCall("Chained.Input", "Input_GetMouseScroll_Ptr", (void*)&Input_GetMouseScroll);
		assembly.AddInternalCall("Chained.Input", "Input_GetMouseDelta_Ptr", (void*)&Input_GetMouseDelta);

		// ── Log ───────────────────────────────────────────────────────────
		assembly.AddInternalCall("Chained.Log", "Log_Info_Ptr", (void*)&Log_Info);
		assembly.AddInternalCall("Chained.Log", "Log_Warn_Ptr", (void*)&Log_Warn);
		assembly.AddInternalCall("Chained.Log", "Log_Error_Ptr", (void*)&Log_Error);

		// ── Scene ─────────────────────────────────────────────────────────
		assembly.AddInternalCall("Chained.Scene", "Scene_FindEntityByTag_Ptr", (void*)&Scene_FindEntityByTag);
		assembly.AddInternalCall("Chained.Scene", "Scene_LoadScene_Ptr", (void*)&Scene_LoadScene);
		assembly.AddInternalCall("Chained.Scene", "Scene_GetPrimaryCameraEntity_Ptr",
								 (void*)&Scene_GetPrimaryCameraEntity);
		assembly.AddInternalCall("Chained.Scene", "Scene_CopyEntity_Ptr", (void*)&Scene_CopyEntity);
		assembly.AddInternalCall("Chained.Scene", "Scene_GetCurrentScenePath_Ptr", (void*)&Scene_GetCurrentScenePath);

		// ── Audio static ──────────────────────────────────────────────────
		assembly.AddInternalCall("Chained.Audio", "Audio_Play_Ptr", (void*)&Audio_Play);
		assembly.AddInternalCall("Chained.Audio", "Audio_Stop_Ptr", (void*)&Audio_Stop);
		assembly.AddInternalCall("Chained.Audio", "Audio_StopAll_Ptr", (void*)&Audio_StopAll);
		assembly.AddInternalCall("Chained.Audio", "Audio_GetMasterVolume_Ptr", (void*)&Audio_GetMasterVolume);
		assembly.AddInternalCall("Chained.Audio", "Audio_SetMasterVolume_Ptr", (void*)&Audio_SetMasterVolume);
		assembly.AddInternalCall("Chained.Audio", "Audio_GetMusicVolume_Ptr", (void*)&Audio_GetMusicVolume);
		assembly.AddInternalCall("Chained.Audio", "Audio_SetMusicVolume_Ptr", (void*)&Audio_SetMusicVolume);
		assembly.AddInternalCall("Chained.Audio", "Audio_GetSFXVolume_Ptr", (void*)&Audio_GetSFXVolume);
		assembly.AddInternalCall("Chained.Audio", "Audio_SetSFXVolume_Ptr", (void*)&Audio_SetSFXVolume);

		// ── Application ───────────────────────────────────────────────────
		assembly.AddInternalCall("Chained.Application", "Application_Close_Ptr", (void*)&Application_Close);
		assembly.AddInternalCall("Chained.Application", "Application_GetFPS_Ptr", (void*)&Application_GetFPS);
		assembly.AddInternalCall("Chained.Application", "Application_GetFrameTime_Ptr",
								 (void*)&Application_GetFrameTime);

		// ── Window ────────────────────────────────────────────────────────
		assembly.AddInternalCall("Chained.AppWindow", "Window_SetSize_Ptr", (void*)&Window_SetSize);
		assembly.AddInternalCall("Chained.AppWindow", "Window_SetFullscreen_Ptr", (void*)&Window_SetFullscreen);
		assembly.AddInternalCall("Chained.AppWindow", "Window_SetVSync_Ptr", (void*)&Window_SetVSync);
		assembly.AddInternalCall("Chained.AppWindow", "Window_SetAntialiasing_Ptr", (void*)&Window_SetAntialiasing);
		assembly.AddInternalCall("Chained.AppWindow", "Window_SetAntiAliasingSamples_Ptr",
								 (void*)&Window_SetAntiAliasingSamples);
		assembly.AddInternalCall("Chained.AppWindow", "Window_GetAntiAliasingSamples_Ptr",
								 (void*)&Window_GetAntiAliasingSamples);
		assembly.AddInternalCall("Chained.AppWindow", "Window_SetEnableShadows_Ptr", (void*)&Window_SetEnableShadows);
		assembly.AddInternalCall("Chained.AppWindow", "Window_GetEnableShadows_Ptr", (void*)&Window_GetEnableShadows);
		assembly.AddInternalCall("Chained.AppWindow", "Window_SetShadowResolution_Ptr",
								 (void*)&Window_SetShadowResolution);
		assembly.AddInternalCall("Chained.AppWindow", "Window_GetShadowResolution_Ptr",
								 (void*)&Window_GetShadowResolution);
		assembly.AddInternalCall("Chained.AppWindow", "Window_GetVSync_Ptr", (void*)&Window_GetVSync);
		assembly.AddInternalCall("Chained.AppWindow", "Window_GetFullscreen_Ptr", (void*)&Window_GetFullscreen);
		assembly.AddInternalCall("Chained.AppWindow", "Window_GetWidth_Ptr", (void*)&Window_GetWidth);
		assembly.AddInternalCall("Chained.AppWindow", "Window_GetHeight_Ptr", (void*)&Window_GetHeight);
		assembly.AddInternalCall("Chained.AppWindow", "Window_GetSupportedResolution_Ptr",
								 (void*)&Window_GetSupportedResolution);

		// ── Physics ─────────────────────────────────────────────────────
		assembly.AddInternalCall("Chained.Physics", "Physics_GetGravity_Ptr", (void*)&Physics_GetGravity);

		// ── Network ─────────────────────────────────────────────────────
		assembly.AddInternalCall("Chained.Network", "Network_HostGame_Ptr", (void*)&Network_HostGame);
		assembly.AddInternalCall("Chained.Network", "Network_ConnectTo_Ptr", (void*)&Network_ConnectTo);
		assembly.AddInternalCall("Chained.Network", "Network_Disconnect_Ptr", (void*)&Network_Disconnect);
		assembly.AddInternalCall("Chained.Network", "Network_IsHost_Ptr", (void*)&Network_IsHost);
		assembly.AddInternalCall("Chained.Network", "Network_IsClient_Ptr", (void*)&Network_IsClient);
		assembly.AddInternalCall("Chained.Network", "Network_IsConnected_Ptr", (void*)&Network_IsConnected);
		assembly.AddInternalCall("Chained.Network", "Network_GetClientCount_Ptr", (void*)&Network_GetClientCount);
		assembly.AddInternalCall("Chained.Network", "Network_GetMaxClients_Ptr", (void*)&Network_GetMaxClients);
		assembly.AddInternalCall("Chained.Network", "Network_GetRole_Ptr", (void*)&Network_GetRole);
		assembly.AddInternalCall("Chained.Network", "Network_GetListenAddress_Ptr", (void*)&Network_GetListenAddress);
		assembly.AddInternalCall("Chained.Network", "Network_GetPublicAddress_Ptr", (void*)&Network_GetPublicAddress);
		assembly.AddInternalCall("Chained.Network", "Network_BroadcastSceneChange_Ptr",
								 (void*)&Network_BroadcastSceneChange);
		assembly.AddInternalCall("Chained.Network", "Network_HasPendingSceneChange_Ptr",
								 (void*)&Network_HasPendingSceneChange);
		assembly.AddInternalCall("Chained.Network", "Network_GetPendingSceneChange_Ptr",
								 (void*)&Network_GetPendingSceneChange);
		assembly.AddInternalCall("Chained.Network", "Network_ClearPendingSceneChange_Ptr",
								 (void*)&Network_ClearPendingSceneChange);

		// New: Player list
		assembly.AddInternalCall("Chained.Network", "Network_SetLocalPlayerInfo_Ptr",
								 (void*)&Network_SetLocalPlayerInfo);
		assembly.AddInternalCall("Chained.Network", "Network_SendPlayerInfo_Ptr", (void*)&Network_SendPlayerInfo);
		assembly.AddInternalCall("Chained.Network", "Network_GetPlayerCount_Ptr", (void*)&Network_GetPlayerCount);
		assembly.AddInternalCall("Chained.Network", "Network_GetPlayerListJSON_Ptr", (void*)&Network_GetPlayerListJSON);
		assembly.AddInternalCall("Chained.Network", "Network_GetLocalNetworkID_Ptr", (void*)&Network_GetLocalNetworkID);

		// New: Chat
		assembly.AddInternalCall("Chained.Network", "Network_SendChatMessage_Ptr", (void*)&Network_SendChatMessage);
		assembly.AddInternalCall("Chained.Network", "Network_HasPendingChat_Ptr", (void*)&Network_HasPendingChat);
		assembly.AddInternalCall("Chained.Network", "Network_GetPendingChatJSON_Ptr",
								 (void*)&Network_GetPendingChatJSON);
		assembly.AddInternalCall("Chained.Network", "Network_ClearPendingChat_Ptr", (void*)&Network_ClearPendingChat);

		// New: Prefab
		assembly.AddInternalCall("Chained.Network", "Network_SetPlayerPrefab_Ptr", (void*)&Network_SetPlayerPrefab);

		// UPnP
		assembly.AddInternalCall("Chained.Network", "Network_IsUpnpAvailable_Ptr", (void*)&Network_IsUpnpAvailable);
		assembly.AddInternalCall("Chained.Network", "Network_IsFullyConnected_Ptr", (void*)&Network_IsFullyConnected);

		// STUN / NAT Traversal
		assembly.AddInternalCall("Chained.Network", "Network_HasStunResult_Ptr", (void*)&Network_HasStunResult);
		assembly.AddInternalCall("Chained.Network", "Network_GetStunPublicAddress_Ptr",
								 (void*)&Network_GetStunPublicAddress);
		assembly.AddInternalCall("Chained.Network", "Network_StartHolePunch_Ptr", (void*)&Network_StartHolePunch);
		assembly.AddInternalCall("Chained.Network", "Network_QueryStun_Ptr", (void*)&Network_QueryStun);

		// Clipboard
		assembly.AddInternalCall("Chained.Clipboard", "Clipboard_SetText_Ptr", (void*)&Clipboard_SetText);

		// ── Auto-generated: Player, Spawn, NetworkIdentity properties ──────
#include "generated/script_glue_generated_reg.inl"

		assembly.UploadInternalCalls();
		CH_CORE_INFO("[ScriptGlue] Registered {} internal calls for '{}'.", 172, (std::string)assembly.GetName());
	}

} // namespace Chained
