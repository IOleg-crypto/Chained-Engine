using System;
using System.Collections.Generic;
using System.Globalization;
using Chained;

namespace ChainedDecos.Scripts
{
    /// <summary>
    /// Full-screen settings overlay rendered inside start_menu.chscene.
    /// Attach to any entity in start_menu. The Options button should call
    /// OptionsOverlay.IsOpen = true (via ShowSettingsScript).
    /// </summary>
    [AutoAttach("OptionsOverlay")]
    public class OptionsOverlay : Script
    {
        public static bool IsOpen { get; set; } = false;

        private int m_Tab = 0;
        private static readonly string[] k_Tabs = { "VIDEO", "SHADOWS", "AUDIO" };

        private static readonly string[] k_Resolutions = { "1280x720", "1600x900", "1920x1080", "2560x1440", "3840x2160" };
        private static readonly string[] k_AA          = { "Off", "2x MSAA", "4x MSAA", "8x MSAA", "16x MSAA" };
        private static readonly int[]    k_AAValues    = { 0, 2, 4, 8, 16 };
        private int  m_ResIdx     = 2;
        private int  m_AAIdx      = 2;
        private bool m_Fullscreen = false;
        private bool m_VSync      = true;

        private static readonly string[] k_ShadowRes  = { "512 (Low)", "1024 (Med)", "2048 (High)", "4096 (Ultra)" };
        private static readonly int[]    k_ShadowVals = { 512, 1024, 2048, 4096 };
        private bool m_Shadows   = true;
        private int  m_ShadowIdx = 2;

        private bool  m_MuteAudio = false;
        private float m_Master = 100f;
        private float m_Music  = 100f;
        private float m_SFX    = 100f;

        private string m_Desc = "Hover over a setting to see its description.";

        public override void OnCreate()
        {
            IsOpen = false;
            m_VSync       = AppWindow.GetVSync();
            m_Fullscreen  = AppWindow.GetFullscreen();
            m_Shadows     = AppWindow.GetEnableShadows();
            m_Master      = Audio.GetMasterVolume() * 100f;
            m_Music       = Audio.GetMusicVolume()  * 100f;
            m_SFX         = Audio.GetSFXVolume()    * 100f;
            m_MuteAudio   = (m_Master < 0.1f);
        }

        public override void OnUpdate(float deltaTime)
        {
            if (Input.IsKeyPressed(Key.Escape) && IsOpen)
                IsOpen = false;
        }

        public override void OnGUI()
        {
            if (!IsOpen) return;

            Vector2 display = UI.GetDisplaySize();

            UI.BeginWindow("##OptionsOverlay", 0f, 0f, display.X, display.Y, 0.96f);

            // HEADER
            UI.Dummy(0f, 16f);
            UI.SetWindowFontScale(1.8f);
            UI.TextColored("SETTINGS", 1f, 1f, 1f, 1f);
            UI.SetWindowFontScale(1.0f);
            UI.Dummy(0f, 16f);

            // TABS
            for (int i = 0; i < k_Tabs.Length; i++)
            {
                if (i > 0) UI.SameLine(0f, 10f);
                if (m_Tab == i) UI.PushStyleColor(21, 0.2f, 0.6f, 0.8f, 1f);
                else            UI.PushStyleColor(21, 0.1f, 0.1f, 0.1f, 1f);
                if (UI.Button("   " + k_Tabs[i] + "   ")) m_Tab = i;
                UI.PopStyleColor(1);
            }

            UI.Dummy(0f, 10f);
            UI.Separator();
            UI.Dummy(0f, 20f);

            float leftW  = display.X * 0.55f;
            float rightW = display.X * 0.35f;
            float paneH  = display.Y * 0.65f;

            UI.BeginChild("##OLeft", leftW, paneH, false);
            switch (m_Tab)
            {
                case 0: DrawVideo(leftW);   break;
                case 1: DrawShadows(leftW); break;
                case 2: DrawAudio(leftW);   break;
            }
            UI.EndChild();

            UI.SameLine(0f, 40f);

            UI.BeginChild("##ORight", rightW, paneH, false);
            UI.Dummy(0f, 20f);
            UI.BeginChild("##ODescBox", rightW - 20f, 160f, true);
            UI.TextColored(m_Desc, 0.8f, 0.8f, 0.8f, 1f);
            UI.EndChild();
            UI.EndChild();

            UI.Separator();
            UI.Dummy(0f, 15f);

            UI.PushStyleColor(21, 0.2f, 0.6f, 0.8f, 1f);
            if (UI.Button("   Apply   ")) ApplySettings();
            UI.PopStyleColor(1);

            UI.SameLine(0f, 20f);
            if (UI.Button("   Reset Category   ")) ResetCategory();

            UI.SameLine(0f, 20f);
            if (UI.Button("   Back   "))
                IsOpen = false; // Just hide — no scene load, session preserved!

            UI.EndWindow();
        }

        private void DrawVideo(float w)
        {
            SectionHeader("Display");
            DrawCycleRow("Resolution", k_Resolutions, ref m_ResIdx, w,
                "Sets the rendering resolution.\n\n(Default: 1920x1080)");
            DrawCycleRow("Anti-Aliasing", k_AA, ref m_AAIdx, w,
                "Smooths jagged edges.\n\n(Default: 4x MSAA)");
            UI.Dummy(0f, 20f);
            SectionHeader("Window");
            DrawToggleRow("Fullscreen", ref m_Fullscreen, w,
                "Exclusive fullscreen mode.\n\n(Default: Off)");
            DrawToggleRow("VSync", ref m_VSync, w,
                "Sync framerate to monitor.\n\n(Default: On)");
        }

