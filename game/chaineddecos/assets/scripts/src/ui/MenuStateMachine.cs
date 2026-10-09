using System;
using System.Collections.Generic;
using System.Globalization;
using Chained;

namespace ChainedDecos.Scripts
{
[AutoAttach("MenuStateMachine")]
    public class MenuStateMachine : Script
    {
        private enum State
        {
            Main, Settings, Info, Play, Host, Join, Lobby
        }

        // ── State ─────────────────────────────────────────────────────────────
        private State m_State     = State.Main;
        private State m_PrevState = State.Main;
        private int m_SettingsTab     = 0; // 0=Video 1=Shadows 2=Audio

        // ── Network ───────────────────────────────────────────────────────────
        private string m_HostPort   = "7777";
        private string m_JoinIP     = "127.0.0.1";
        private string m_JoinPort   = "7777";
        private string m_PlayerName = "Player";

        // ── Video Settings ────────────────────────────────────────────────────
        private static readonly string[] k_Resolutions = { "1280x720", "1600x900", "1920x1080", "2560x1440", "3840x2160" };
        private static readonly string[] k_AA          = { "Off", "2x MSAA", "4x MSAA", "8x MSAA", "16x MSAA" };
        private static readonly int[]    k_AAValues    = { 0, 2, 4, 8, 16 };
        private int  m_ResIdx     = 2;
        private int  m_AAIdx      = 2;
        private bool m_Fullscreen = false;
        private bool m_VSync      = true;

        // ── Shadow Settings ───────────────────────────────────────────────────
        private static readonly string[] k_ShadowRes  = { "512 (Low)", "1024 (Med)", "2048 (High)", "4096 (Ultra)" };
        private static readonly int[]    k_ShadowVals = { 512, 1024, 2048, 4096 };
        private bool m_Shadows   = true;
        private int  m_ShadowIdx = 2;

        // ── Audio Settings ────────────────────────────────────────────────────
        private bool  m_MuteAudio = false;
        private float m_Master    = 100f;
        private float m_Music     = 100f;
        private float m_SFX       = 100f;

        // ── Hover description ─────────────────────────────────────────────────
        private string m_Desc = "Hover over a setting to see its description.";

        // ── Scene paths ───────────────────────────────────────────────────────
        public string GameScene  = "scenes/game_mode_selection.chscene";
        public string LobbyScene = "scenes/lobby.chscene";

        // ── Lifecycle ─────────────────────────────────────────────────────────
        public override void OnCreate()
        {
            // Restore settings values from engine
            m_VSync       = AppWindow.GetVSync();
            m_Fullscreen  = AppWindow.GetFullscreen();
            m_Shadows     = AppWindow.GetEnableShadows();
            m_Master      = Audio.GetMasterVolume() * 100f;
            m_Music       = Audio.GetMusicVolume()  * 100f;
            m_SFX         = Audio.GetSFXVolume()    * 100f;
            m_MuteAudio   = m_Master < 0.5f;

            // Reset any leftover game state
            SpectatorState.Reset();
            GameHUD.IsPaused = false;

            // Set initial state
            GoTo(Scene.HasActiveSession() ? State.Main : State.Main);
        }

        public override void OnUpdate(float deltaTime)
        {
            // Auto-enter lobby when fully connected
            if (Network.IsFullyConnected && m_State != State.Lobby)
                GoTo(State.Lobby);
            else if (!Network.IsConnected && m_State == State.Lobby)
                GoTo(State.Main);
        }

        // ── State Machine ─────────────────────────────────────────────────────
        private void GoTo(State next)
        {
            m_PrevState = m_State;
            m_State     = next;
        }

        private void GoBack()
        {
            GoTo(m_PrevState);
        }

        // ── Rendering ─────────────────────────────────────────────────────────
        public override void OnGUI()
        {
            switch (m_State)
            {
                case State.Main:            DrawMain();         break;
                case State.Settings:        DrawSettings();     break;
                case State.Info:            DrawInfo();         break;
                case State.Play:            DrawPlay();         break;
                case State.Host:            DrawHost();         break;
                case State.Join:            DrawJoin();         break;
                case State.Lobby:           DrawLobby();        break;
            }
        }

