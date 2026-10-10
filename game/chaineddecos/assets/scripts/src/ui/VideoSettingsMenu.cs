using System;
using System.Collections.Generic;
using System.Globalization;
using Chained;

namespace ChainedDecos.Scripts
{
    [AutoAttach("VideoSettingsMenu")]
    public class VideoSettingsMenu : Script
    {
        private int m_Tab = 0;
        private static readonly string[] k_Tabs = { "VIDEO", "SHADOWS", "AUDIO" };

        private static readonly string[] k_Resolutions = { "1280x720", "1600x900", "1920x1080", "2560x1440", "3840x2160" };
        private static readonly string[] k_AA          = { "Off", "2x MSAA", "4x MSAA", "8x MSAA", "16x MSAA" };
        private static readonly int[]    k_AAValues    = { 0, 2, 4, 8, 16 };
        
        private int  m_ResIdx       = 2;
        private int  m_AAIdx        = 2;
        private bool m_Fullscreen   = false;
        private bool m_VSync        = true;

        private static readonly string[] k_ShadowRes = { "512 (Low)", "1024 (Med)", "2048 (High)", "4096 (Ultra)" };
        private static readonly int[]    k_ShadowVals = { 512, 1024, 2048, 4096 };
        private bool m_Shadows    = true;
        private int  m_ShadowIdx  = 2;

        private bool m_MuteAudio = false;
        private float m_Master = 100f;
        private float m_Music  = 100f;
        private float m_SFX    = 100f;

        private string m_Desc = "Hover over a setting to see its description.";
        public string BackScene = "scenes/start_menu.chscene";

        public override void OnCreate()
        {
            LoadFromConfig();
        }

        public override void OnStart()
        {
            LoadFromConfig();
        }

        private void LoadFromConfig()
        {
            SettingsConfig.ApplyAll();
            var cfg = SettingsConfig.Load();

            // Resolution
            if (cfg.TryGetValue("Resolution", out string? resStr) && !string.IsNullOrEmpty(resStr))
            {
                int idx = Array.IndexOf(k_Resolutions, resStr);
                if (idx >= 0) m_ResIdx = idx;
                else
                {
                    string curRes = $"{AppWindow.GetWidth()}x{AppWindow.GetHeight()}";
                    int curIdx = Array.IndexOf(k_Resolutions, curRes);
                    if (curIdx >= 0) m_ResIdx = curIdx;
                }
            }
            else
            {
                string curRes = $"{AppWindow.GetWidth()}x{AppWindow.GetHeight()}";
                int curIdx = Array.IndexOf(k_Resolutions, curRes);
                if (curIdx >= 0) m_ResIdx = curIdx;
            }

            // Anti-Aliasing
            if (cfg.TryGetValue("AntiAliasingSamples", out string? aaStr) && int.TryParse(aaStr, out int parsedAA))
            {
                int idx = Array.IndexOf(k_AAValues, parsedAA);
                if (idx >= 0) m_AAIdx = idx;
            }
            else
            {
                int curAA = AppWindow.GetAntiAliasingSamples();
                int idx = Array.IndexOf(k_AAValues, curAA);
                if (idx >= 0) m_AAIdx = idx;
            }

            // Shadows
            if (cfg.TryGetValue("EnableShadows", out string? shStr) && bool.TryParse(shStr, out bool sh))
                m_Shadows = sh;
            else
                m_Shadows = AppWindow.GetEnableShadows();

            // Shadow Resolution
            if (cfg.TryGetValue("ShadowResolution", out string? srStr) && int.TryParse(srStr, out int parsedSR))
            {
                int idx = Array.IndexOf(k_ShadowVals, parsedSR);
                if (idx >= 0) m_ShadowIdx = idx;
            }
            else
            {
                int curSR = AppWindow.GetShadowResolution();
                int idx = Array.IndexOf(k_ShadowVals, curSR);
                if (idx >= 0) m_ShadowIdx = idx;
            }

            // Fullscreen & VSync
            if (cfg.TryGetValue("Fullscreen", out string? fsStr) && bool.TryParse(fsStr, out bool fs))
                m_Fullscreen = fs;
            else
                m_Fullscreen = AppWindow.GetFullscreen();

            if (cfg.TryGetValue("VSync", out string? vsStr) && bool.TryParse(vsStr, out bool vs))
                m_VSync = vs;
            else
                m_VSync = AppWindow.GetVSync();

            // Audio
            if (cfg.TryGetValue("MuteAudio", out string? muteStr) && bool.TryParse(muteStr, out bool mute))
                m_MuteAudio = mute;

            if (cfg.TryGetValue("MasterVolume", out string? mvStr) &&
                float.TryParse(mvStr, NumberStyles.Float, CultureInfo.InvariantCulture, out float mv))
                m_Master = mv > 1.0f ? mv : mv * 100f;
            else
                m_Master = Audio.GetMasterVolume() * 100f;

            if (cfg.TryGetValue("MusicVolume", out string? musStr) &&
                float.TryParse(musStr, NumberStyles.Float, CultureInfo.InvariantCulture, out float mus))
                m_Music = mus > 1.0f ? mus : mus * 100f;
            else
                m_Music = Audio.GetMusicVolume() * 100f;

            if (cfg.TryGetValue("SFXVolume", out string? sfxStr) &&
                float.TryParse(sfxStr, NumberStyles.Float, CultureInfo.InvariantCulture, out float sfx))
                m_SFX = sfx > 1.0f ? sfx : sfx * 100f;
            else
                m_SFX = Audio.GetSFXVolume() * 100f;
        }

        public override void OnUpdate(float deltaTime)
        {
            if (Input.IsKeyPressed(Key.Escape))
            {
                Scene.LoadScene(BackScene);
            }
        }

