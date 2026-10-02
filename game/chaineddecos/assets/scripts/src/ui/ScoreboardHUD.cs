using System.Collections.Generic;
using System.Text.Json;
using Chained;

namespace ChainedDecos.Scripts
{
    public class ScoreboardHUD : Script
    {
        private class PlayerEntry
        {
            public ulong id { get; set; }
            public string name { get; set; } = "";
            public int skin { get; set; }
            public int isHost { get; set; }
            public int ping { get; set; }
        }

        private bool m_ShowScoreboard = false;

        public override void OnCreate()
        {
            Priority = 85;
            Log.Info("[ScoreboardHUD] Created");
        }

        public override void OnUpdate(float deltaTime)
        {
            var netComp = Entity.GetComponent<NetworkIdentityComponent>();
            if (netComp != null && !netComp.IsOwner) return;

            m_ShowScoreboard = Input.IsKeyDown(Key.Tab);
        }

        public override void OnGUI()
        {
            var netComp = Entity.GetComponent<NetworkIdentityComponent>();
            if (netComp != null && !netComp.IsOwner) return;

            if (!m_ShowScoreboard && !Input.IsKeyDown(Key.Tab))
            {
                return;
            }

            DrawScoreboard();
        }

        private void DrawScoreboard()
        {
            Vector2 screenSize = UI.GetDisplaySize();
            float windowWidth = 360f;
            float windowHeight = 220f;

            float startX = (screenSize.X - windowWidth) * 0.5f;
            float startY = (screenSize.Y - windowHeight) * 0.5f;

            UI.BeginWindow("##ScoreboardWindow", startX, startY, windowWidth, windowHeight, 0.85f);
            
            UI.Text("=== PLAYERS ===");
            UI.Separator();

            if (!Network.IsConnected)
            {
                UI.TextColored("[HOST]  ", 0.7f, 0.7f, 0.7f, 1.0f);
                UI.SameLine(90f);
                UI.TextColored("Local Player (You)", 1.0f, 1.0f, 1.0f, 1.0f);
                UI.SameLine(280f);
                UI.TextColored("0 ms", 0.2f, 1.0f, 0.2f, 1.0f);

                UI.EndWindow();
                return;
            }

            string json = Network.GetPlayerListJSON();
            if (string.IsNullOrEmpty(json))
            {
                UI.Text("Loading player list...");
                UI.EndWindow();
                return;
            }

            try
            {
                var players = JsonSerializer.Deserialize<List<PlayerEntry>>(json);
                if (players != null && players.Count > 0)
                {
                    foreach (var p in players)
                    {
                        string displayName = string.IsNullOrEmpty(p.name) ? "Player" : p.name;
                        if (p.id == Network.GetLocalNetworkID())
                            displayName += " (You)";

                        string role = p.isHost != 0 ? "[HOST]  " : "[CLIENT]";
                        
                        UI.TextColored(role, 0.7f, 0.7f, 0.7f, 1.0f);
                        UI.SameLine(90f);
                        UI.TextColored(displayName, 1.0f, 1.0f, 1.0f, 1.0f);
                        UI.SameLine(280f);

                        float r = p.ping < 50 ? 0.2f : p.ping < 150 ? 1.0f : 1.0f;
                        float g = p.ping < 50 ? 1.0f : p.ping < 150 ? 1.0f : 0.2f;
                        float b = 0.2f;

                        string pingStr = p.isHost != 0 ? "0 ms" : $"{p.ping} ms";
                        UI.TextColored(pingStr, r, g, b, 1.0f);
                    }
                }
            }
            catch (JsonException)
            {
                UI.Text("Parsing players...");
            }

            UI.EndWindow();
        }
    }
}
