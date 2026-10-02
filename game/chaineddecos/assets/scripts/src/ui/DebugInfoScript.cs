using System;
using System.Collections.Generic;
using Chained;

namespace ChainedDecos.Scripts
{
    public class DebugInfoScript : Script
    {
        public Key ToggleKey = Key.F3;
        public Key ToggleKey2 = Key.F4;
        private bool m_ShowDebug = false;

        public override void OnUpdate(float ts)
        {
            var netComp = Entity.GetComponent<NetworkIdentityComponent>();
            if (netComp != null && !netComp.IsOwner) return;

            if (Input.IsKeyPressed(ToggleKey))
            {
                m_ShowDebug = !m_ShowDebug;
            }
        }

        public override void OnGUI()
        {
            var netComp = Entity.GetComponent<NetworkIdentityComponent>();
            if (netComp != null && !netComp.IsOwner) return;

            if (m_ShowDebug)
            {
                UI.Text($"FPS: {Time.FPS}");
                UI.Text($"DeltaTime: {Time.DeltaTime:F4}s");
                if (Network.IsClient)
                {
                    uint ping = Network.GetPing();
                    UI.Text($"Ping: {ping}ms");
                }
            }
        }
    }
}
