using System;
using System.Collections.Generic;
using Chained;

namespace ChainedDecos.Scripts
{
    public class SettingsScript : Script
    {
        private bool m_WasApplied = false;

        public override void OnStart()
        {
            PopulateResolutions();
            PopulateAntiAliasing();
            PopulateShadows();
            InitFullscreen();
            InitVSync();
            InitShadows();
            InitAudioSliders();
        }

        // ── UI Init ────────────────────────────────────────────────────────

        private void PopulateResolutions()
        {
            Entity? resEnt = Scene.FindEntityByTag("resolution");
            ComboBoxControl? combo = resEnt?.GetComponent<ComboBoxControl>();
            if (combo == null) return;

            combo.ClearItems();
            string resolutions = AppWindow.GetSupportedResolutions();
            if (string.IsNullOrEmpty(resolutions))
            {
                combo.AddItem("1920x1080");
                combo.AddItem("1600x900");
                combo.AddItem("1366x768");
                combo.AddItem("1280x720");
            }
            else
            {
                foreach (string res in resolutions.Split(';'))
                {
                    if (!string.IsNullOrEmpty(res))
                        combo.AddItem(res);
                }
            }

            string currentRes = $"{AppWindow.GetWidth()}x{AppWindow.GetHeight()}";
            for (int i = 0; i < combo.ItemCount; i++)
            {
                if (combo.GetItem(i) == currentRes)
                {
                    combo.SelectedIndex = i;
                    return;
                }
            }
        }

        private void PopulateAntiAliasing()
        {
            Entity? aaEnt = Scene.FindEntityByTag("anti_aliasing_combobox");
            ComboBoxControl? combo = aaEnt?.GetComponent<ComboBoxControl>();
            if (combo == null) return;

            combo.ClearItems();
            combo.AddItem("None");
            combo.AddItem("2x MSAA");
            combo.AddItem("4x MSAA");
            combo.AddItem("8x MSAA");
            combo.AddItem("16x MSAA");

            int current = AppWindow.GetAntiAliasingSamples();
            int[] aaValues = { 0, 2, 4, 8, 16 };
            for (int i = 0; i < aaValues.Length; i++)
            {
                if (aaValues[i] == current)
                {
                    combo.SelectedIndex = i;
                    return;
                }
            }
            combo.SelectedIndex = 1; // Default 2x MSAA
        }

        private void PopulateShadows()
        {
            Entity? srEnt = Scene.FindEntityByTag("shadow_resolution_combobox");
            ComboBoxControl? combo = srEnt?.GetComponent<ComboBoxControl>();
            if (combo == null) return;

            combo.ClearItems();
            combo.AddItem("512 (Low)");
            combo.AddItem("1024 (Medium)");
            combo.AddItem("2048 (High)");
            combo.AddItem("4096 (Ultra)");

            int current = AppWindow.GetShadowResolution();
            int[] srValues = { 512, 1024, 2048, 4096 };
            for (int i = 0; i < srValues.Length; i++)
            {
                if (srValues[i] == current)
                {
                    combo.SelectedIndex = i;
                    return;
                }
            }
            combo.SelectedIndex = 2; // Default 2048 (High)
        }

        private void InitFullscreen()
        {
            Entity? fullEnt = Scene.FindEntityByTag("option_fullscreen");
            CheckboxControl? check = fullEnt?.GetComponent<CheckboxControl>();
            if (check != null)
                check.IsChecked = AppWindow.GetFullscreen();
        }

        private void InitVSync()
        {
            Entity? vsyncEnt = Scene.FindEntityByTag("option_vsync");
            CheckboxControl? check = vsyncEnt?.GetComponent<CheckboxControl>();
            if (check != null)
                check.IsChecked = AppWindow.GetVSync();
        }

        private void InitShadows()
        {
            Entity? shEnt = Scene.FindEntityByTag("option_shadows");
            CheckboxControl? check = shEnt?.GetComponent<CheckboxControl>();
            if (check != null)
                check.IsChecked = AppWindow.GetEnableShadows();
        }

        private void InitAudioSliders()
        {
            SetSliderValue("volume_master", Audio.GetMasterVolume());
            SetSliderValue("volume_music", Audio.GetMusicVolume());
            SetSliderValue("volume_sfx", Audio.GetSFXVolume());
        }

        // ── Helpers ────────────────────────────────────────────────────────

        private bool GetCheckboxState(string tag)
        {
            Entity? ent = Scene.FindEntityByTag(tag);
            CheckboxControl? check = ent?.GetComponent<CheckboxControl>();
            return check != null && check.IsChecked;
        }

        private float GetSliderValue(string tag)
        {
            Entity? ent = Scene.FindEntityByTag(tag);
            SliderControl? slider = ent?.GetComponent<SliderControl>();
            return slider != null ? slider.Value : 1.0f;
        }

        private void SetSliderValue(string tag, float normalized)
        {
            Entity? ent = Scene.FindEntityByTag(tag);
            SliderControl? slider = ent?.GetComponent<SliderControl>();
            if (slider != null)
                slider.Value = normalized * 100f;
        }

        // ── Apply ──────────────────────────────────────────────────────────

