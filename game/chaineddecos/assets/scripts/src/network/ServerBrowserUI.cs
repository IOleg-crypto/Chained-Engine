using System;
using System.Collections.Generic;
using Chained;

namespace ChainedDecos.Scripts
{
    /// <summary>
    /// Interactive card-based Server Browser UI for discovering and joining multiplayer servers.
    /// </summary>
    public class ServerBrowserUI : Script
    {
        public string LobbyScene       = "scenes/lobby.chscene";
        public string MultiplayerMenu = "scenes/multiplayer_menu.chscene";

        private string m_DirectIp      = "127.0.0.1";
        private string m_DirectPort    = "7777";
        private string m_PlayerNick    = "Player";
        private string m_StatusMessage = "";
        private bool   m_StatusIsError = false;

        private bool   m_IsConnecting  = false;
        private float  m_ConnectTimer  = 0f;
        private string m_ConnectingIp  = "";
        private ushort m_ConnectingPort= 0;

        public override void OnCreate()
        {
            Network.Disconnect();
            m_PlayerNick = string.IsNullOrWhiteSpace(PlayerSettings.Nickname) ? "Player" : PlayerSettings.Nickname;
            LanDiscoveryListener.Start();
            m_IsConnecting = false;
            m_ConnectTimer = 0f;
        }

        public override void OnDestroy()
        {
            LanDiscoveryListener.Stop();
        }

        public override void OnUpdate(float deltaTime)
        {
            if (m_IsConnecting)
            {
                m_ConnectTimer += deltaTime;

                if (Network.IsFullyConnected)
                {
                    m_IsConnecting = false;
                    LanDiscoveryListener.Stop();
                    Log.Info("[ServerBrowser] Connected to " + m_ConnectingIp + ":" + m_ConnectingPort + ", loading lobby");
                    Scene.LoadScene(LobbyScene);
                    return;
                }

                if (m_ConnectTimer >= 15f)
                {
                    m_IsConnecting = false;
                    Network.Disconnect();
                    m_StatusMessage = "Connection to " + m_ConnectingIp + ":" + m_ConnectingPort + " timed out";
                    m_StatusIsError = true;
                    Log.Warn("[ServerBrowser] " + m_StatusMessage);
                }
            }
        }

        public override void OnGUI()
        {
            Vector2 display = UI.GetDisplaySize();
            float winW = Math.Min(960.0f, display.X - 40.0f);
            float winH = Math.Min(640.0f, display.Y - 40.0f);
            float winX = (display.X - winW) * 0.5f;
            float winY = (display.Y - winH) * 0.5f;

            UI.PushStyleVar(UIStyleVar.WindowRounding, 12.0f);
            UI.PushStyleVar(UIStyleVar.WindowPadding, 20.0f, 16.0f);
            UI.PushStyleColor(UICol.WindowBg, 0.08f, 0.09f, 0.12f, 0.95f);
            UI.PushStyleColor(UICol.Border, 0.22f, 0.70f, 0.65f, 0.50f);

            UI.BeginWindow("ServerBrowserWindow", winX, winY, winW, winH, 0.95f);

            // ── Header Bar ────────────────────────────────────────────────
            UI.SetWindowFontScale(1.35f);
            UI.TextColored("SERVER BROWSER", 0.35f, 0.90f, 0.82f, 1.0f);
            UI.SetWindowFontScale(1.0f);

            UI.SameLine(winW - 220.0f, -1.0f);
            UI.TextColored("LAN & Radmin Discovery", 0.65f, 0.70f, 0.75f, 1.0f);

            UI.Separator();
            UI.Dummy(0.0f, 6.0f);

            // ── Top Controls: Nickname & Direct Connect ───────────────────
            DrawTopControls(winW);

            UI.Dummy(0.0f, 6.0f);
            UI.Separator();
            UI.Dummy(0.0f, 6.0f);

            // ── Status banner if connecting or error ──────────────────────
            if (m_IsConnecting)
            {
                UI.PushStyleColor(UICol.ChildBg, 0.12f, 0.35f, 0.32f, 0.85f);
                UI.PushStyleVar(UIStyleVar.ChildRounding, 6.0f);
                UI.BeginChild("ConnectingBanner", winW - 40.0f, 32.0f, true);
                UI.TextColored("  Connecting to " + m_ConnectingIp + ":" + m_ConnectingPort + "... (" + (int)(15f - m_ConnectTimer) + "s)", 0.45f, 1.0f, 0.85f, 1.0f);
                UI.SameLine(winW - 140.0f, -1.0f);
                if (UI.Button("Cancel"))
                {
                    m_IsConnecting = false;
                    Network.Disconnect();
                }
                UI.EndChild();
                UI.PopStyleVar(1);
                UI.PopStyleColor(1);
                UI.Dummy(0.0f, 6.0f);
            }
            else if (!string.IsNullOrEmpty(m_StatusMessage))
            {
                float r = m_StatusIsError ? 1.0f : 0.4f;
                float g = m_StatusIsError ? 0.35f : 0.9f;
                float b = m_StatusIsError ? 0.35f : 0.5f;
                UI.TextColored(m_StatusMessage, r, g, b, 1.0f);
                UI.Dummy(0.0f, 4.0f);
            }

            // ── Middle Region: Server Cards List ──────────────────────────
            DrawServerList(winW, winH - 240.0f);

            // ── Footer Bar: Back Button ───────────────────────────────────
            UI.Dummy(0.0f, 8.0f);
            UI.Separator();
            UI.Dummy(0.0f, 6.0f);

            UI.PushStyleColor(UICol.Button, 0.18f, 0.20f, 0.24f, 1.0f);
            UI.PushStyleColor(UICol.ButtonHovered, 0.28f, 0.30f, 0.36f, 1.0f);
            UI.PushStyleVar(UIStyleVar.FrameRounding, 6.0f);

            if (UI.Button("  <- Back to Menu  "))
            {
                LanDiscoveryListener.Stop();
                Scene.LoadScene(MultiplayerMenu);
            }

            UI.SameLine(winW - 280.0f, -1.0f);
            UI.TextColored("Looking for servers on 255.255.255.255:7778", 0.45f, 0.48f, 0.52f, 1.0f);

            UI.PopStyleVar(1);
            UI.PopStyleColor(2);

            UI.EndWindow();
            UI.PopStyleColor(2);
            UI.PopStyleVar(2);
        }

