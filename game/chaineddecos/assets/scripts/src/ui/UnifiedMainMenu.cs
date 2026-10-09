using System;
using System.Collections.Generic;
using System.Globalization;
using Chained;

namespace ChainedDecos.Scripts
{
    [AutoAttach("UnifiedMainMenu")]
    public class UnifiedMainMenu : Script
    {
        private enum MenuScreen
        {
            Main,
            PlaySelect,
            HostSetup,
            JoinSetup,
            Lobby,
            Settings,
            Info
        }

        private MenuScreen m_Screen = MenuScreen.Main;
        private MenuScreen m_PrevScreen = MenuScreen.Main;

        // ── Settings Data ─────────────────────────────────────────────────────
        private int m_SettingsTab = 0; // 0=Video, 1=Shadows, 2=Audio
        private static readonly string[] k_SettingsTabs = { "VIDEO", "SHADOWS", "AUDIO" };

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
        private float m_Master    = 100f;
        private float m_Music     = 100f;
        private float m_SFX       = 100f;

        private string m_Desc = "Hover over any setting to view details.";

        // ── Multiplayer Setup Data ────────────────────────────────────────────
        private string m_PlayerName = "Player";
        private string m_HostPort   = "7777";
        private string m_JoinIP     = "127.0.0.1";
        private string m_JoinPort   = "7777";

        // ── Maps ──────────────────────────────────────────────────────────────
        public string DefaultGameScene = "scenes/rpg_strategy_scene.chscene";
        public string DefaultMpGameScene = "scenes/rpg_strategy_scene_mp.chscene";

        public override void OnCreate()
        {
            m_Screen = MenuScreen.Main;
            
            // Load engine settings
            m_VSync       = AppWindow.GetVSync();
            m_Fullscreen  = AppWindow.GetFullscreen();
            m_Shadows     = AppWindow.GetEnableShadows();
            m_Master      = Audio.GetMasterVolume() * 100f;
            m_Music       = Audio.GetMusicVolume()  * 100f;
            m_SFX         = Audio.GetSFXVolume()    * 100f;
            m_MuteAudio   = (m_Master < 0.5f && m_Music < 0.5f && m_SFX < 0.5f);

            SpectatorState.Reset();
            GameHUD.IsPaused = false;
        }

        public override void OnUpdate(float deltaTime)
        {
            // Auto switch to Lobby if network connects
            if (Network.IsFullyConnected && m_Screen != MenuScreen.Lobby)
            {
                SetScreen(MenuScreen.Lobby);
            }
            else if (!Network.IsConnected && m_Screen == MenuScreen.Lobby)
            {
                SetScreen(MenuScreen.Main);
            }
        }

        private void SetScreen(MenuScreen screen)
        {
            m_PrevScreen = m_Screen;
            m_Screen = screen;
        }

        private void GoBack()
        {
            SetScreen(m_PrevScreen);
        }

        public override void OnGUI()
        {
            switch (m_Screen)
            {
                case MenuScreen.Main:       DrawMainMenu();   break;
                case MenuScreen.PlaySelect: DrawPlaySelect(); break;
                case MenuScreen.HostSetup:  DrawHostSetup();  break;
                case MenuScreen.JoinSetup:  DrawJoinSetup();  break;
                case MenuScreen.Lobby:      DrawLobby();      break;
                case MenuScreen.Settings:   DrawSettings();   break;
                case MenuScreen.Info:       DrawInfo();       break;
            }
        }

