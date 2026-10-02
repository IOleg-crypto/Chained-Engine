using System;
using System.Runtime.InteropServices;

namespace Chained
{
    // ─────────────────────────────────────────────────────────────────────────────
    //  Scene — static API for scene / entity queries
    // ─────────────────────────────────────────────────────────────────────────────
    /// <summary>Scene helpers.</summary>
    public static class Scene
    {
#pragma warning disable 0649
        
        internal static unsafe delegate* unmanaged<char*, ulong> Scene_FindEntityByTag_Ptr;
        internal static unsafe delegate* unmanaged<char*, void> Scene_LoadScene_Ptr;
        internal static unsafe delegate* unmanaged<ulong> Scene_GetPrimaryCameraEntity_Ptr;
        internal static unsafe delegate* unmanaged<ulong, ulong> Scene_CopyEntity_Ptr;
        internal static unsafe delegate* unmanaged<char*> Scene_GetCurrentScenePath_Ptr;
#pragma warning restore 0649

        /// <summary>Returns the current scene file path (e.g. 'scenes/test_platform_scene.chscene').</summary>
        public static unsafe string GetCurrentScenePath()
        {
            if (Scene_GetCurrentScenePath_Ptr == null) return "";
            char* ptr = Scene_GetCurrentScenePath_Ptr();
            return ptr != null ? new string(ptr) : "";
        }

        /// <summary>Finds an entity by tag. Returns null if not found or pointer is uninitialized.</summary>
        
        public static unsafe Entity? FindEntityByTag(string tag)
{
    if (Scene_FindEntityByTag_Ptr == null)
    {
        Console.WriteLine("[ScriptEngine] Error: Scene_FindEntityByTag_Ptr is NULL!");
        return null;
    }

    if (string.IsNullOrEmpty(tag)) return null;

    fixed (char* ptr = tag)
    {
        ulong entityId = Scene_FindEntityByTag_Ptr(ptr);
        return (entityId != Entity.NullID && entityId != ulong.MaxValue && entityId != (ulong)uint.MaxValue)
            ? new Entity(entityId)
            : null;
    }
}

        /// <summary>Loads a scene.</summary>
        public static unsafe void LoadScene(string path)
        {
            if (Scene_LoadScene_Ptr == null) return;
            fixed (char* ptr = path) Scene_LoadScene_Ptr(ptr);
        }

        private static unsafe ulong GetPrimaryCameraEntity_Native()
        {
            if (Scene_GetPrimaryCameraEntity_Ptr == null) return 0;
            return Scene_GetPrimaryCameraEntity_Ptr();
        }

        /// <summary>Returns the main camera entity.</summary>
        public static Entity? GetMainCamera()
        {
            ulong entityId = GetPrimaryCameraEntity_Native();
            if (entityId != 0) return new Entity(entityId);

            // Fallback: first camera in the scene.
            var entityIds = Entity.FindAllWithComponent<CameraComponent>();
            if (entityIds.Length > 0) return new Entity(entityIds[0]);

            return null;
        }