        // ── MAIN ──────────────────────────────────────────────────────────────
        private void DrawMain()
        {
            Vector2 d = UI.GetDisplaySize();
            float w = 340f, h = Scene.HasActiveSession() ? 380f : 320f;
            UI.BeginWindow("##Main", (d.X - w) * 0.5f, (d.Y - h) * 0.5f, w, h, 0.0f);

            // Logo text
            UI.SetWindowFontScale(1.6f);
            UI.TextColored("CHAINED DECOS", 1f, 1f, 1f, 1f);
            UI.SetWindowFontScale(0.85f);
            UI.TextColored("Pre-Alpha v0.1", 0.45f, 0.45f, 0.45f, 1f);
            UI.SetWindowFontScale(1.0f);
            UI.Dummy(0f, 24f);

            if (Scene.HasActiveSession())
            {
                MenuBtn("Resume", () => Scene.ResumeSession());
                UI.Dummy(0f, 4f);
            }

            MenuBtn("Start", () => GoTo(State.Play));
            UI.Dummy(0f, 4f);
            MenuBtn("Options", () => GoTo(State.Settings));
            UI.Dummy(0f, 4f);
            MenuBtn("Info", () => GoTo(State.Info));
            UI.Dummy(0f, 4f);
            MenuBtn("Exit", () => Application.Close());

            UI.EndWindow();
        }

        // ── PLAY SELECT ───────────────────────────────────────────────────────
        private void DrawPlay()
        {
            SmallWindow("PLAY", 300f, 240f, () =>
            {
                MenuBtn("Host Game", () => GoTo(State.Host));
                UI.Dummy(0f, 6f);
                MenuBtn("Join Game", () => GoTo(State.Join));
                UI.Dummy(0f, 16f);
                UI.Separator();
                BackBtn();
            });
        }

        // ── HOST ──────────────────────────────────────────────────────────────
        private void DrawHost()
        {
            SmallWindow("HOST SERVER", 340f, 280f, () =>
            {
                UI.Text("Player Name");
                UI.SetNextItemWidth(240f);
                UI.InputText("##hname", ref m_PlayerName, 32);
                UI.Dummy(0f, 8f);
                UI.Text("Port");
                UI.SetNextItemWidth(120f);
                UI.InputText("##hport", ref m_HostPort, 8);
                UI.Dummy(0f, 16f);
                if (UI.Button("   Start Server   "))
                {
                    if (ushort.TryParse(m_HostPort, out ushort port))
                    {
                        Network.SetLocalPlayerInfo(m_PlayerName, 0);
                        Network.HostGame(port, 4);
                    }
                }
                UI.Separator();
                BackBtn();
            });
        }

        // ── JOIN ──────────────────────────────────────────────────────────────
        private void DrawJoin()
        {
            SmallWindow("JOIN GAME", 340f, 320f, () =>
            {
                UI.Text("Player Name");
                UI.SetNextItemWidth(240f);
                UI.InputText("##jname", ref m_PlayerName, 32);
                UI.Dummy(0f, 8f);
                UI.Text("IP Address");
                UI.SetNextItemWidth(200f);
                UI.InputText("##jip", ref m_JoinIP, 32);
                UI.Dummy(0f, 8f);
                UI.Text("Port");
                UI.SetNextItemWidth(120f);
                UI.InputText("##jport", ref m_JoinPort, 8);
                UI.Dummy(0f, 16f);
                if (UI.Button("   Connect   "))
                {
                    if (ushort.TryParse(m_JoinPort, out ushort port))
                    {
                        Network.SetLocalPlayerInfo(m_PlayerName, 0);
                        Network.ConnectTo(m_JoinIP, port);
                    }
                }
                UI.Separator();
                BackBtn();
            });
        }

        // ── LOBBY ─────────────────────────────────────────────────────────────
        private void DrawLobby()
        {
            SmallWindow("LOBBY", 360f, 260f, () =>
            {
                if (Network.IsHost)
                {
                    UI.TextColored("You are the Host", 0.3f, 1.0f, 0.5f, 1f);
                    UI.Dummy(0f, 16f);
                    if (UI.Button("   >> START GAME <<   "))
                        Scene.LoadScene(GameScene);
                }
                else
                {
                    UI.TextColored("Connected as Client", 0.3f, 0.8f, 1.0f, 1f);
                    UI.TextColored("Waiting for host to start...", 0.6f, 0.6f, 0.6f, 1f);
                }
                UI.Dummy(0f, 20f);
                UI.Separator();
                if (UI.Button("   Disconnect   "))
                {
                    Network.Disconnect();
                    GoTo(State.Main);
                }
            });
        }

