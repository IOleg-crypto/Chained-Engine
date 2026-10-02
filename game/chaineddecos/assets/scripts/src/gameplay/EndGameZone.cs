using System;
using Chained;

namespace ChainedDecos.Scripts
{
    public class EndGameZone : Script
    {
        public string TargetScene = "scenes/start_menu.chscene";
        public string LevelId = "endgamezone";
        public int BonusExperience = 180;
        public int BonusGold = 120;

        private bool m_Triggered = false;
        // Track IDs we've already logged as ignored, to prevent per-frame log spam
        // (Physics engines often re-fire OnCollisionEnter each tick for persistent contacts)
        private System.Collections.Generic.HashSet<ulong> m_IgnoredIds = new();

        public override void OnCollisionEnter(ulong otherID)
        {
            if (m_Triggered)
            {
                return;
            }

            Entity other = new Entity(otherID);

            TagComponent? tagComp = other.GetComponent<TagComponent>();
            string otherTag = (tagComp?.Tag ?? string.Empty).Trim();

            bool hasPlayerComponent = other.HasComponent<PlayerComponent>();
            bool playerByTag = otherTag.Equals("Player", StringComparison.OrdinalIgnoreCase) ||
                               otherTag.Contains("player", StringComparison.OrdinalIgnoreCase);

            if (!hasPlayerComponent && !playerByTag)
            {
                if (m_IgnoredIds.Add(otherID)) // Add returns false if already present
                    Log.Info($"EndGameZone: collision ignored (otherID={otherID}, tag='{otherTag}').");
                return;
            }

            m_Triggered = true;

            Log.Info($"EndGameZone: player detected (otherID={otherID}, tag='{otherTag}'). Loading {TargetScene}");
            Scene.LoadScene(TargetScene);
        }

        public override void OnUpdate(float deltaTime)
        {
            if (m_Triggered)
            {
                return;
            }

            // Debug fallback in case collision callbacks are not fired in a scene setup.
            if (Input.IsKeyPressed(Key.F9))
            {
                m_Triggered = true;
                Log.Info($"EndGameZone: F9 debug trigger -> Loading {TargetScene}");
                Scene.LoadScene(TargetScene);
            }
        }
    }
}
