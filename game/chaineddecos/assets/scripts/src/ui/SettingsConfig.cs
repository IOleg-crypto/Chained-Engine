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
            string baseDir = AppDomain.CurrentDomain.BaseDirectory;
            string localPath = Path.Combine(baseDir, "settings.cfg");
            if (File.Exists(localPath)) return localPath;

            string cwdPath = Path.Combine(Directory.GetCurrentDirectory(), "settings.cfg");
            if (File.Exists(cwdPath)) return cwdPath;

            return localPath;
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

            if (cfg.TryGetValue("Resolution", out string? resStr) && !string.IsNullOrEmpty(resStr))
            {
                string[] parts = resStr!.Split('x');
                if (parts.Length == 2 && int.TryParse(parts[0], out int w) && int.TryParse(parts[1], out int h))
                    AppWindow.SetSize(w, h);
            }

            if (cfg.TryGetValue("MuteAudio", out string? muteStr) && bool.TryParse(muteStr, out bool mute) && mute)
            {
                Audio.SetMasterVolume(0f);
                Audio.SetMusicVolume(0f);
                Audio.SetSFXVolume(0f);
            }
            else
            {
                if (cfg.TryGetValue("MasterVolume", out string? mvStr) &&
                    float.TryParse(mvStr, NumberStyles.Float, CultureInfo.InvariantCulture, out float mv))
                    Audio.SetMasterVolume(mv > 1.0f ? mv / 100f : mv);

                if (cfg.TryGetValue("MusicVolume", out string? musStr) &&
                    float.TryParse(musStr, NumberStyles.Float, CultureInfo.InvariantCulture, out float mus))
                    Audio.SetMusicVolume(mus > 1.0f ? mus / 100f : mus);

                if (cfg.TryGetValue("SFXVolume", out string? sfxStr) &&
                    float.TryParse(sfxStr, NumberStyles.Float, CultureInfo.InvariantCulture, out float sfx))
                    Audio.SetSFXVolume(sfx > 1.0f ? sfx / 100f : sfx);
            }

            SyncAllAudioEntities();
        }

        public static void SyncAllAudioEntities()
        {
            var cfg = Load();
            bool isMuted = false;
            if (cfg.TryGetValue("MuteAudio", out string? muteStr) && bool.TryParse(muteStr, out bool m))
                isMuted = m;

            float master = 100f;
            if (cfg.TryGetValue("MasterVolume", out string? mvStr) &&
                float.TryParse(mvStr, NumberStyles.Float, CultureInfo.InvariantCulture, out float mv))
                master = mv > 1.0f ? mv : mv * 100f;

            float music = 100f;
            if (cfg.TryGetValue("MusicVolume", out string? musStr) &&
                float.TryParse(musStr, NumberStyles.Float, CultureInfo.InvariantCulture, out float mus))
                music = mus > 1.0f ? mus : mus * 100f;

            float sfx = 100f;
            if (cfg.TryGetValue("SFXVolume", out string? sfxStr) &&
                float.TryParse(sfxStr, NumberStyles.Float, CultureInfo.InvariantCulture, out float parsedSFX))
                sfx = parsedSFX > 1.0f ? parsedSFX : parsedSFX * 100f;

            bool silence = isMuted || master <= 0.001f;

            // NOTE: Do NOT call Audio.StopAll() here — it stops miniaudio sounds but does NOT
            // reset AudioComponent.IsPlaying flags, so audio_system restarts sounds next frame.
            // comp.Stop() correctly sets IsPlaying=false, preventing that restart loop.
            //
            // NOTE: Do NOT call comp.Play() here — this function must only MUTE/UNMUTE sounds.
            // Sounds with PlayOnStart=false are controlled by gameplay scripts (e.g. PlayerFall).
            // Force-playing them here would break gameplay audio logic.

            ulong[] audioEntities = Entity.FindAllWithComponent<AudioComponent>();
            foreach (ulong id in audioEntities)
            {
                Entity e = new Entity(id);
                AudioComponent? comp = e.GetComponent<AudioComponent>();
                if (comp == null) continue;

                if (silence)
                {
                    comp.Volume = 0f;
                    comp.Stop();
                }
                else
                {
                    // Restore volume — gameplay scripts will call Play() when appropriate
                    float finalVol = (master / 100f) * (music / 100f);
                    comp.Volume = finalVol;
                }
            }
        }
    }
}