        private void DrawTopControls(float winW)
        {
            // Nickname row
            UI.Text("Nickname:");
            UI.SameLine(85.0f, -1.0f);
            UI.SetNextItemWidth(160.0f);
            UI.InputText("##NickInput", ref m_PlayerNick, 32);
            if (!string.IsNullOrWhiteSpace(m_PlayerNick))
            {
                PlayerSettings.Nickname = m_PlayerNick.Trim();
            }

            // Direct Connect row
            UI.SameLine(280.0f, -1.0f);
            UI.Text("IP:");
            UI.SameLine(305.0f, -1.0f);
            UI.SetNextItemWidth(180.0f);
            UI.InputText("##DirectIP", ref m_DirectIp, 64);

            UI.SameLine(500.0f, -1.0f);
            UI.Text("Port:");
            UI.SameLine(545.0f, -1.0f);
            UI.SetNextItemWidth(70.0f);
            UI.InputText("##DirectPort", ref m_DirectPort, 8);

            UI.SameLine(630.0f, -1.0f);
            UI.PushStyleColor(UICol.Button, 0.22f, 0.70f, 0.65f, 1.0f);
            UI.PushStyleColor(UICol.ButtonHovered, 0.32f, 0.80f, 0.75f, 1.0f);
            UI.PushStyleColor(UICol.ButtonActive, 0.15f, 0.55f, 0.50f, 1.0f);
            UI.PushStyleColor(UICol.Text, 0.05f, 0.10f, 0.10f, 1.0f);
            UI.PushStyleVar(UIStyleVar.FrameRounding, 6.0f);

            if (UI.Button("  Direct Connect  "))
            {
                InitiateDirectConnect();
            }

            UI.SameLine(winW - 130.0f, -1.0f);
            UI.PushStyleColor(UICol.Button, 0.20f, 0.22f, 0.28f, 1.0f);
            UI.PushStyleColor(UICol.ButtonHovered, 0.30f, 0.33f, 0.40f, 1.0f);
            UI.PushStyleColor(UICol.Text, 0.90f, 0.92f, 0.95f, 1.0f);

            if (UI.Button("  Refresh  "))
            {
                LanDiscoveryListener.Refresh();
                m_StatusMessage = "Scanning for active servers...";
                m_StatusIsError = false;
            }

            UI.PopStyleColor(7);
            UI.PopStyleVar(1);
        }

        private void DrawServerList(float winW, float listH)
        {
            var servers = LanDiscoveryListener.GetActiveServers(4.0f);

            UI.PushStyleColor(UICol.ChildBg, 0.05f, 0.06f, 0.08f, 0.85f);
            UI.PushStyleVar(UIStyleVar.ChildRounding, 8.0f);
            UI.PushStyleVar(UIStyleVar.WindowPadding, 12.0f, 12.0f);

            UI.BeginChild("ServerListRegion", winW - 40.0f, Math.Max(160.0f, listH), true);

            if (servers.Count == 0)
            {
                UI.Dummy(0.0f, 40.0f);
                UI.TextColored("          No active servers found on your LAN or Radmin VPN network.", 0.60f, 0.65f, 0.70f, 1.0f);
                UI.Dummy(0.0f, 6.0f);
                UI.TextColored("          Make sure the host has started the server, or use Direct Connect with their IP.", 0.45f, 0.48f, 0.52f, 1.0f);
            }
            else
            {
                for (int i = 0; i < servers.Count; ++i)
                {
                    DrawServerCard(servers[i], i, winW - 68.0f);
                    UI.Dummy(0.0f, 6.0f);
                }
            }

            UI.EndChild();
            UI.PopStyleVar(2);
            UI.PopStyleColor(1);
        }

