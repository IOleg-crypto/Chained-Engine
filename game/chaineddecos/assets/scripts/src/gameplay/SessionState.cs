using System;
using System.IO;
using Chained;

namespace ChainedDecos.Scripts
{
    public static class SessionState
    {
        private static readonly string SaveFilePath = Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "savegame.dat");

        public static string? LastGameplayScene { get; set; } = null;
        public static Vector3? SavedPlayerPosition { get; set; } = null;
        public static Vector3? SavedPlayerRotation { get; set; } = null;


        public static bool HasActiveSession
        {
            get
            {
                if (!string.IsNullOrEmpty(LastGameplayScene)) return true;
                return LoadFromDisk();
            }
        }

        public static void SaveToDisk()
        {
            try
            {
                if (string.IsNullOrEmpty(LastGameplayScene) || !SavedPlayerPosition.HasValue) return;

                using var writer = new StreamWriter(SaveFilePath, false);
                writer.WriteLine(LastGameplayScene);
                writer.WriteLine($"{SavedPlayerPosition.Value.X};{SavedPlayerPosition.Value.Y};{SavedPlayerPosition.Value.Z}");
                if (SavedPlayerRotation.HasValue)
                {
                    writer.WriteLine($"{SavedPlayerRotation.Value.X};{SavedPlayerRotation.Value.Y};{SavedPlayerRotation.Value.Z}");
                }
                else
                {
                    writer.WriteLine("0;0;0");
                }
                Log.Info($"[SessionState] Saved session to {SaveFilePath}");
            }
            catch (Exception ex)
            {
                Log.Warn($"[SessionState] Failed to save game state: {ex.Message}");
            }
        }

        public static bool LoadFromDisk()
        {
            try
            {
                if (!File.Exists(SaveFilePath)) return false;

                using var reader = new StreamReader(SaveFilePath);
                string? scene = reader.ReadLine();
                string? posStr = reader.ReadLine();
                string? rotStr = reader.ReadLine();

                if (string.IsNullOrEmpty(scene) || string.IsNullOrEmpty(posStr)) return false;

                var posParts = posStr.Split(';');
                if (posParts.Length == 3 &&
                    float.TryParse(posParts[0], out float px) &&
                    float.TryParse(posParts[1], out float py) &&
                    float.TryParse(posParts[2], out float pz))
                {
                    LastGameplayScene = scene;
                    SavedPlayerPosition = new Vector3(px, py, pz);

                    if (!string.IsNullOrEmpty(rotStr))
                    {
                        var rotParts = rotStr.Split(';');
                        if (rotParts.Length == 3 &&
                            float.TryParse(rotParts[0], out float rx) &&
                            float.TryParse(rotParts[1], out float ry) &&
                            float.TryParse(rotParts[2], out float rz))
                        {
                            SavedPlayerRotation = new Vector3(rx, ry, rz);
                        }
                    }
                    return true;
                }
            }
            catch (Exception ex)
            {
                Log.Warn($"[SessionState] Failed to load game state: {ex.Message}");
            }
            return false;
        }

        public static void ClearSave()
        {
            LastGameplayScene = null;
            SavedPlayerPosition = null;
            SavedPlayerRotation = null;
           
             if (File.Exists(SaveFilePath)) File.Delete(SaveFilePath);
            
           
        }
    }
}