        // ── INFO ──────────────────────────────────────────────────────────────
        private void DrawInfo()
        {
            SmallWindow("INFO", 400f, 200f, () =>
            {
                UI.TextColored("Chained Decos", 0.9f, 0.9f, 1f, 1f);
                UI.Text("The game is currently in development.");
                UI.Dummy(0f, 16f);
                UI.Separator();
                BackBtn();
            });
        }

        // ── SETTINGS (tabbed, RE-style) ───────────────────────────────────────
        private static readonly string[] k_SettingsTabs = { "VIDEO", "SHADOWS", "AUDIO" };

        private void DrawSettings()
        {
            Vector2 d = UI.GetDisplaySize();
            float w = d.X * 0.75f, h = d.Y * 0.82f;
            UI.BeginWindow("##Settings", (d.X - w) * 0.5f, (d.Y - h) * 0.5f, w, h, 0.95f);

            // Header
            UI.SetWindowFontScale(1.5f);
            UI.TextColored("SETTINGS", 1f, 1f, 1f, 1f);
            UI.SetWindowFontScale(1.0f);
            UI.Dummy(0f, 12f);

            // Tabs
            for (int i = 0; i < k_SettingsTabs.Length; i++)
            {
                if (i > 0) UI.SameLine(0f, 10f);
                bool active = (m_SettingsTab == i);
                UI.PushStyleColor(21, active ? 0.15f : 0.08f, active ? 0.5f : 0.08f, active ? 0.75f : 0.08f, 1f);
                if (UI.Button("   " + k_SettingsTabs[i] + "   ")) m_SettingsTab = i;
                UI.PopStyleColor(1);
            }

            UI.Dummy(0f, 8f);
            UI.Separator();
            UI.Dummy(0f, 16f);

            // Split layout
            float leftW  = w * 0.55f;
            float rightW = w * 0.36f;
            float paneH  = h * 0.68f;

            UI.BeginChild("##SLeft", leftW, paneH, false);
            switch (m_SettingsTab)
            {
                case 0: DrawVideoTab(leftW);   break;
                case 1: DrawShadowTab(leftW);  break;
                case 2: DrawAudioTab(leftW);   break;
            }
            UI.EndChild();

            UI.SameLine(0f, 30f);

            UI.BeginChild("##SRight", rightW, paneH, false);
            UI.Dummy(0f, 16f);
            UI.BeginChild("##SDesc", rightW - 16f, 140f, true);
            UI.TextColored(m_Desc, 0.78f, 0.85f, 0.80f, 1f);
            UI.EndChild();
            UI.EndChild();

            // Bottom bar
            UI.Separator();
            UI.Dummy(0f, 12f);
            UI.PushStyleColor(21, 0.15f, 0.5f, 0.75f, 1f);
            if (UI.Button("   Apply   ")) ApplySettings();
            UI.PopStyleColor(1);
            UI.SameLine(0f, 16f);
            if (UI.Button("   Reset Category   ")) ResetSettingsCategory();
            UI.SameLine(0f, 16f);
            // Back without scene load — session stays alive!
            if (UI.Button("   Back   ")) GoTo(State.Main);

            UI.EndWindow();
        }

        private void DrawVideoTab(float w)
        {
            SettingsHeader("Display");
            CycleRow("Resolution",    k_Resolutions, ref m_ResIdx, w,
                "Sets the rendering resolution.\n\n(Default: 1920x1080)");
            CycleRow("Anti-Aliasing", k_AA, ref m_AAIdx, w,
                "Smooths jagged edges.\n\n(Default: 4x MSAA)");
            UI.Dummy(0f, 16f);
            SettingsHeader("Window");
            ToggleRow("Fullscreen", ref m_Fullscreen, w,
                "Exclusive fullscreen mode.\n\n(Default: Off)");
            ToggleRow("VSync", ref m_VSync, w,
                "Sync framerate to monitor refresh rate.\n\n(Default: On)");
        }