        // ── Main Menu Screen ──────────────────────────────────────────────────
        private void DrawMainMenu()
        {
            Vector2 disp = UI.GetDisplaySize();
            float winW = 360f;
            float winH = Scene.HasActiveSession() ? 440f : 380f;
            float winX = (disp.X - winW) * 0.5f;
            float winY = (disp.Y - winH) * 0.5f;

            UI.BeginWindow("##MainMenuWindow", winX, winY, winW, winH, 0.0f);

            // Title / Logo
            UI.Dummy(0f, 10f);
            UI.SetWindowFontScale(1.8f);
            UI.TextColored("CHAINED DECOS", 1.0f, 1.0f, 1.0f, 1.0f);
            UI.SetWindowFontScale(0.85f);
            UI.TextColored("Pre-Alpha v0.1", 0.45f, 0.45f, 0.45f, 1.0f);
            UI.SetWindowFontScale(1.0f);
            UI.Dummy(0f, 20f);

            // Resume Button (Active session preservation)
            if (Scene.HasActiveSession())
            {
                UI.PushStyleColor(21, 0.15f, 0.55f, 0.35f, 1f); // Greenish button for resume
                if (UI.Button("        RESUME GAME        "))
                {
                    Log.Info("[UnifiedMainMenu] Resuming active game session...");
                    Scene.ResumeSession();
                }
                UI.PopStyleColor(1);
                UI.Dummy(0f, 8f);
            }

            // Start Game
            if (UI.Button("           START           "))
            {
                SetScreen(MenuScreen.PlaySelect);
            }
            UI.Dummy(0f, 6f);

            // Options / Settings
            if (UI.Button("          OPTIONS          "))
            {
                SetScreen(MenuScreen.Settings);
            }
            UI.Dummy(0f, 6f);

            // Info
            if (UI.Button("           INFO            "))
            {
                SetScreen(MenuScreen.Info);
            }
            UI.Dummy(0f, 6f);

            // Quit
            if (UI.Button("           EXIT            "))
            {
                Application.Close();
            }

            UI.EndWindow();
        }

        // ── Play Select Screen ────────────────────────────────────────────────
        private void DrawPlaySelect()
        {
            DrawModalBox("SELECT MODE", 360f, 280f, () =>
            {
                if (UI.Button("     Singleplayer Game     "))
                {
                    Scene.LoadScene(DefaultGameScene);
                }
                UI.Dummy(0f, 8f);

                if (UI.Button("        Host Server        "))
                {
                    SetScreen(MenuScreen.HostSetup);
                }
                UI.Dummy(0f, 8f);

                if (UI.Button("      Join Multiplayer     "))
                {
                    SetScreen(MenuScreen.JoinSetup);
                }
                UI.Dummy(0f, 16f);

                UI.Separator();
                UI.Dummy(0f, 6f);

                if (UI.Button("          < Back           "))
                {
                    SetScreen(MenuScreen.Main);
                }
            });
        }

        // ── Host Setup Screen ─────────────────────────────────────────────────
        private void DrawHostSetup()
        {
            DrawModalBox("HOST MULTIPLAYER", 380f, 320f, () =>
            {
                UI.Text("Nickname:");
                UI.SetNextItemWidth(260f);
                UI.InputText("##host_nick", ref m_PlayerName, 32);
                UI.Dummy(0f, 8f);

                UI.Text("Port:");
                UI.SetNextItemWidth(140f);
                UI.InputText("##host_port", ref m_HostPort, 8);
                UI.Dummy(0f, 16f);

                if (UI.Button("       Start Server       "))
                {
                    if (ushort.TryParse(m_HostPort, out ushort port))
                    {
                        Network.SetLocalPlayerInfo(m_PlayerName, 0);
                        Network.HostGame(port, 4);
                    }
                }
                UI.Dummy(0f, 12f);
                UI.Separator();
                UI.Dummy(0f, 6f);

                if (UI.Button("          < Back           "))
                {
                    SetScreen(MenuScreen.PlaySelect);
                }
            });
        }

