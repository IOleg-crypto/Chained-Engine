using System;
using Chained;

namespace ChainedDecos.Scripts
{
    // Demonstration of a AAA "Resident Evil" style menu using ImGui (Chained.UI)
    [AutoAttach("ReRequiemMenu")]
    public class ReRequiemMenu : Script
    {
        private int m_ActiveTab = 2; // Default to CAMERA to match screenshot
        private string[] m_Tabs = { "CONTROLS", "GAMEPLAY", "CAMERA", "HUD", "GRAPHICS", "AUDIO" };
        
        // Mock Settings
        private int m_Perspective = 0; // 0 = First-person, 1 = Third-person
        private int m_Inversion = 0;   // 0 = None, 1 = Inverted
        private float m_CameraWobble = 10.0f;
        
        private string m_HoveredDescription = "Hover over a setting to see details.";

        public override void OnUpdate(float deltaTime)
        {
            // Just logic updates if needed
        }

        public override void OnGUI()
        {
            Vector2 display = UI.GetDisplaySize();
            
            // Draw a full-screen window with a dark background
            // Assuming ImGuiCol_WindowBg is standard (usually index 2, but we use alpha in BeginWindow)
            UI.BeginWindow("##ReMenu", 0, 0, display.X, display.Y, 0.95f);
            
            // 1. TOP HEADER (OPTIONS)
            UI.SetWindowFontScale(1.5f);
            UI.TextColored("OPTIONS", 1.0f, 1.0f, 1.0f, 1.0f);
            UI.SetWindowFontScale(1.0f);
            UI.Dummy(0.0f, 10.0f);
            
            // 2. TABS ROW
            for (int i = 0; i < m_Tabs.Length; i++)
            {
                if (i > 0) UI.SameLine(0.0f, 20.0f);
                
                if (m_ActiveTab == i)
                {
                    // Active tab: White text
                    UI.TextColored(m_Tabs[i], 1.0f, 1.0f, 1.0f, 1.0f);
                }
                else
                {
                    // Inactive tab: Gray text + clickable
                    UI.TextColored(m_Tabs[i], 0.5f, 0.5f, 0.5f, 1.0f);
                    if (UI.IsItemHovered() && UI.Button("##tab" + i)) 
                    {
                        m_ActiveTab = i;
                    }
                }
            }
            
            UI.Dummy(0.0f, 5.0f);
            UI.Separator();
            UI.Dummy(0.0f, 20.0f);
            
            // 3. SPLIT LAYOUT: Left (Settings), Right (Description)
            float leftWidth = display.X * 0.5f;
            float rightWidth = display.X * 0.4f;
            
            UI.BeginChild("LeftPane", leftWidth, display.Y * 0.7f, false);
            
            if (m_ActiveTab == 2) // CAMERA
            {
                DrawCameraSettings(leftWidth);
            }
            else
            {
                UI.TextColored("Select CAMERA tab to see the RE-style implementation.", 0.5f, 0.5f, 0.5f, 1.0f);
            }
            
            UI.EndChild();
            
            UI.SameLine(leftWidth + 40.0f, -1.0f);
            
            UI.BeginChild("RightPane", rightWidth, display.Y * 0.7f, false);
            UI.TextColored(m_HoveredDescription, 0.8f, 0.8f, 0.8f, 1.0f);
            UI.Dummy(0.0f, 30.0f);
            UI.TextColored("(Preview Image would go here if UI.Image was exposed)", 0.3f, 0.3f, 0.3f, 1.0f);
            UI.EndChild();
            
            // 4. BOTTOM BAR (Controls)
            UI.Separator();
            UI.Dummy(0.0f, 10.0f);
            UI.TextColored(" [ Reset All ]    [ Reset Category ]    [ Confirm ]    [ Back ]", 0.7f, 0.7f, 0.7f, 1.0f);
            
            UI.EndWindow();
        }

        private void DrawCameraSettings(float width)
        {
            UI.TextColored("Perspective and Camera", 0.6f, 0.6f, 0.6f, 1.0f);
            UI.Separator();
            
            DrawSelectionRow("Grace's Camera", ref m_Perspective, new string[] { "First-person", "Third-person" }, width, "Change Grace's camera perspective.\nA dedicated screen will open when selected.\n\n(Default: First-person)");
            DrawSelectionRow("Leon's Camera", ref m_Perspective, new string[] { "First-person", "Third-person" }, width, "Change Leon's camera perspective.");
            
            UI.Dummy(0.0f, 20.0f);
            UI.TextColored("Camera Inversion", 0.6f, 0.6f, 0.6f, 1.0f);
            UI.Separator();
            DrawSelectionRow("Normal Gameplay", ref m_Inversion, new string[] { "None", "Invert X", "Invert Y", "Both" }, width, "Invert camera controls during normal gameplay.");
            DrawSelectionRow("When Aiming", ref m_Inversion, new string[] { "None", "Invert X", "Invert Y", "Both" }, width, "Invert camera controls when aiming a weapon.");
            
            UI.Dummy(0.0f, 20.0f);
            UI.TextColored("Camerawork", 0.6f, 0.6f, 0.6f, 1.0f);
            UI.Separator();
            
            // Custom slider row
            UI.Text("Camera Wobble");
            if (UI.IsItemHovered()) m_HoveredDescription = "Adjust the intensity of the camera wobble effect.\n\n(Default: 10)";
            UI.SameLine(width - 250.0f, -1.0f);
            UI.SetNextItemWidth(200.0f);
            UI.SliderFloat("##Wobble", ref m_CameraWobble, 0.0f, 100.0f);
        }

        private void DrawSelectionRow(string label, ref int value, string[] options, float width, string description)
        {
            UI.Text(label);
            if (UI.IsItemHovered()) m_HoveredDescription = description;
            
            UI.SameLine(width - 250.0f, -1.0f);
            
            if (UI.Button(" < ##" + label)) 
            {
                value--;
                if (value < 0) value = options.Length - 1;
            }
            
            UI.SameLine(0.0f, 20.0f);
            string currentText = options[Math.Clamp(value, 0, options.Length - 1)];
            UI.Text(currentText);
            
            UI.SameLine(width - 50.0f, -1.0f);
            if (UI.Button(" > ##" + label))
            {
                value++;
                if (value >= options.Length) value = 0;
            }
        }
    }
}
