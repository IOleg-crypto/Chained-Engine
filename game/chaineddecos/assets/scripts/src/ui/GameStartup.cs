using Chained;

namespace ChainedDecos.Scripts
{
    // Applied to any entity in the start scene. Loads and applies settings.cfg
    // on game startup so that resolution, fullscreen, VSync, AA, and audio
    // volumes are correct before the first frame renders.
    public class GameStartup : Script
    {
        private static bool s_Applied;

        public override void OnStart()
        {
            if (s_Applied) return;
            s_Applied = true;
            SettingsConfig.ApplyAll();
        }
    }
}
