using System;
using System.Collections.Generic;
using Chained;

namespace ChainedDecos.Scripts
{
    /// <summary>
    /// Manages the 3D lobby: positions player avatars in slots, handles chat, and transitions to game.
    /// Uses pre-placed avatar entities in the scene (avatar_0 .. avatar_3).
    /// </summary>
    public class LobbyManager : Script
    {
        private const int MaxAvatars = 4;
        private const string AvatarTagPrefix = "avatar_";

        public string PlayerPrefabPath = "prefab/player.chprefab";
        public string GameScene = "scenes/rpg_strategy_scene_mp.chscene";

        // Shared state from previous scenes
        public static ushort SelectedPort = 7777;
        public static int SelectedSkinIndex = 0;
        public static string SelectedMap = "scenes/rpg_strategy_scene_mp.chscene";
        public static int MaxClients = 4;
        public static uint RoomCode = 0;

        private float m_RefreshTimer = 0.0f;

        // Maps networkId -> slot index (0..3)
        private Dictionary<ulong, int> m_SlotByNetId = new Dictionary<ulong, int>();
        // Maps slot index -> networkId (0 = empty)
        private ulong[] m_SlotNetIds = new ulong[MaxAvatars];

        public override void OnCreate()
        {
            Log.Info("LobbyManager: Initialized");

            Network.SetPlayerPrefab(PlayerPrefabPath);

            // Hide all avatar slots initially
            for (int i = 0; i < MaxAvatars; i++)
            {
                SetSlotHidden(i);
            }

            if (Network.IsHost)
            {
                string hostName = PlayerSettings.Nickname;
                Network.SetLocalPlayerInfo(hostName, (byte)SelectedSkinIndex);
                Log.Info($"LobbyManager: Host ready (name='{hostName}', skin={SelectedSkinIndex})");
            }
            else if (Network.IsClient)
            {
                Network.SendPlayerInfo(PlayerSettings.Nickname, (byte)SelectedSkinIndex);
                Log.Info($"LobbyManager: Client sent player info (name='{PlayerSettings.Nickname}', skin={SelectedSkinIndex})");
            }
        }

        public override void OnUpdate(float deltaTime)
        {
            m_RefreshTimer += deltaTime;

            if (m_RefreshTimer >= 0.5f)
            {
                m_RefreshTimer = 0.0f;
                RefreshAvatars();
            }

            // Scene change is handled by NetworkSystem::CheckAndPropagateSceneChange
            // in C++, which sets Scene.PendingScenePath after net->Update() processes
            // the SceneChangeMessage packet. This avoids the timing issue where C# OnUpdate
            // ran before net->Update() could process incoming packets.
        }

        private void RefreshAvatars()
        {
            if (!Network.IsHost && !Network.IsClient)
                return;

            string json = Network.GetPlayerListJSON();
            if (string.IsNullOrEmpty(json) || json == "[]")
                return;

            // Parse player list JSON
            var players = ParsePlayerList(json);

            // Build set of current networkIds
            var currentIds = new HashSet<ulong>();
            foreach (var p in players)
                currentIds.Add(p.NetworkID);

            // Release slots for players that left
            for (int i = 0; i < MaxAvatars; i++)
            {
                if (m_SlotNetIds[i] != 0 && !currentIds.Contains(m_SlotNetIds[i]))
                {
                    ulong leftId = m_SlotNetIds[i];
                    m_SlotByNetId.Remove(leftId);
                    m_SlotNetIds[i] = 0;
                    SetSlotHidden(i);
                    Log.Info($"LobbyManager: Freed slot {i} (netID={leftId} left)");
                }
            }

            // Assign slots for new players
            foreach (var p in players)
            {
                if (m_SlotByNetId.ContainsKey(p.NetworkID))
                {
                    // Already has a slot - just update color
                    int slot = m_SlotByNetId[p.NetworkID];
                    SetSlotColor(slot, p.SkinIndex);
                    continue;
                }

                // Find free slot
                int freeSlot = -1;
                for (int i = 0; i < MaxAvatars; i++)
                {
                    if (m_SlotNetIds[i] == 0)
                    {
                        freeSlot = i;
                        break;
                    }
                }

                if (freeSlot < 0)
                {
                    Log.Warn("LobbyManager: No free avatar slots!");
                    continue;
                }

                m_SlotNetIds[freeSlot] = p.NetworkID;
                m_SlotByNetId[p.NetworkID] = freeSlot;
                SetSlotPosition(freeSlot, freeSlot);
                SetSlotColor(freeSlot, p.SkinIndex);
                Log.Info($"LobbyManager: Assigned netID={p.NetworkID} to slot {freeSlot} (skin={p.SkinIndex})");
            }
        }