        /// <summary>Creates a deep copy of an entity.</summary>
        public static Entity? CopyEntity(Entity entity)
        {
            if (entity == null || !entity.IsValid) return null;

            ulong newId = 0;
            unsafe
            {
                if (Scene_CopyEntity_Ptr == null) return null;
                newId = Scene_CopyEntity_Ptr(entity.ID);
            }
            return newId != 0 ? new Entity(newId) : null;
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Audio — static API for audio playback
    // ─────────────────────────────────────────────────────────────────────────────
    /// <summary>Audio helpers.</summary>
    public static class Audio
    {
#pragma warning disable 0649
        internal static unsafe delegate* unmanaged<char*, float, float, byte, void> Audio_Play_Ptr;
        internal static unsafe delegate* unmanaged<char*, void> Audio_Stop_Ptr;
        internal static unsafe delegate* unmanaged<void> Audio_StopAll_Ptr;
        internal static unsafe delegate* unmanaged<float> Audio_GetMasterVolume_Ptr;
        internal static unsafe delegate* unmanaged<float, void> Audio_SetMasterVolume_Ptr;
        internal static unsafe delegate* unmanaged<float> Audio_GetMusicVolume_Ptr;
        internal static unsafe delegate* unmanaged<float, void> Audio_SetMusicVolume_Ptr;
        internal static unsafe delegate* unmanaged<float> Audio_GetSFXVolume_Ptr;
        internal static unsafe delegate* unmanaged<float, void> Audio_SetSFXVolume_Ptr;
#pragma warning restore 0649

        /// <summary>Plays an audio clip.</summary>
        public static unsafe void Play(string path, float volume = 1.0f, float pitch = 1.0f, bool loop = false)
        {
            if (Audio_Play_Ptr == null) return;
            fixed (char* ptr = path) Audio_Play_Ptr(ptr, volume, pitch, (byte)(loop ? 1 : 0));
        }

        /// <summary>Stops playback for one clip.</summary>
        public static unsafe void Stop(string path)
        {
            if (Audio_Stop_Ptr == null) return;
            fixed (char* ptr = path) Audio_Stop_Ptr(ptr);
        }

        /// <summary>Stops all audio.</summary>
        public static unsafe void StopAll()
        {
            if (Audio_StopAll_Ptr == null) return;
            Audio_StopAll_Ptr();
        }

        /// <summary>Returns master volume (0.0 – 1.0).</summary>
        public static unsafe float GetMasterVolume()
        {
            if (Audio_GetMasterVolume_Ptr == null) return 1.0f;
            return Audio_GetMasterVolume_Ptr();
        }

        /// <summary>Sets master volume (0.0 – 1.0).</summary>
        public static unsafe void SetMasterVolume(float volume)
        {
            if (Audio_SetMasterVolume_Ptr == null) return;
            Audio_SetMasterVolume_Ptr(volume);
        }

        /// <summary>Returns music volume (0.0 – 1.0).</summary>
        public static unsafe float GetMusicVolume()
        {
            if (Audio_GetMusicVolume_Ptr == null) return 1.0f;
            return Audio_GetMusicVolume_Ptr();
        }

        /// <summary>Sets music volume (0.0 – 1.0).</summary>
        public static unsafe void SetMusicVolume(float volume)
        {
            if (Audio_SetMusicVolume_Ptr == null) return;
            Audio_SetMusicVolume_Ptr(volume);
        }

        /// <summary>Returns SFX volume (0.0 – 1.0).</summary>
        public static unsafe float GetSFXVolume()
        {
            if (Audio_GetSFXVolume_Ptr == null) return 1.0f;
            return Audio_GetSFXVolume_Ptr();
        }

        /// <summary>Sets SFX volume (0.0 – 1.0).</summary>
        public static unsafe void SetSFXVolume(float volume)
        {
            if (Audio_SetSFXVolume_Ptr == null) return;
            Audio_SetSFXVolume_Ptr(volume);
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Application — static API for application control
    // ─────────────────────────────────────────────────────────────────────────────
    /// <summary>Application helpers.</summary>
    public static class Application
    {
#pragma warning disable 0649
        internal static unsafe delegate* unmanaged<void> Application_Close_Ptr;
        internal static unsafe delegate* unmanaged<int> Application_GetFPS_Ptr;
        internal static unsafe delegate* unmanaged<float> Application_GetFrameTime_Ptr;
#pragma warning restore 0649

        /// <summary>Requests shutdown.</summary>
        public static unsafe void Close()
        {
            if (Application_Close_Ptr == null) return;
            Application_Close_Ptr();
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Time — static API for timing information
    // ─────────────────────────────────────────────────────────────────────────────
    /// <summary>Frame timing.</summary>
    public static class Time
    {
        /// <summary>Current frame rate.</summary>
        public static unsafe int FPS => Application.Application_GetFPS_Ptr != null ? Application.Application_GetFPS_Ptr() : 0;
        
        /// <summary>Current frame time.</summary>
        public static unsafe float DeltaTime => Application.Application_GetFrameTime_Ptr != null ? Application.Application_GetFrameTime_Ptr() : 0.0f;
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  AppWindow — static API for window control
    // ─────────────────────────────────────────────────────────────────────────────
    /// <summary>Window helpers.</summary>
    public static class AppWindow
    {
#pragma warning disable 0649
        internal static unsafe delegate* unmanaged<int, int, void> Window_SetSize_Ptr;
        internal static unsafe delegate* unmanaged<byte, void> Window_SetFullscreen_Ptr;
        internal static unsafe delegate* unmanaged<byte, void> Window_SetVSync_Ptr;
        internal static unsafe delegate* unmanaged<byte, void> Window_SetAntialiasing_Ptr;
        internal static unsafe delegate* unmanaged<int, void> Window_SetAntiAliasingSamples_Ptr;
        internal static unsafe delegate* unmanaged<int> Window_GetAntiAliasingSamples_Ptr;
        internal static unsafe delegate* unmanaged<byte, void> Window_SetEnableShadows_Ptr;
        internal static unsafe delegate* unmanaged<byte> Window_GetEnableShadows_Ptr;
        internal static unsafe delegate* unmanaged<int, void> Window_SetShadowResolution_Ptr;
        internal static unsafe delegate* unmanaged<int> Window_GetShadowResolution_Ptr;
        internal static unsafe delegate* unmanaged<byte> Window_GetVSync_Ptr;
        internal static unsafe delegate* unmanaged<byte> Window_GetFullscreen_Ptr;
        internal static unsafe delegate* unmanaged<int> Window_GetWidth_Ptr;
        internal static unsafe delegate* unmanaged<int> Window_GetHeight_Ptr;
        internal static unsafe delegate* unmanaged<char*> Window_GetSupportedResolution_Ptr;
#pragma warning restore 0649

        /// <summary>Sets the window size.</summary>
        public static unsafe void SetSize(int width, int height)
        {
            if (Window_SetSize_Ptr == null) return;
            Window_SetSize_Ptr(width, height);
        }

        /// <summary>Toggles fullscreen mode.</summary>
        public static unsafe void SetFullscreen(bool enabled)
        {
            if (Window_SetFullscreen_Ptr == null) return;
            Window_SetFullscreen_Ptr((byte)(enabled ? 1 : 0));
        }

        /// <summary>Toggles vertical sync.</summary>
        public static unsafe void SetVSync(bool enabled)
        {
            if (Window_SetVSync_Ptr == null) return;
            Window_SetVSync_Ptr((byte)(enabled ? 1 : 0));
        }

        /// <summary>Toggles multisampling.</summary>
        public static unsafe void SetAntialiasing(bool enabled)
        {
            if (Window_SetAntialiasing_Ptr == null) return;
            Window_SetAntialiasing_Ptr((byte)(enabled ? 1 : 0));
        }

        /// <summary>Sets the MSAA sample count (0, 2, 4, 8, or 16). Recreates the HDR framebuffer with the new sample count.</summary>
        public static unsafe void SetAntiAliasingSamples(int samples)
        {
            if (Window_SetAntiAliasingSamples_Ptr == null) return;
            Window_SetAntiAliasingSamples_Ptr(samples);
        }

        /// <summary>Returns the current MSAA sample count (0, 2, 4, 8, or 16).</summary>
        public static unsafe int GetAntiAliasingSamples()
        {
            if (Window_GetAntiAliasingSamples_Ptr == null) return 4;
            return Window_GetAntiAliasingSamples_Ptr();
        }

        /// <summary>Enables or disables shadow rendering.</summary>
        public static unsafe void SetEnableShadows(bool enabled)
        {
            if (Window_SetEnableShadows_Ptr == null) return;
            Window_SetEnableShadows_Ptr((byte)(enabled ? 1 : 0));
        }

        /// <summary>Returns whether shadow rendering is enabled.</summary>
        public static unsafe bool GetEnableShadows()
        {
            if (Window_GetEnableShadows_Ptr == null) return true;
            return Window_GetEnableShadows_Ptr() != 0;
        }

        /// <summary>Sets the shadow map resolution (512, 1024, 2048, 4096).</summary>
        public static unsafe void SetShadowResolution(int resolution)
        {
            if (Window_SetShadowResolution_Ptr == null) return;
            Window_SetShadowResolution_Ptr(resolution);
        }

        /// <summary>Returns the shadow map resolution in pixels.</summary>
        public static unsafe int GetShadowResolution()
        {
            if (Window_GetShadowResolution_Ptr == null) return 2048;
            return Window_GetShadowResolution_Ptr();
        }

        /// <summary>Returns whether vertical sync is enabled.</summary>
        public static unsafe bool GetVSync()
        {
            if (Window_GetVSync_Ptr == null) return false;
            return Window_GetVSync_Ptr() != 0;
        }

        /// <summary>Returns whether the window is in fullscreen mode.</summary>
        public static unsafe bool GetFullscreen()
        {
            if (Window_GetFullscreen_Ptr == null) return false;
            return Window_GetFullscreen_Ptr() != 0;
        }

        /// <summary>Returns the current window width in pixels.</summary>
        public static unsafe int GetWidth()
        {
            if (Window_GetWidth_Ptr == null) return 1280;
            return Window_GetWidth_Ptr();
        }

        /// <summary>Returns the current window height in pixels.</summary>
        public static unsafe int GetHeight()
        {
            if (Window_GetHeight_Ptr == null) return 720;
            return Window_GetHeight_Ptr();
        }

        /// <summary>Returns all supported resolutions as a semicolon-separated string (e.g. "1920x1080;1280x720").</summary>
        public static unsafe string GetSupportedResolutions()
        {
            if (Window_GetSupportedResolution_Ptr == null) return "";
            char* ptr = Window_GetSupportedResolution_Ptr();
            return ptr != null ? new string(ptr) : "";
        }
    }

    // ─────────────────────────────────────────────────────────────────────────────
    //  Physics — static API for physics settings
    // ─────────────────────────────────────────────────────────────────────────────
    /// <summary>Physics configuration helpers.</summary>
    public static class Physics
    {
#pragma warning disable 0649
        internal static unsafe delegate* unmanaged<float> Physics_GetGravity_Ptr;
#pragma warning restore 0649

        /// <summary>Returns the world gravity from the project configuration (units/s^2).</summary>
        public static unsafe float GetGravity()
        {
            if (Physics_GetGravity_Ptr == null) return 20.0f;
            return Physics_GetGravity_Ptr();
        }
    }
}