        // ── Join Setup Screen ─────────────────────────────────────────────────
        private void DrawJoinSetup()
        {
            DrawModalBox("JOIN MULTIPLAYER", 380f, 360f, () =>
            {
                UI.Text("Nickname:");
                UI.SetNextItemWidth(260f);
                UI.InputText("##join_nick", ref m_PlayerName, 32);
                UI.Dummy(0f, 8f);

                UI.Text("Host IP:");
                UI.SetNextItemWidth(260f);
                UI.InputText("##join_ip", ref m_JoinIP, 32);
                UI.Dummy(0f, 8f);

                UI.Text("Port:");
                UI.SetNextItemWidth(140f);
                UI.InputText("##join_port", ref m_JoinPort, 8);
                UI.Dummy(0f, 16f);

                if (UI.Button("         Connect          "))
                {
                    if (ushort.TryParse(m_JoinPort, out ushort port))
                    {
                        Network.SetLocalPlayerInfo(m_PlayerName, 0);
                        Network.ConnectTo(m_JoinIP, port);
                    }
                }
                UI.Dummy(0f, 12f);
                UI.Separator();
                UI.Dummy(0f, 6f);

                if (UI.Button("          < Back           "))
                {
                    SetScreen(MenuScreen.PlaySelect);
                }
            });
        }

        // ── Lobby Screen ──────────────────────────────────────────────────────
        private void DrawLobby()
        {
            DrawModalBox("MULTIPLAYER LOBBY", 420f, 300f, () =>
            {
                if (Network.IsHost)
                {
                    UI.TextColored("[HOST STATUS] Active Server", 0.3f, 1.0f, 0.5f, 1f);
                    UI.Dummy(0f, 10f);

                    UI.PushStyleColor(21, 0.15f, 0.6f, 0.35f, 1f);
                    if (UI.Button("     >>> START MATCH <<<     "))
                    {
                        Scene.LoadScene(DefaultMpGameScene);
                    }
                    UI.PopStyleColor(1);
                }
                else
                {
                    UI.TextColored("[CLIENT STATUS] Connected to Server", 0.3f, 0.8f, 1.0f, 1f);
                    UI.Dummy(0f, 10f);
                    UI.TextColored("Waiting for the host to launch the game...", 0.65f, 0.65f, 0.65f, 1f);
                }

                UI.Dummy(0f, 20f);
                UI.Separator();
                UI.Dummy(0f, 6f);

                if (UI.Button("        Disconnect         "))
                {
                    Network.Disconnect();
                    SetScreen(MenuScreen.Main);
                }
            });
        }

        // ── Info Screen ───────────────────────────────────────────────────────
        private void DrawInfo()
        {
            DrawModalBox("INFORMATION", 440f, 240f, () =>
            {
                UI.TextColored("Chained Decos", 0.9f, 0.9f, 1.0f, 1.0f);
                UI.Text("Multiplayer Action Game.");
                UI.TextColored("Developed with Custom C++ Engine.", 0.6f, 0.6f, 0.6f, 1.0f);
                UI.Dummy(0f, 20f);
                UI.Separator();
                UI.Dummy(0f, 6f);

                if (UI.Button("          < Back           "))
                {
                    SetScreen(MenuScreen.Main);
                }
            });
        }