        private void DrawShadows(float w)
        {
            SectionHeader("Shadow Quality");
            DrawToggleRow("Enable Shadows", ref m_Shadows, w,
                "Dynamic shadow casting.\n\n(Default: On)");
            DrawCycleRow("Shadow Resolution", k_ShadowRes, ref m_ShadowIdx, w,
                "Shadow map resolution.\n\n(Default: 2048 High)");
        }

        private void DrawAudio(float w)
        {
            SectionHeader("Volume");
            DrawToggleRow("Mute All Audio", ref m_MuteAudio, w,
                "Completely mutes all audio.\n\n(Default: Off)");
            DrawSliderRow("Master Volume", ref m_Master, 0f, 100f, w,
                "Overall audio volume.\n\n(Default: 100)");
            DrawSliderRow("Music Volume", ref m_Music, 0f, 100f, w,
                "Background music volume.\n\n(Default: 100)");
            DrawSliderRow("SFX Volume", ref m_SFX, 0f, 100f, w,
                "Sound effects volume.\n\n(Default: 100)");
        }

        private void SectionHeader(string title)
        {
            UI.TextColored(title, 0.4f, 0.8f, 1.0f, 1f);
            UI.Separator();
            UI.Dummy(0f, 10f);
        }

        private void DrawCycleRow(string label, string[] options, ref int index, float w, string desc)
        {
            UI.Text(label);
            if (UI.IsItemHovered()) m_Desc = desc;
            UI.SameLine(w - 280f, -1f);
            UI.PushStyleColor(21, 0.15f, 0.15f, 0.15f, 1f);
            if (UI.Button(" < ##o" + label)) { index--; if (index < 0) index = options.Length - 1; }
            UI.SameLine(0f, 15f);
            UI.TextColored(options[index], 1f, 1f, 1f, 1f);
            UI.SameLine(0f, 15f);
            if (UI.Button(" > ##o" + label)) { index++; if (index >= options.Length) index = 0; }
            UI.PopStyleColor(1);
            UI.Dummy(0f, 10f);
        }

        private void DrawToggleRow(string label, ref bool value, float w, string desc)
        {
            UI.Checkbox(label, ref value);
            if (UI.IsItemHovered()) m_Desc = desc;
            UI.Dummy(0f, 10f);
        }

        private void DrawSliderRow(string label, ref float value, float min, float max, float w, string desc)
        {
            UI.Text(label);
            if (UI.IsItemHovered()) m_Desc = desc;
            UI.SameLine(w - 300f, -1f);
            UI.SetNextItemWidth(250f);
            UI.SliderFloat("##osl_" + label, ref value, min, max);
            UI.Dummy(0f, 10f);
        }

        private void ApplySettings()
        {
            string[] parts = k_Resolutions[m_ResIdx].Split('x');
            if (parts.Length == 2 && int.TryParse(parts[0], out int w) && int.TryParse(parts[1], out int h))
                AppWindow.SetSize(w, h);
            AppWindow.SetFullscreen(m_Fullscreen);
            AppWindow.SetVSync(m_VSync);
            AppWindow.SetEnableShadows(m_Shadows);
            AppWindow.SetAntiAliasingSamples(k_AAValues[m_AAIdx]);
            AppWindow.SetShadowResolution(k_ShadowVals[m_ShadowIdx]);
            if (m_MuteAudio) { Audio.SetMasterVolume(0f); Audio.SetMusicVolume(0f); Audio.SetSFXVolume(0f); }
            else { Audio.SetMasterVolume(m_Master / 100f); Audio.SetMusicVolume(m_Music / 100f); Audio.SetSFXVolume(m_SFX / 100f); }

            var cfg = new Dictionary<string, string>
            {
                ["Resolution"]          = k_Resolutions[m_ResIdx],
                ["Fullscreen"]          = m_Fullscreen.ToString(),
                ["VSync"]               = m_VSync.ToString(),
                ["EnableShadows"]       = m_Shadows.ToString(),
                ["AntiAliasingSamples"] = k_AAValues[m_AAIdx].ToString(),
                ["ShadowResolution"]    = k_ShadowVals[m_ShadowIdx].ToString(),
                ["MuteAudio"]           = m_MuteAudio.ToString(),
                ["MasterVolume"]        = m_Master.ToString("F1", CultureInfo.InvariantCulture),
                ["MusicVolume"]         = m_Music.ToString("F1",  CultureInfo.InvariantCulture),
                ["SFXVolume"]           = m_SFX.ToString("F1",   CultureInfo.InvariantCulture),
            };
            SettingsConfig.Save(cfg);
        }

        private void ResetCategory()
        {
            switch (m_Tab)
            {
                case 0: m_ResIdx = 2; m_AAIdx = 2; m_Fullscreen = false; m_VSync = true; break;
                case 1: m_Shadows = true; m_ShadowIdx = 2; break;
                case 2: m_MuteAudio = false; m_Master = 100f; m_Music = 100f; m_SFX = 100f; break;
            }
        }
    }
}
