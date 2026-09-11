using System;
using System.Collections.Generic;
using System.Numerics;
using Chained;

namespace ChainedDecos.Scripts
{
    public class InGameChat : Script
    {
        public class ChatEntry
        {
            public string Sender  = "";
            public string Message = "";
            public float  TimeLeft = 8.0f;
        }

        public static bool IsChatOpen { get; private set; } = false;
        public bool  IsOpen         { get; private set; } = false;
        public int   MaxHistory     = 30;
        public float MessageDuration = 8.0f;

        // Chat window constants (relative to viewport)
        private const float WinW       = 500.0f;
        private const float WinH       = 240.0f;
        private const float OverlayW   = 480.0f;
        private const float OverlayH   = 160.0f;
        private const float PadX       = 16.0f;
        private const float PadY       = 16.0f;

        private List<ChatEntry> m_Messages   = new List<ChatEntry>();
        private string          m_InputText  = "";
        private bool            m_FocusInput = false;
        private bool            m_JustOpened = false;

        public override void OnCreate()
        {
            Priority = 90;
            IsOpen = false;
            IsChatOpen = false;
            m_JustOpened = false;
        }

        public override void OnUpdate(float deltaTime)
        {
            var netComp = Entity.GetComponent<NetworkIdentityComponent>();
            if (netComp != null && !netComp.IsOwner) return;

            // Fetch pending network chat messages
            if (Network.IsConnected && Network.HasPendingChat)
            {
                string json = Network.GetPendingChatJSON();
                Network.ClearPendingChat();
                ParseAndAppendChat(json);
            }

            // Decrement message display timers
            for (int i = m_Messages.Count - 1; i >= 0; i--)
            {
                m_Messages[i].TimeLeft -= deltaTime;
            }

            // Toggle chat open/close
            if (!IsOpen)
            {
                if (Input.IsKeyPressed(Key.T) || Input.IsKeyPressed(Key.Enter))
                {
                    IsOpen       = true;
                    IsChatOpen   = true;
                    m_FocusInput = true;
                    m_JustOpened = true;
                    m_InputText  = "";
                    ConsumeEvent();
                }
            }
            else
            {
                if (Input.IsKeyPressed(Key.Escape) || Input.IsKeyPressed(Key.Delete))
                {
                    IsOpen       = false;
                    IsChatOpen   = false;
                    m_JustOpened = false;
                    m_InputText  = "";
                }
                ConsumeEvent();
            }
        }

        public override void OnGUI()
        {
            var netComp = Entity.GetComponent<NetworkIdentityComponent>();
            if (netComp != null && !netComp.IsOwner) return;

            // Get actual game viewport size each frame
            Vector2 display = UI.GetDisplaySize();
            float screenW = display.X;
            float screenH = display.Y;

            if (IsOpen)
            {
                // Full chat window — anchored bottom-left inside the game window
                float x = PadX;
                float y = screenH - WinH - PadY;

                UI.BeginWindow("##InGameChatWindow", x, y, WinW, WinH, 0.78f);

                // History
                int start = Math.Max(0, m_Messages.Count - 12);
                for (int i = start; i < m_Messages.Count; i++)
                {
                    var msg = m_Messages[i];
                    UI.TextColored($"[{msg.Sender}]: ", 0.4f, 0.8f, 1.0f, 1.0f);
                    UI.Text(msg.Message);
                }

                UI.SetScrollHereY(1.0f);

                // Input field
                if (m_FocusInput)
                {
                    UI.SetKeyboardFocusHere();
                    m_FocusInput = false;
                }

                bool submitted = UI.InputText("##ChatInput", ref m_InputText, 256);
                if (m_JustOpened)
                {
                    // Ignore submission on the frame it was opened by pressing Enter
                    m_JustOpened = false;
                    submitted = false;
                }

                if (submitted)
                {
                    string toSend = m_InputText.Trim();
                    m_InputText = "";
                    IsOpen      = false;
                    IsChatOpen  = false;

                    if (!string.IsNullOrWhiteSpace(toSend))
                    {
                        Network.SendChatMessage(toSend);
                    }
                }

                UI.EndWindow();
            }
            else
            {
                // Compact overlay — recent active messages, same anchor
                bool anyActive = false;
                int start = Math.Max(0, m_Messages.Count - 6);
                for (int i = start; i < m_Messages.Count; i++)
                {
                    if (m_Messages[i].TimeLeft > 0.0f) { anyActive = true; break; }
                }

                if (anyActive)
                {
                    float x = PadX;
                    float y = screenH - OverlayH - PadY;

                    UI.BeginWindow("##InGameChatOverlay", x, y, OverlayW, OverlayH, 0.0f);

                    for (int i = start; i < m_Messages.Count; i++)
                    {
                        var msg = m_Messages[i];
                        if (msg.TimeLeft > 0.0f)
                        {
                            float alpha = Math.Min(1.0f, msg.TimeLeft);
                            UI.TextColored($"[{msg.Sender}]: {msg.Message}", 1.0f, 1.0f, 1.0f, alpha);
                        }
                    }

                    UI.EndWindow();
                }
            }
        }

        private void ParseAndAppendChat(string json)
        {
            if (string.IsNullOrEmpty(json) || json == "[]") return;

            int pos = 0;
            while (true)
            {
                int sStart = json.IndexOf("\"sender\":\"", pos, StringComparison.Ordinal);
                if (sStart < 0) break;
                sStart += 10;
                int sEnd = json.IndexOf('"', sStart);
                if (sEnd < 0) break;
                string sender = json.Substring(sStart, sEnd - sStart);

                int mStart = json.IndexOf("\"message\":\"", sEnd, StringComparison.Ordinal);
                if (mStart < 0) break;
                mStart += 11;
                int mEnd = json.IndexOf('"', mStart);
                if (mEnd < 0) break;
                string message = json.Substring(mStart, mEnd - mStart);

                m_Messages.Add(new ChatEntry
                {
                    Sender   = sender,
                    Message  = message,
                    TimeLeft = MessageDuration
                });

                if (m_Messages.Count > MaxHistory)
                    m_Messages.RemoveAt(0);

                pos = mEnd + 1;
            }
        }
    }
}
