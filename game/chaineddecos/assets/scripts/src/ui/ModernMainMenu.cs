using System;
using Chained;

namespace ChainedDecos.Scripts
{
    public enum MainMenuState
    {
        Main,
        PlaySelect,
        Host,
        Join,
        Settings,
        Lobby
    }

    [AutoAttach("MainMenu")]
    public class ModernMainMenu : Script
    {
        private MainMenuState m_State = MainMenuState.Main;
        
        // Host & Join states
        private string m_HostPort = "7777";
        private string m_JoinIP = "127.0.0.1";
        private string m_JoinPort = "7777";
        private string m_PlayerName = "Player";

        // Settings states (mock values, linked to AppWindow where possible)
        private bool m_VSync = true;
        private bool m_Fullscreen = false;
        private float m_MasterVolume = 100.0f;

        public override void OnCreate()
        {
            m_State = MainMenuState.Main;
            m_VSync = AppWindow.GetVSync();
            m_Fullscreen = AppWindow.GetFullscreen();
            Log.Info("ModernMainMenu: Initialized.");
        }

        public override void OnUpdate(float deltaTime)
        {
            // Auto-transition to Lobby if network connects
            if (Network.IsConnected && m_State != MainMenuState.Lobby)
            {
                m_State = MainMenuState.Lobby;
            }
            // Auto-transition back if disconnected
            else if (!Network.IsConnected && m_State == MainMenuState.Lobby)
            {
                m_State = MainMenuState.Main;
            }
        }

        public override void OnGUI()
        {
            Vector2 display = UI.GetDisplaySize();
            float winW = 600.0f;
            float winH = 450.0f;
            float winX = (display.X - winW) * 0.5f;
            float winY = (display.Y - winH) * 0.5f;

            // Make the window look modern and dark
            UI.BeginWindow("##MainMenuWindow", winX, winY, winW, winH, 0.90f);

            // Title Header
            UI.SetWindowFontScale(1.5f);
            UI.TextColored("CHAINED DECOS", 0.9f, 0.9f, 1.0f, 1.0f);
            UI.SetWindowFontScale(1.0f);
            UI.TextColored("Pre-Alpha v0.1", 0.5f, 0.5f, 0.5f, 1.0f);
            UI.Separator();
            UI.Dummy(0.0f, 20.0f); // Spacing

            // Render active state
            switch (m_State)
            {
                case MainMenuState.Main: DrawMain(); break;
                case MainMenuState.PlaySelect: DrawPlaySelect(); break;
                case MainMenuState.Host: DrawHost(); break;
                case MainMenuState.Join: DrawJoin(); break;
                case MainMenuState.Settings: DrawSettings(); break;
                case MainMenuState.Lobby: DrawLobby(); break;
            }

            UI.EndWindow();
        }

        private void DrawMain()
        {
            if (UI.Button(" Play ")) m_State = MainMenuState.PlaySelect;
            UI.Dummy(0.0f, 10.0f);
            if (UI.Button(" Settings ")) m_State = MainMenuState.Settings;
            UI.Dummy(0.0f, 10.0f);
            if (UI.Button(" Quit ")) Application.Close();
        }

        private void DrawPlaySelect()
        {
            UI.Text("=== SELECT MODE ===");
            UI.Dummy(0.0f, 10.0f);
            if (UI.Button(" Host Game ")) m_State = MainMenuState.Host;
            UI.Dummy(0.0f, 5.0f);
            if (UI.Button(" Join Game ")) m_State = MainMenuState.Join;
            UI.Dummy(0.0f, 20.0f);
            UI.Separator();
            if (UI.Button(" <-- Back ")) m_State = MainMenuState.Main;
        }

        private void DrawHost()
        {
            UI.Text("=== HOST SERVER ===");
            UI.InputText("Player Name", ref m_PlayerName, 32);
            UI.InputText("Port", ref m_HostPort, 10);
            
            UI.Dummy(0.0f, 20.0f);
            if (UI.Button(" Start Server! "))
            {
                if (ushort.TryParse(m_HostPort, out ushort port))
                {
                    Log.Info($"Starting server on port {port}...");
                    Network.HostGame(port, 4); // Max 4 clients
                }
            }
            UI.Separator();
            if (UI.Button(" <-- Back ")) m_State = MainMenuState.PlaySelect;
        }

        private void DrawJoin()
        {
            UI.Text("=== JOIN GAME ===");
            UI.InputText("Player Name", ref m_PlayerName, 32);
            UI.InputText("IP Address", ref m_JoinIP, 32);
            UI.InputText("Port", ref m_JoinPort, 10);
            
            UI.Dummy(0.0f, 20.0f);
            if (UI.Button(" Connect! "))
            {
                if (ushort.TryParse(m_JoinPort, out ushort port))
                {
                    Log.Info($"Connecting to {m_JoinIP}:{port}...");
                    Network.ConnectTo(m_JoinIP, port);
                }
            }
            UI.Separator();
            if (UI.Button(" <-- Back ")) m_State = MainMenuState.PlaySelect;
        }

        private void DrawSettings()
        {
            UI.Text("=== SETTINGS ===");
            
            UI.Checkbox(" VSync", ref m_VSync);
            UI.Checkbox(" Fullscreen", ref m_Fullscreen);
            UI.Dummy(0.0f, 10.0f);
            UI.SliderFloat(" Master Volume", ref m_MasterVolume, 0.0f, 100.0f);
            
            UI.Dummy(0.0f, 20.0f);
            UI.Separator();
            if (UI.Button(" Apply "))
            {
                AppWindow.SetVSync(m_VSync);
                AppWindow.SetFullscreen(m_Fullscreen);
                Log.Info("Settings applied.");
            }
            UI.SameLine();
            if (UI.Button(" <-- Back ")) m_State = MainMenuState.Main;
        }

        private void DrawLobby()
        {
            UI.TextColored("=== MULTIPLAYER LOBBY ===", 0.3f, 1.0f, 0.3f, 1.0f);
            UI.Dummy(0.0f, 10.0f);

            if (Network.IsHost)
            {
                UI.Text("You are the Host. Waiting for players...");
                UI.Dummy(0.0f, 20.0f);
                if (UI.Button(" >> START GAME << "))
                {
                    Log.Info("Host starting game...");
                    // Перехід на бойову сцену! Мережа автоматично синхронізує всіх клієнтів.
                    Scene.LoadScene("scenes/rpg_strategy_scene_mp.chscene");
                }
            }
            else
            {
                UI.Text("You are connected as a Client.");
                UI.TextColored("Waiting for the Host to start the game...", 0.8f, 0.8f, 0.8f, 1.0f);
            }

            UI.Dummy(0.0f, 20.0f);
            UI.Separator();
            if (UI.Button(" Disconnect "))
            {
                Network.Disconnect();
                m_State = MainMenuState.Main;
            }
        }
    }
}
