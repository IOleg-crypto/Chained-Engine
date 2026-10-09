using Chained;

namespace ChainedDecos.Scripts
{
    // Applied to any entity in the start scene.
    // Resets all static gameplay state every time the menu loads.
    public class GameStartup : Script
    {
        private static bool s_Applied;

        public override void OnStart()
        {
            // Always reset gameplay state on every menu visit
            SpectatorState.Reset();
            GameHUD.IsPaused = false;

            // Only disconnect if there is NO active session to resume.
            // If HasActiveSession() is true, the player paused to menu —
            // we must keep the network alive so Resume can work.
            if (Network.IsConnected && !Scene.HasActiveSession())
            {
                Log.Info("[GameStartup] No active session — cleaning up stale network connection.");
                Network.Disconnect();
            }

            // Apply settings only once per process lifetime
            if (s_Applied) return;
            s_Applied = true;
            SettingsConfig.ApplyAll();
        }
    }
}