        // ── RE-Style Settings Screen ──────────────────────────────────────────
        private void DrawSettings()
        {
            Vector2 d = UI.GetDisplaySize();
            float w = d.X * 0.80f;
            float h = d.Y * 0.82f;
            float x = (d.X - w) * 0.5f;
            float y = (d.Y - h) * 0.5f;

            UI.BeginWindow("##SettingsWindow", x, y, w, h, 0.96f);

            // Header
            UI.Dummy(0f, 10f);
            UI.SetWindowFontScale(1.6f);
            UI.TextColored("OPTIONS & SETTINGS", 1.0f, 1.0f, 1.0f, 1.0f);
            UI.SetWindowFontScale(1.0f);
            UI.Dummy(0f, 12f);

            // Tab Bar
            for (int i = 0; i < k_SettingsTabs.Length; i++)
            {
                if (i > 0) UI.SameLine(0f, 12f);

                bool active = (m_SettingsTab == i);
                if (active) UI.PushStyleColor(21, 0.15f, 0.55f, 0.75f, 1f);
                else        UI.PushStyleColor(21, 0.10f, 0.10f, 0.10f, 1f);

                if (UI.Button("     " + k_SettingsTabs[i] + "     "))
                {
                    m_SettingsTab = i;
                }
                UI.PopStyleColor(1);
            }

            UI.Dummy(0f, 8f);
            UI.Separator();
            UI.Dummy(0f, 14f);

            // Split View: Left (Controls), Right (Description)
            float leftW  = w * 0.56f;
            float rightW = w * 0.36f;
            float paneH  = h * 0.64f;

            UI.BeginChild("##LeftSettingsPane", leftW, paneH, false);
            switch (m_SettingsTab)
            {
                case 0: DrawVideoTab(leftW);   break;
                case 1: DrawShadowTab(leftW);  break;
                case 2: DrawAudioTab(leftW);   break;
            }
            UI.EndChild();

            UI.SameLine(0f, 30f);

            UI.BeginChild("##RightSettingsPane", rightW, paneH, false);
            UI.Dummy(0f, 14f);
            UI.BeginChild("##DescBoxChild", rightW - 14f, 150f, true);
            UI.TextColored(m_Desc, 0.8f, 0.88f, 0.84f, 1.0f);
            UI.EndChild();
            UI.EndChild();

            // Bottom Actions Bar
            UI.Separator();
            UI.Dummy(0f, 10f);

            UI.PushStyleColor(21, 0.15f, 0.55f, 0.75f, 1f);
            if (UI.Button("     Apply Settings     "))
            {
                ApplySettings();
            }
            UI.PopStyleColor(1);

            UI.SameLine(0f, 18f);

            if (UI.Button("     Reset Tab     "))
            {
                ResetCurrentTab();
            }

            UI.SameLine(0f, 18f);

            // Back simply returns to Main state — NO SCENE RELOAD = SESSION PRESERVED!
            if (UI.Button("         < Back         "))
            {
                SetScreen(MenuScreen.Main);
            }

            UI.EndWindow();
        }

        private void DrawVideoTab(float w)
        {
            DrawSectionHeader("Display");
            DrawCycleRow("Resolution", k_Resolutions, ref m_ResIdx, w,
                "Configures the render resolution.\n(Default: 1920x1080)");

            DrawCycleRow("Anti-Aliasing", k_AA, ref m_AAIdx, w,
                "Smooths polygon edges via Multi-Sample Anti-Aliasing.\n(Default: 4x MSAA)");

            UI.Dummy(0f, 16f);
            DrawSectionHeader("Window Mode");
            DrawToggleRow("Fullscreen", ref m_Fullscreen, w,
                "Toggles fullscreen display mode.\n(Default: Off)");

            DrawToggleRow("VSync", ref m_VSync, w,
                "Locks framerate to your display refresh rate to eliminate tearing.\n(Default: On)");
        }

        private void DrawShadowTab(float w)
        {
            DrawSectionHeader("Shadows");
            DrawToggleRow("Enable Shadows", ref m_Shadows, w,
                "Enables dynamic shadow mapping.\n(Default: On)");

            DrawCycleRow("Shadow Resolution", k_ShadowRes, ref m_ShadowIdx, w,
                "Texture resolution for shadow maps.\n(Default: 2048 High)");
        }

        private void DrawAudioTab(float w)
        {
            DrawSectionHeader("Audio Channels");
            DrawToggleRow("Mute All Audio", ref m_MuteAudio, w,
                "Silences all sound effects, ambient tracks, and music.\n(Default: Off)");

            DrawSliderRow("Master Volume", ref m_Master, 0f, 100f, w,
                "Overall sound output volume.\n(Default: 100)");

            DrawSliderRow("Music Volume", ref m_Music, 0f, 100f, w,
                "Volume of background music tracks.\n(Default: 100)");

            DrawSliderRow("SFX Volume", ref m_SFX, 0f, 100f, w,
                "Volume of in-game sound effects & UI.\n(Default: 100)");
        }

