using System;
using Chained;

namespace ChainedDecos.Scripts
{
    [AutoAttach("InGameMenu")]
    public class InGameMenuManager : Script
    {
        private bool m_IsMenuOpen = false;

        public override void OnCreate()
        {
            m_IsMenuOpen = false;
        }

        public override void OnUpdate(float deltaTime)
        {
            // Toggle menu with ESC key
            if (Input.IsKeyPressed(Key.Escape))
            {
                m_IsMenuOpen = !m_IsMenuOpen;
                
                // Update global pause state so player controller stops moving
                GameHUD.IsPaused = m_IsMenuOpen;
            }
        }

        public override void OnGUI()
        {
            if (!m_IsMenuOpen)
            {
                return;
            }

            Vector2 displaySize = UI.GetDisplaySize();
            float winW = 300.0f;
            float winH = 250.0f;
            float winX = (displaySize.X - winW) * 0.5f;
            float winY = (displaySize.Y - winH) * 0.5f;

            UI.BeginWindow("##PauseMenu", winX, winY, winW, winH, 0.95f);

            UI.TextColored("=== PAUSED ===", 1.0f, 0.85f, 0.2f, 1.0f);
            UI.Separator();
            
            UI.Text(""); // Spacing

            if (UI.Button(" Resume "))
            {
                m_IsMenuOpen = false;
                GameHUD.IsPaused = false;
            }

            UI.Text(""); // Spacing

            if (UI.Button(" Quit to Main Menu "))
            {
                m_IsMenuOpen = false;
                GameHUD.IsPaused = false;
                
                if (Network.IsConnected)
                {
                    Network.Disconnect();
                }
                
                Scene.QuitToMenu("scenes/start_menu.chscene", keepSession: false);
            }

            UI.Text(""); // Spacing

            if (UI.Button(" Quit to Desktop "))
            {
                Application.Close();
            }

            UI.EndWindow();
        }
    }
}
