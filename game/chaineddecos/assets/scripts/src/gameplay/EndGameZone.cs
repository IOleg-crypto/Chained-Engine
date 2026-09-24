using System;
using Chained;

namespace ChainedDecos.Scripts
{
    [AutoAttach("EndGameZone")]
    public class EndGameZone : Script
    {
        public string TargetScene = "scenes/start_menu.chscene";
        public string LevelId = "endgamezone";
        public int BonusExperience = 180;
        public int BonusGold = 120;

        public Vector3 ZoneSize = new Vector3(50.0f, 50.0f, 100.0f);
        public bool EnableProximityCheck = true;
        public bool EnableDebugKey = true;
        public Key DebugKey = Key.F4;

        private bool m_Triggered = false;
        private System.Collections.Generic.HashSet<ulong> m_IgnoredIds = new();

        public override void OnCreate()
        {
            m_Triggered = false;
            m_IgnoredIds.Clear();
        }

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
                if (m_IgnoredIds.Add(otherID))
                    Log.Info($"EndGameZone: collision ignored (otherID={otherID}, tag='{otherTag}').");
                return;
            }

            // Only trigger if this is our local player avatar or offline singleplayer
            if (Network.IsConnected)
            {
                var netId = other.GetComponent<NetworkIdentityComponent>();
                if (netId != null && !netId.IsOwner)
                {
                    // Remote player reached finish line
                    return;
                }
            }

            m_Triggered = true;
            Log.Info($"EndGameZone: Local player entered finish zone via collision! (otherID={otherID}, tag='{otherTag}').");
            SpectatorState.TriggerFinish(GameHUD.ElapsedTime);
        }

        public override void OnUpdate(float deltaTime)
        {
            if (m_Triggered)
            {
                return;
            }

            // 1. Proximity volume trigger (works even without rigid body / physics contacts)
            if (EnableProximityCheck)
            {
                CheckProximityTrigger();
                if (m_Triggered) return;
            }

            // 2. Debug fallback to test victory / spectator mode (F4 or F2 or custom DebugKey)
            if (EnableDebugKey && (Input.IsKeyPressed(Key.F4) || Input.IsKeyPressed(Key.F2) || (DebugKey != Key.Null && Input.IsKeyPressed(DebugKey))))
            {
                m_Triggered = true;
                Log.Info("EndGameZone: Debug trigger (F4/F2) -> Local player finished!");
                SpectatorState.TriggerFinish(GameHUD.ElapsedTime);
            }
        }

        private void CheckProximityTrigger()
        {
            TransformComponent? zoneTransform = Entity.GetComponent<TransformComponent>();
            if (zoneTransform == null) return;

            ulong localPlayerId = 0;
            if (Network.IsConnected)
            {
                ulong[] netEntities = Entity.FindAllWithComponent<NetworkIdentityComponent>();
                foreach (ulong id in netEntities)
                {
                    Entity e = new Entity(id);
                    var netId = e.GetComponent<NetworkIdentityComponent>();
                    if (netId != null && netId.IsOwner)
                    {
                        localPlayerId = id;
                        break;
                    }
                }
            }

            if (localPlayerId == 0)
            {
                Entity? playerEnt = Scene.FindEntityByTag("Player");
                if (playerEnt == null) playerEnt = Scene.FindEntityByTag("player");
                if (playerEnt != null) localPlayerId = playerEnt.ID;
            }

            if (localPlayerId == 0) return;

            Entity localPlayer = new Entity(localPlayerId);
            TransformComponent? playerTransform = localPlayer.GetComponent<TransformComponent>();
            if (playerTransform == null) return;

            float scaleX = Math.Abs(zoneTransform.Scale.X) > 0.001f ? Math.Abs(zoneTransform.Scale.X) : 1.0f;
            float scaleY = Math.Abs(zoneTransform.Scale.Y) > 0.001f ? Math.Abs(zoneTransform.Scale.Y) : 1.0f;
            float scaleZ = Math.Abs(zoneTransform.Scale.Z) > 0.001f ? Math.Abs(zoneTransform.Scale.Z) : 1.0f;

            float halfX = Math.Abs(ZoneSize.X) * scaleX * 0.5f;
            float halfY = Math.Abs(ZoneSize.Y) * scaleY * 0.5f;
            float halfZ = Math.Abs(ZoneSize.Z) * scaleZ * 0.5f;

            float dx = Math.Abs(playerTransform.Translation.X - zoneTransform.Translation.X);
            float dy = Math.Abs(playerTransform.Translation.Y - zoneTransform.Translation.Y);
            float dz = Math.Abs(playerTransform.Translation.Z - zoneTransform.Translation.Z);

            if (dx <= halfX && dy <= halfY && dz <= halfZ)
            {
                m_Triggered = true;
                Log.Info("EndGameZone: Local player stepped into finish zone volume!");
                SpectatorState.TriggerFinish(GameHUD.ElapsedTime);
            }
        }
    }
}