        public override void OnUpdate(float deltaTime)
        {
            ButtonControl? btn = Entity.GetComponent<ButtonControl>();
            if (btn == null) return;

            bool isPressed = btn.IsClicked;

            if (isPressed && !m_WasApplied)
            {
                m_WasApplied = true;
                ApplySettings();
                Log.Info("Settings applied");
            }
            else if (!btn.IsDown)
            {
                m_WasApplied = false;
            }
        }

        private void ApplySettings()
        {
            // 1. Resolution (ComboBox)
            Entity? resEnt = Scene.FindEntityByTag("resolution");
            if (resEnt != null)
            {
                ComboBoxControl? combo = resEnt.GetComponent<ComboBoxControl>();
                if (combo != null && combo.ItemCount > 0)
                {
                    string? resStr = combo.GetItem(combo.SelectedIndex);
                    if (!string.IsNullOrEmpty(resStr))
                    {
                        string[] parts = resStr.Split('x');
                        if (parts.Length == 2 && int.TryParse(parts[0], out int w) && int.TryParse(parts[1], out int h))
                            AppWindow.SetSize(w, h);
                    }
                }
            }

            // 2. Fullscreen (Checkbox)
            AppWindow.SetFullscreen(GetCheckboxState("option_fullscreen"));

            // 3. VSync (Checkbox)
            AppWindow.SetVSync(GetCheckboxState("option_vsync"));

            // 4. Shadows (Checkbox)
            AppWindow.SetEnableShadows(GetCheckboxState("option_shadows"));

            // 5. Anti-aliasing / MSAA (ComboBox)
            Entity? aaEnt = Scene.FindEntityByTag("anti_aliasing_combobox");
            if (aaEnt != null)
            {
                ComboBoxControl? combo = aaEnt.GetComponent<ComboBoxControl>();
                if (combo != null && combo.ItemCount > 0)
                {
                    int[] aaValues = { 0, 2, 4, 8, 16 };
                    int idx = combo.SelectedIndex;
                    if (idx >= 0 && idx < aaValues.Length)
                        AppWindow.SetAntiAliasingSamples(aaValues[idx]);
                }
            }

            // 6. Shadow Resolution (ComboBox)
            Entity? srEnt = Scene.FindEntityByTag("shadow_resolution_combobox");
            if (srEnt != null)
            {
                ComboBoxControl? combo = srEnt.GetComponent<ComboBoxControl>();
                if (combo != null && combo.ItemCount > 0)
                {
                    int[] srValues = { 512, 1024, 2048, 4096 };
                    int idx = combo.SelectedIndex;
                    if (idx >= 0 && idx < srValues.Length)
                        AppWindow.SetShadowResolution(srValues[idx]);
                }
            }

            // 7. Audio volumes (Sliders: 0–100 → 0.0–1.0)
            Audio.SetMasterVolume(GetSliderValue("volume_master") / 100f);
            Audio.SetMusicVolume(GetSliderValue("volume_music") / 100f);
            Audio.SetSFXVolume(GetSliderValue("volume_sfx") / 100f);

            // 8. Save to config file
            SaveSettings();
        }

        private void SaveSettings()
        {
            var cfg = new Dictionary<string, string>();

            Entity? resEnt = Scene.FindEntityByTag("resolution");
            ComboBoxControl? resCombo = resEnt?.GetComponent<ComboBoxControl>();
            if (resCombo != null && resCombo.ItemCount > 0)
            {
                string? resStr = resCombo.GetItem(resCombo.SelectedIndex);
                if (!string.IsNullOrEmpty(resStr))
                    cfg["Resolution"] = resStr;
            }

            cfg["Fullscreen"] = GetCheckboxState("option_fullscreen").ToString();
            cfg["VSync"] = GetCheckboxState("option_vsync").ToString();
            cfg["EnableShadows"] = GetCheckboxState("option_shadows").ToString();

            Entity? aaEnt = Scene.FindEntityByTag("anti_aliasing_combobox");
            ComboBoxControl? aaCombo = aaEnt?.GetComponent<ComboBoxControl>();
            if (aaCombo != null)
            {
                int[] aaValues = { 0, 2, 4, 8, 16 };
                int idx = aaCombo.SelectedIndex;
                if (idx >= 0 && idx < aaValues.Length)
                    cfg["AntiAliasingSamples"] = aaValues[idx].ToString();
            }

            Entity? srEnt = Scene.FindEntityByTag("shadow_resolution_combobox");
            ComboBoxControl? srCombo = srEnt?.GetComponent<ComboBoxControl>();
            if (srCombo != null)
            {
                int[] srValues = { 512, 1024, 2048, 4096 };
                int idx = srCombo.SelectedIndex;
                if (idx >= 0 && idx < srValues.Length)
                    cfg["ShadowResolution"] = srValues[idx].ToString();
            }

            cfg["MasterVolume"] = GetSliderValue("volume_master").ToString("F1", System.Globalization.CultureInfo.InvariantCulture);
            cfg["MusicVolume"] = GetSliderValue("volume_music").ToString("F1", System.Globalization.CultureInfo.InvariantCulture);
            cfg["SFXVolume"] = GetSliderValue("volume_sfx").ToString("F1", System.Globalization.CultureInfo.InvariantCulture);

            SettingsConfig.Save(cfg);
        }
    }
}