        public override void OnGUI()
        {
            Vector2 display = UI.GetDisplaySize();

            UI.BeginWindow("##VideoSettings", 0f, 0f, display.X, display.Y, 0.95f);

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

                bool active = (m_Tab == i);
                
                if (active) UI.PushStyleColor(21, 0.2f, 0.6f, 0.8f, 1f);
                else        UI.PushStyleColor(21, 0.1f, 0.1f, 0.1f, 1f);

                if (UI.Button("   " + k_Tabs[i] + "   ")) m_Tab = i;
                UI.PopStyleColor(1);
            }

            UI.Dummy(0f, 10f);
            UI.Separator();
            UI.Dummy(0f, 20f);

            // SPLIT LAYOUT
            float leftW  = display.X * 0.55f;
            float rightW = display.X * 0.35f;
            float paneH  = display.Y * 0.65f;

            UI.BeginChild("##Left", leftW, paneH, false);
            switch (m_Tab)
            {
                case 0: DrawVideo(leftW);   break;
                case 1: DrawShadows(leftW); break;
                case 2: DrawAudio(leftW);   break;
            }
            UI.EndChild();

            UI.SameLine(0f, 40f);

            UI.BeginChild("##Right", rightW, paneH, false);
            UI.Dummy(0f, 20f);
            
            // Description box
            UI.BeginChild("##DescBox", rightW - 20f, 160f, true);
            UI.TextColored(m_Desc, 0.8f, 0.8f, 0.8f, 1f);
            UI.EndChild();
            UI.EndChild();

            // BOTTOM BAR
            UI.Separator();
            UI.Dummy(0f, 15f);

            UI.PushStyleColor(21, 0.2f, 0.6f, 0.8f, 1f);
            if (UI.Button("   Apply   ")) ApplySettings();
            UI.PopStyleColor(1);

            UI.SameLine(0f, 20f);
            if (UI.Button("   Reset Category   ")) ResetCategory();
            
            UI.SameLine(0f, 20f);
            if (UI.Button("   Back   ")) Scene.LoadScene(BackScene);

            UI.EndWindow();
        }

        private void DrawVideo(float w)
        {
            SectionHeader("Display");
            DrawCycleRow("Resolution", k_Resolutions, ref m_ResIdx, w,
                "Sets the rendering resolution.\nHigher values are sharper but more demanding.\n\n(Default: 1920x1080)");

            DrawCycleRow("Anti-Aliasing", k_AA, ref m_AAIdx, w,
                "Smooths jagged edges on geometry.\n16x MSAA is the highest quality\nbut significantly impacts performance.\n\n(Default: 4x MSAA)");

            UI.Dummy(0f, 20f);
            SectionHeader("Window");
            DrawToggleRow("Fullscreen", ref m_Fullscreen, w,
                "Runs the game in exclusive fullscreen mode.\nMay improve performance on some systems.\n\n(Default: Off)");

            DrawToggleRow("VSync", ref m_VSync, w,
                "Synchronizes the frame rate with your\nmonitor refresh rate to prevent screen tearing.\n\n(Default: On)");
        }

        private void DrawShadows(float w)
        {
            SectionHeader("Shadow Quality");
            DrawToggleRow("Enable Shadows", ref m_Shadows, w,
                "Enables dynamic shadow casting.\nDisabling shadows greatly improves\nperformance on low-end hardware.\n\n(Default: On)");

            DrawCycleRow("Shadow Resolution", k_ShadowRes, ref m_ShadowIdx, w,
                "Controls the resolution of shadow maps.\n4096 (Ultra) gives the sharpest shadows\nbut requires a powerful GPU.\n\n(Default: 2048 High)");
        }

        private void DrawAudio(float w)
        {
            SectionHeader("Volume");
            
             
            DrawSliderRow("Master Volume", ref m_Master, 0f, 100f, w,
                "Controls the overall volume of all audio in the game.\n\n(Default: 100)");

            DrawSliderRow("Music Volume", ref m_Music, 0f, 100f, w,
                "Controls the volume of background music.\n\n(Default: 100)");

            DrawSliderRow("SFX Volume", ref m_SFX, 0f, 100f, w,
                "Controls the volume of sound effects.\n\n(Default: 100)");
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
            if (UI.Button(" < ##" + label))
            {
                index--;
                if (index < 0) index = options.Length - 1;
            }
            
            UI.SameLine(0f, 15f);
            UI.TextColored(options[index], 1f, 1f, 1f, 1f);
            
            UI.SameLine(0f, 15f);
            if (UI.Button(" > ##" + label))
            {
                index++;
                if (index >= options.Length) index = 0;
            }
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
            UI.SliderFloat("##sl_" + label, ref value, min, max);
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

            if (m_MuteAudio)
            {
                // Mute ALL audio channels, not just master
                Audio.SetMasterVolume(0f);
                Audio.SetMusicVolume(0f);
                Audio.SetSFXVolume(0f);
            }
            else
            {
                Audio.SetMasterVolume(m_Master / 100f);
                Audio.SetMusicVolume(m_Music / 100f);
                Audio.SetSFXVolume(m_SFX / 100f);
            }

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
            SettingsConfig.SyncAllAudioEntities();
        }

        private void ResetCategory()
        {
            switch (m_Tab)
            {
                case 0:
                    m_ResIdx     = 2;
                    m_AAIdx      = 2;
                    m_Fullscreen = false;
                    m_VSync      = true;
                    break;
                case 1:
                    m_Shadows   = true;
                    m_ShadowIdx = 2;
                    break;
                case 2:
                    m_MuteAudio = false;
                    m_Master    = 100f;
                    m_Music     = 100f;
                    m_SFX       = 100f;
                    break;
            }
        }
    }
}
