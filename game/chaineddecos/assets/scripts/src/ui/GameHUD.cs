using System;
using System.Numerics;
using Chained;

namespace ChainedDecos.Scripts
{
    public class GameHUD : Script
    {
        public static bool IsPaused { get; set; } = false;
        public string MenuScene = "scenes/start_menu.chscene";

        private float m_Timer = 0.0f;

        public override void OnCreate()
        {
            Priority = 80;
            IsPaused = false;
        }

        public override void OnUpdate(float deltaTime)
        {
            var netComp = Entity.GetComponent<NetworkIdentityComponent>();
            if (netComp != null && !netComp.IsOwner) return;

            // Toggle pause on Escape
            if (Input.IsKeyPressed(Key.Escape))
            {
                IsPaused = !IsPaused;
                ConsumeEvent();
            }

            if (!IsPaused)
            {
                m_Timer += deltaTime;

                if (Input.IsKeyPressed(Key.R))
                {
                    m_Timer = 0.0f;
                    Log.Info("timer reset via R");
                }
            }
        }

        public override void OnGUI()
        {
            var netComp = Entity.GetComponent<NetworkIdentityComponent>();
            if (netComp != null && !netComp.IsOwner) return;

            TransformComponent? transform = Entity.GetComponent<TransformComponent>();
            float altitude = transform != null ? transform.Translation.Y : 0.0f;

            int hours = (int)(m_Timer / 3600.0f);
            int minutes = (int)((m_Timer - hours * 3600.0f) / 60.0f);
            int seconds = (int)(m_Timer) % 60;

            UI.Text($"Altitude: {altitude:F2}");
            UI.Text($"Time: {hours:D2}:{minutes:D2}:{seconds:D2}");

            // Pause Overlay Menu
            if (IsPaused)
            {
                Vector2 display = UI.GetDisplaySize();
                float winW = 320.0f;
                float winH = 220.0f;
                float x = (display.X - winW) * 0.5f;
                float y = (display.Y - winH) * 0.5f;

                UI.BeginWindow("##PauseMenu", x, y, winW, winH, 0.90f);

                UI.TextColored("        === PAUSE ===", 1.0f, 0.85f, 0.2f, 1.0f);
                UI.Text("");

                if (UI.Button("Resume Game"))
                {
                    IsPaused = false;
                }

                UI.Text("");

                if (UI.Button("Restart Level"))
                {
                    IsPaused = false;
                    string currentScene = Scene.GetCurrentScenePath();
                    if (!string.IsNullOrEmpty(currentScene))
                    {
                        Scene.LoadScene(currentScene);
                    }
                }

                UI.Text("");

                string exitLabel = Network.IsConnected ? "Leave Match" : "Exit to Menu";
                if (UI.Button(exitLabel))
                {
                    IsPaused = false;
                    if (!Network.IsConnected)
                    {
                        string currentScene = Scene.GetCurrentScenePath();
                        if (!string.IsNullOrEmpty(currentScene))
                        {
                            SessionState.LastGameplayScene = currentScene;
                        }
                        if (transform != null)
                        {
                            SessionState.SavedPlayerPosition = transform.Translation;
                            SessionState.SavedPlayerRotation = transform.Rotation;
                            SessionState.SaveToDisk();
                        }
                    }
                    else
                    {
                        Network.Disconnect();
                    }
                    Scene.LoadScene(MenuScene);
                }

                UI.EndWindow();
            }
        }
    }
}