        private void DrawShadowTab(float w)
        {
            SettingsHeader("Shadow Quality");
            ToggleRow("Enable Shadows", ref m_Shadows, w,
                "Dynamic shadow casting.\n\n(Default: On)");
            CycleRow("Shadow Resolution", k_ShadowRes, ref m_ShadowIdx, w,
                "Shadow map resolution.\n\n(Default: 2048 High)");
        }

        private void DrawAudioTab(float w)
        {
            SettingsHeader("Volume");
            ToggleRow("Mute All Audio", ref m_MuteAudio, w,
                "Mutes all audio channels.\n\n(Default: Off)");
            SliderRow("Master Volume", ref m_Master, 0f, 100f, w,
                "Overall audio volume.\n\n(Default: 100)");
            SliderRow("Music Volume", ref m_Music, 0f, 100f, w,
                "Background music volume.\n\n(Default: 100)");
            SliderRow("SFX Volume", ref m_SFX, 0f, 100f, w,
                "Sound effects volume.\n\n(Default: 100)");
        }

        // ── Settings helpers ──────────────────────────────────────────────────
        private void SettingsHeader(string title)
        {
            UI.TextColored(title, 0.35f, 0.75f, 1.0f, 1f);
            UI.Separator();
            UI.Dummy(0f, 8f);
        }

        private void CycleRow(string label, string[] opts, ref int idx, float w, string desc)
        {
            UI.Text(label);
            if (UI.IsItemHovered()) m_Desc = desc;
            UI.SameLine(w - 270f, -1f);
            UI.PushStyleColor(21, 0.12f, 0.12f, 0.12f, 1f);
            if (UI.Button(" < ##" + label)) { idx--; if (idx < 0) idx = opts.Length - 1; }
            UI.SameLine(0f, 12f);
            UI.TextColored(opts[idx], 1f, 1f, 1f, 1f);
            UI.SameLine(0f, 12f);
            if (UI.Button(" > ##" + label)) { idx++; if (idx >= opts.Length) idx = 0; }
            UI.PopStyleColor(1);
            UI.Dummy(0f, 8f);
        }

        private void ToggleRow(string label, ref bool val, float w, string desc)
        {
            UI.Checkbox(label, ref val);
            if (UI.IsItemHovered()) m_Desc = desc;
            UI.Dummy(0f, 8f);
        }

        private void SliderRow(string label, ref float val, float min, float max, float w, string desc)
        {
            UI.Text(label);
            if (UI.IsItemHovered()) m_Desc = desc;
            UI.SameLine(w - 280f, -1f);
            UI.SetNextItemWidth(230f);
            UI.SliderFloat("##s_" + label, ref val, min, max);
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
            { Audio.SetMasterVolume(0f); Audio.SetMusicVolume(0f); Audio.SetSFXVolume(0f); }
            else
            { Audio.SetMasterVolume(m_Master / 100f); Audio.SetMusicVolume(m_Music / 100f); Audio.SetSFXVolume(m_SFX / 100f); }

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

        private void ResetSettingsCategory()
        {
            switch (m_SettingsTab)
            {
                case 0: m_ResIdx = 2; m_AAIdx = 2; m_Fullscreen = false; m_VSync = true; break;
                case 1: m_Shadows = true; m_ShadowIdx = 2; break;
                case 2: m_MuteAudio = false; m_Master = 100f; m_Music = 100f; m_SFX = 100f; break;
            }
        }

        // ── Generic UI helpers ────────────────────────────────────────────────
        private void MenuBtn(string label, Action action)
        {
            Vector2 d = UI.GetDisplaySize();
            UI.SetNextItemWidth(280f);
            if (UI.Button("  " + label + "  ")) action();
        }

        private void BackBtn() { if (UI.Button("  < Back  ")) GoBack(); }

        private void SmallWindow(string title, float w, float h, Action content)
        {
            Vector2 d = UI.GetDisplaySize();
            UI.BeginWindow("##" + title, (d.X - w) * 0.5f, (d.Y - h) * 0.5f, w, h, 0.92f);
            UI.SetWindowFontScale(1.3f);
            UI.TextColored(title, 0.9f, 0.9f, 1f, 1f);
            UI.SetWindowFontScale(1.0f);
            UI.Dummy(0f, 12f);
            content();
            UI.EndWindow();
        }
    }
}