        private void SetSlotPosition(int slot, int index)
        {
            Entity? avatar = Scene.FindEntityByTag(AvatarTagPrefix + slot);
            if (avatar == null || !avatar.IsValid)
                return;

            TransformComponent? t = avatar.GetComponent<TransformComponent>();
            if (t != null)
            {
                t.Translation = new Vector3(index * 2.0f, 0.5f, 0.0f);
            }
        }

        private void SetSlotColor(int slot, int skinIndex)
        {
            Entity? avatar = Scene.FindEntityByTag(AvatarTagPrefix + slot);
            if (avatar == null || !avatar.IsValid) return;
            // Skin colors are now managed via Material Editor (.chmat)
        }

        private void SetSlotHidden(int slot)
        {
            Entity? avatar = Scene.FindEntityByTag(AvatarTagPrefix + slot);
            if (avatar == null || !avatar.IsValid) return;
            var t = avatar.GetComponent<TransformComponent>();
            if (t != null) t.Translation = new Vector3(0, -100, 0);
        }

        private struct PlayerEntry
        {
            public ulong NetworkID;
            public string Name;
            public int SkinIndex;
        }

        private List<PlayerEntry> ParsePlayerList(string json)
        {
            var result = new List<PlayerEntry>();
            int cursor = 0;

            while (cursor < json.Length)
            {
                int objStart = json.IndexOf('{', cursor);
                if (objStart < 0) break;
                int objEnd = json.IndexOf('}', objStart);
                if (objEnd < 0) break;

                string obj = json.Substring(objStart, objEnd - objStart + 1);
                cursor = objEnd + 1;

                // Extract "id": <num>
                ulong networkId = 0;
                int idKey = obj.IndexOf("\"id\":");
                if (idKey >= 0)
                {
                    int valStart = idKey + 5;
                    while (valStart < obj.Length && (obj[valStart] == ' ' || obj[valStart] == ':')) valStart++;
                    int valEnd = valStart;
                    while (valEnd < obj.Length && char.IsDigit(obj[valEnd])) valEnd++;
                    if (valEnd > valStart)
                    {
                        ulong.TryParse(obj.Substring(valStart, valEnd - valStart), out networkId);
                    }
                }

                if (networkId == 0) continue;

                // Extract "name": "..."
                string name = "Player";
                int nameKey = obj.IndexOf("\"name\":\"");
                if (nameKey >= 0)
                {
                    int strStart = nameKey + 8;
                    int strEnd = obj.IndexOf('"', strStart);
                    if (strEnd > strStart)
                    {
                        name = obj.Substring(strStart, strEnd - strStart);
                    }
                }

                // Extract "skin": <num>
                int skinValue = 0;
                int skinKey = obj.IndexOf("\"skin\":");
                if (skinKey >= 0)
                {
                    int valStart = skinKey + 7;
                    while (valStart < obj.Length && (obj[valStart] == ' ' || obj[valStart] == ':')) valStart++;
                    int valEnd = valStart;
                    while (valEnd < obj.Length && char.IsDigit(obj[valEnd])) valEnd++;
                    if (valEnd > valStart)
                    {
                        int.TryParse(obj.Substring(valStart, valEnd - valStart), out skinValue);
                    }
                }

                result.Add(new PlayerEntry { NetworkID = networkId, Name = name, SkinIndex = skinValue });
            }

            return result;
        }

    }
}