        private void DrawServerCard(LanServerInfo server, int index, float cardW)
        {
            UI.PushStyleColor(UICol.ChildBg, 0.12f, 0.14f, 0.18f, 0.95f);
            UI.PushStyleColor(UICol.Border, 0.25f, 0.28f, 0.35f, 0.80f);
            UI.PushStyleVar(UIStyleVar.ChildRounding, 8.0f);
            UI.PushStyleVar(UIStyleVar.WindowPadding, 14.0f, 10.0f);

            UI.BeginChild("Card_" + index, cardW, 68.0f, true);

            // Server title & Map badge
            UI.SetWindowFontScale(1.15f);
            UI.TextColored(server.Name, 0.95f, 0.96f, 0.98f, 1.0f);
            UI.SetWindowFontScale(1.0f);

            UI.SameLine(280.0f, -1.0f);
            UI.TextColored("Map: " + server.Map, 0.45f, 0.75f, 0.85f, 1.0f);

            // Players badge
            UI.SameLine(460.0f, -1.0f);
            UI.TextColored("Players: " + server.PlayerCount + " / " + server.MaxPlayers, 0.85f, 0.85f, 0.60f, 1.0f);

            // Ping badge
            UI.SameLine(610.0f, -1.0f);
            float pr = server.PingMs < 60 ? 0.3f : (server.PingMs < 120 ? 0.9f : 1.0f);
            float pg = server.PingMs < 60 ? 0.9f : (server.PingMs < 120 ? 0.8f : 0.3f);
            float pb = 0.3f;
            UI.TextColored(server.PingMs + " ms", pr, pg, pb, 1.0f);

            // Row 2: IP:Port and Join Button
            UI.TextColored(server.IpAddress + ":" + server.Port, 0.50f, 0.55f, 0.60f, 1.0f);

            UI.SameLine(cardW - 120.0f, -1.0f);
            UI.PushStyleColor(UICol.Button, 0.22f, 0.70f, 0.65f, 1.0f);
            UI.PushStyleColor(UICol.ButtonHovered, 0.32f, 0.85f, 0.78f, 1.0f);
            UI.PushStyleColor(UICol.ButtonActive, 0.15f, 0.55f, 0.50f, 1.0f);
            UI.PushStyleColor(UICol.Text, 0.05f, 0.10f, 0.10f, 1.0f);
            UI.PushStyleVar(UIStyleVar.FrameRounding, 5.0f);

            if (UI.Button("  JOIN  ##" + index))
            {
                ConnectToServer(server.IpAddress, server.Port);
            }

            UI.PopStyleVar(1);
            UI.PopStyleColor(4);

            UI.EndChild();
            UI.PopStyleVar(2);
            UI.PopStyleColor(2);
        }

        private void InitiateDirectConnect()
        {
            string ip = m_DirectIp.Trim();
            ushort port = 7777;

            if (ip.Contains(":"))
            {
                int colon = ip.LastIndexOf(':');
                string portStr = ip.Substring(colon + 1).Trim();
                ip = ip.Substring(0, colon).Trim();
                if (ushort.TryParse(portStr, out ushort parsed) && parsed > 0)
                {
                    port = parsed;
                }
            }
            else
            {
                if (ushort.TryParse(m_DirectPort.Trim(), out ushort parsed) && parsed > 0)
                {
                    port = parsed;
                }
            }

            ConnectToServer(ip, port);
        }

        private void ConnectToServer(string ip, ushort port)
        {
            if (m_IsConnecting) return;

            string nick = string.IsNullOrWhiteSpace(m_PlayerNick) ? "Player" : m_PlayerNick.Trim();
            PlayerSettings.Nickname = nick;
            Network.SetLocalPlayerInfo(nick, (byte)LobbyManager.SelectedSkinIndex);

            m_ConnectingIp   = ip;
            m_ConnectingPort = port;
            m_IsConnecting   = true;
            m_ConnectTimer   = 0f;
            m_StatusMessage  = "Connecting to " + ip + ":" + port + "...";
            m_StatusIsError  = false;

            LobbyManager.SelectedPort = port;
            Log.Info("[ServerBrowser] Connecting to " + ip + ":" + port);
            Network.ConnectTo(ip, port);
        }
    }
}
