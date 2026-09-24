using System;
using Chained;

namespace ChainedDecos.Scripts
{
    [AutoAttach("Player")]
    public class GameHUD : Script
    {
        public static bool IsPaused { get; set; } = false;
        public static float ElapsedTime { get; private set; } = 0.0f;
        private float m_Timer = 0.0f;

        public override void OnCreate()
        {
            Priority = 80;
            m_Timer = 0.0f;
            ElapsedTime = 0.0f;
        }

        public override void OnUpdate(float deltaTime)
        {
            var netComp = Entity.GetComponent<NetworkIdentityComponent>();
            if (netComp != null && !netComp.IsOwner) return;

            if (!SpectatorState.IsFinished)
            {
                m_Timer += deltaTime;
                ElapsedTime = m_Timer;
            }
        }

        public override void OnGUI()
        {
            var netComp = Entity.GetComponent<NetworkIdentityComponent>();
            if (netComp != null && !netComp.IsOwner) return;

            if (SpectatorState.IsFinished)
            {
                DrawVictoryHUD();
                return;
            }

            TransformComponent? transform = Entity.GetComponent<TransformComponent>();
            float altitude = transform != null ? transform.Translation.Y : 0.0f;

            int hours   = (int)(m_Timer / 3600.0f);
            int minutes = (int)((m_Timer - hours * 3600.0f) / 60.0f);
            int seconds = (int)(m_Timer) % 60;

            UI.Text($"Altitude: {altitude:F2}");
            UI.Text($"Time: {hours:D2}:{minutes:D2}:{seconds:D2}");
        }

        private void DrawVictoryHUD()
        {
            Vector2 displaySize = UI.GetDisplaySize();
            float winW = 520.0f;
            float winH = 160.0f;
            float winX = (displaySize.X - winW) * 0.5f;
            float winY = 24.0f;

            UI.BeginWindow("##VictoryHUD", winX, winY, winW, winH, 0.95f);

            UI.TextColored("=== VICTORY! COURSE COMPLETED! ===", 1.0f, 0.85f, 0.2f, 1.0f);
            UI.Separator();

            UI.TextColored($"Finish Time: {SpectatorState.FormatTime(SpectatorState.FinishTime)}", 0.3f, 1.0f, 0.3f, 1.0f);
            UI.TextColored("Free Fly: WASD / Space (up) / Ctrl (down) / Shift (fast) / Mouse look", 0.6f, 0.85f, 1.0f, 1.0f);

            UI.Separator();

            if (UI.Button("  [ Return to Main Menu ]  "))
            {
                SpectatorState.Reset();
                if (Network.IsConnected)
                    Network.Disconnect();
                Scene.LoadScene("scenes/start_menu.chscene");
            }

            UI.EndWindow();
        }
    }
}
