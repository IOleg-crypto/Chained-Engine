using System;
using System.Collections.Generic;
using System.IO;
using System.Globalization;
using Chained;

namespace ChainedDecos.Scripts
{
    // Static utility for reading/writing the game settings config file (settings.cfg).
    public static class SettingsConfig
    {
        private static string GetConfigPath()
        {
            return Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "settings.cfg");
        }

        public static Dictionary<string, string> Load()
        {
            var dict = new Dictionary<string, string>();
            string path = GetConfigPath();
            if (!File.Exists(path)) return dict;

            foreach (string line in File.ReadAllLines(path))
            {
                string trimmed = line.Trim();
                if (string.IsNullOrEmpty(trimmed) || trimmed.StartsWith("#")) continue;

                int eq = trimmed.IndexOf('=');
                if (eq > 0)
                {
                    string key = trimmed.Substring(0, eq).Trim();
                    string value = trimmed.Substring(eq + 1).Trim();
                    dict[key] = value;
                }
            }
            return dict;
        }

        public static void Save(Dictionary<string, string> settings)
        {
            string path = GetConfigPath();
            using (var writer = new StreamWriter(path))
            {
                foreach (var kv in settings)
                    writer.WriteLine($"{kv.Key}={kv.Value}");
            }
            Log.Info("Settings saved to " + path);
        }

        // Apply all settings from the config file to the engine.
        public static void ApplyAll()
        {
            var cfg = Load();
            if (cfg.Count == 0) return;

            if (cfg.TryGetValue("AntiAliasingSamples", out string? aaStr) && int.TryParse(aaStr, out int aa))
                AppWindow.SetAntiAliasingSamples(aa);

            if (cfg.TryGetValue("EnableShadows", out string? shStr) && bool.TryParse(shStr, out bool sh))
                AppWindow.SetEnableShadows(sh);

            if (cfg.TryGetValue("ShadowResolution", out string? srStr) && int.TryParse(srStr, out int sr))
                AppWindow.SetShadowResolution(sr);

            if (cfg.TryGetValue("Fullscreen", out string? fsStr) && bool.TryParse(fsStr, out bool fs))
                AppWindow.SetFullscreen(fs);

            if (cfg.TryGetValue("VSync", out string? vsStr) && bool.TryParse(vsStr, out bool vs))
                AppWindow.SetVSync(vs);

            if (cfg.TryGetValue("Resolution", out string? resStr))
            {
                string[] parts = resStr!.Split('x');
                if (parts.Length == 2 && int.TryParse(parts[0], out int w) && int.TryParse(parts[1], out int h))
                    AppWindow.SetSize(w, h);
            }

            if (cfg.TryGetValue("MasterVolume", out string? mvStr) &&
                float.TryParse(mvStr, NumberStyles.Float, CultureInfo.InvariantCulture, out float mv))
                Audio.SetMasterVolume(mv);

            if (cfg.TryGetValue("MusicVolume", out string? musStr) &&
                float.TryParse(musStr, NumberStyles.Float, CultureInfo.InvariantCulture, out float mus))
                Audio.SetMusicVolume(mus);

            if (cfg.TryGetValue("SFXVolume", out string? sfxStr) &&
                float.TryParse(sfxStr, NumberStyles.Float, CultureInfo.InvariantCulture, out float sfx))
                Audio.SetSFXVolume(sfx);
        }
    }
}
