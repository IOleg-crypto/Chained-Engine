using Chained;

namespace ChainedDecos.Scripts
{
    // Applied to any entity in the start scene. Loads and applies settings.cfg
    // on game startup so that resolution, fullscreen, VSync, AA, and audio
    // volumes are correct before the first frame renders.
    // Also resets static game state (SpectatorState) so returning to menu
    // from any path (victory, disconnect, crash-return) always starts fresh.
    public class GameStartup : Script
    {
        private static bool s_Applied;

        public override void OnStart()
        {
            // Always reset gameplay state when the menu loads
            SpectatorState.Reset();

            if (s_Applied) return;
            s_Applied = true;
            SettingsConfig.ApplyAll();
        }
    }
}