        private void DrawSectionHeader(string title)
        {
            UI.TextColored(title, 0.35f, 0.75f, 1.0f, 1.0f);
            UI.Separator();
            UI.Dummy(0f, 8f);
        }

        private void DrawCycleRow(string label, string[] options, ref int index, float w, string desc)
        {
            UI.Text(label);
            if (UI.IsItemHovered()) m_Desc = desc;

            UI.SameLine(w - 270f, -1f);
            UI.PushStyleColor(21, 0.12f, 0.12f, 0.12f, 1f);
            if (UI.Button(" < ##" + label))
            {
                index--;
                if (index < 0) index = options.Length - 1;
            }

            UI.SameLine(0f, 12f);
            UI.TextColored(options[index], 1.0f, 1.0f, 1.0f, 1.0f);

            UI.SameLine(0f, 12f);
            if (UI.Button(" > ##" + label))
            {
                index++;
                if (index >= options.Length) index = 0;
            }
            UI.PopStyleColor(1);
            UI.Dummy(0f, 8f);
        }

        private void DrawToggleRow(string label, ref bool val, float w, string desc)
        {
            UI.Checkbox(label, ref val);
            if (UI.IsItemHovered()) m_Desc = desc;
            UI.Dummy(0f, 8f);
        }

        private void DrawSliderRow(string label, ref float val, float min, float max, float w, string desc)
        {
            UI.Text(label);
            if (UI.IsItemHovered()) m_Desc = desc;

            UI.SameLine(w - 280f, -1f);
            UI.SetNextItemWidth(230f);
            UI.SliderFloat("##slider_" + label, ref val, min, max);
            UI.Dummy(0f, 8f);
        }

        private void ApplySettings()
        {
            string[] parts = k_Resolutions[m_ResIdx].Split('x');
            if (parts.Length == 2 && int.TryParse(parts[0], out int rw) && int.TryParse(parts[1], out int rh))
                AppWindow.SetSize(rw, rh);

            AppWindow.SetFullscreen(m_Fullscreen);
            AppWindow.SetVSync(m_VSync);
            AppWindow.SetEnableShadows(m_Shadows);
            AppWindow.SetAntiAliasingSamples(k_AAValues[m_AAIdx]);
            AppWindow.SetShadowResolution(k_ShadowVals[m_ShadowIdx]);

            if (m_MuteAudio)
            {
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
            Log.Info("[UnifiedMainMenu] Settings saved and applied successfully.");
        }

        private void ResetCurrentTab()
        {
            switch (m_SettingsTab)
            {
                case 0:
                    m_ResIdx = 2;
                    m_AAIdx = 2;
                    m_Fullscreen = false;
                    m_VSync = true;
                    break;
                case 1:
                    m_Shadows = true;
                    m_ShadowIdx = 2;
                    break;
                case 2:
                    m_MuteAudio = false;
                    m_Master = 100f;
                    m_Music = 100f;
                    m_SFX = 100f;
                    break;
            }
        }

        private void DrawModalBox(string title, float w, float h, Action renderBody)
        {
            Vector2 d = UI.GetDisplaySize();
            UI.BeginWindow("##Modal_" + title, (d.X - w) * 0.5f, (d.Y - h) * 0.5f, w, h, 0.94f);
            UI.Dummy(0f, 8f);
            UI.SetWindowFontScale(1.3f);
            UI.TextColored(title, 0.9f, 0.9f, 1.0f, 1.0f);
            UI.SetWindowFontScale(1.0f);
            UI.Dummy(0f, 10f);
            UI.Separator();
            UI.Dummy(0f, 12f);
            renderBody();
            UI.EndWindow();
        }
    }
}
