using System;
using System.Runtime.InteropServices;

namespace Chained
{
    /// <summary>
    /// Static API for multiplayer networking.
    /// Provides HostGame, ConnectTo, Disconnect, player list, chat, scene change, and status queries.
    /// </summary>
    public static class Network
    {
        /// <summary>Default UDP port for game networking.</summary>
        public const ushort DefaultPort = 7777;

#pragma warning disable 0649

        internal static unsafe delegate* unmanaged<ushort, int, void> Network_HostGame_Ptr;
        internal static unsafe delegate* unmanaged<char*, ushort, void> Network_ConnectTo_Ptr;
        internal static unsafe delegate* unmanaged<void> Network_Disconnect_Ptr;
        internal static unsafe delegate* unmanaged<byte> Network_IsHost_Ptr;
        internal static unsafe delegate* unmanaged<byte> Network_IsClient_Ptr;
        internal static unsafe delegate* unmanaged<byte> Network_IsConnected_Ptr;
        internal static unsafe delegate* unmanaged<byte> Network_IsFullyConnected_Ptr;
        internal static unsafe delegate* unmanaged<int> Network_GetClientCount_Ptr;
        internal static unsafe delegate* unmanaged<int> Network_GetMaxClients_Ptr;
        internal static unsafe delegate* unmanaged<int> Network_GetRole_Ptr;
        internal static unsafe delegate* unmanaged<char*, int, void> Network_GetListenAddress_Ptr;
        internal static unsafe delegate* unmanaged<char*, int, void> Network_GetPublicAddress_Ptr;
        internal static unsafe delegate* unmanaged<char*, void> Network_BroadcastSceneChange_Ptr;
        internal static unsafe delegate* unmanaged<byte> Network_HasPendingSceneChange_Ptr;
        internal static unsafe delegate* unmanaged<char*, int, void> Network_GetPendingSceneChange_Ptr;
        internal static unsafe delegate* unmanaged<void> Network_ClearPendingSceneChange_Ptr;

        // Player list
        internal static unsafe delegate* unmanaged<char*, byte, void> Network_SetLocalPlayerInfo_Ptr;
        internal static unsafe delegate* unmanaged<char*, byte, void> Network_SendPlayerInfo_Ptr;
        internal static unsafe delegate* unmanaged<int> Network_GetPlayerCount_Ptr;
        internal static unsafe delegate* unmanaged<char*, int, void> Network_GetPlayerListJSON_Ptr;
        internal static unsafe delegate* unmanaged<ulong> Network_GetLocalNetworkID_Ptr;

        // Chat
        internal static unsafe delegate* unmanaged<char*, void> Network_SendChatMessage_Ptr;
        internal static unsafe delegate* unmanaged<byte> Network_HasPendingChat_Ptr;
        internal static unsafe delegate* unmanaged<char*, int, void> Network_GetPendingChatJSON_Ptr;
        internal static unsafe delegate* unmanaged<void> Network_ClearPendingChat_Ptr;

        // Prefab
        internal static unsafe delegate* unmanaged<char*, void> Network_SetPlayerPrefab_Ptr;

        // UPnP
        internal static unsafe delegate* unmanaged<byte> Network_IsUpnpAvailable_Ptr;

        // STUN / NAT Traversal
        internal static unsafe delegate* unmanaged<byte> Network_HasStunResult_Ptr;
        internal static unsafe delegate* unmanaged<char*, int, void> Network_GetStunPublicAddress_Ptr;
        internal static unsafe delegate* unmanaged<char*, ushort, void> Network_StartHolePunch_Ptr;
        internal static unsafe delegate* unmanaged<ushort, void> Network_QueryStun_Ptr;

        // Room Code Signaling
        internal static unsafe delegate* unmanaged<ushort, int, uint> Network_HostRoom_Ptr;
        internal static unsafe delegate* unmanaged<uint, void> Network_ConnectRoom_Ptr;
        internal static unsafe delegate* unmanaged<uint> Network_GetRoomCode_Ptr;

        // Ping / RTT
        internal static unsafe delegate* unmanaged<uint> Network_GetPing_Ptr;

#pragma warning restore 0649

        /// <summary>Starts a listen server on the given port.</summary>
        public static unsafe void HostGame(ushort port = DefaultPort, int maxClients = 4)
        {
            if (Network_HostGame_Ptr == null) return;
            Network_HostGame_Ptr(port, maxClients);
        }

        /// <summary>Connects to a remote server.</summary>
        public static unsafe void ConnectTo(string ip, ushort port = DefaultPort)
        {
            if (Network_ConnectTo_Ptr == null || string.IsNullOrEmpty(ip)) return;
            fixed (char* ptr = ip) Network_ConnectTo_Ptr(ptr, port);
        }

        /// <summary>Disconnects from the current session.</summary>
        public static unsafe void Disconnect()
        {
            if (Network_Disconnect_Ptr == null) return;
            Network_Disconnect_Ptr();
        }

        /// <summary>True when this instance is the host (listen server).</summary>
        public static unsafe bool IsHost => Network_IsHost_Ptr != null && Network_IsHost_Ptr() != 0;

        /// <summary>True when this instance is a connected client.</summary>
        public static unsafe bool IsClient => Network_IsClient_Ptr != null && Network_IsClient_Ptr() != 0;

        /// <summary>True when connected (host or client).</summary>
        public static unsafe bool IsConnected => Network_IsConnected_Ptr != null && Network_IsConnected_Ptr() != 0;

        /// <summary>True only after ENet handshake completes (client-side). Use this instead of IsConnected for connection state checks.</summary>
        public static unsafe bool IsFullyConnected => Network_IsFullyConnected_Ptr != null && Network_IsFullyConnected_Ptr() != 0;

        /// <summary>Number of connected clients (host-side only).</summary>
        public static unsafe int ClientCount => Network_GetClientCount_Ptr != null ? Network_GetClientCount_Ptr() : 0;

        /// <summary>Maximum number of clients allowed on this server (set via HostGame).</summary>
        public static unsafe int MaxClients => Network_GetMaxClients_Ptr != null ? Network_GetMaxClients_Ptr() : 0;

        /// <summary>Network role: 0=Offline, 1=Host, 2=Client.</summary>
        public static unsafe int GetRole() => Network_GetRole_Ptr != null ? Network_GetRole_Ptr() : 0;

		/// <summary>
        /// Address of the local listen socket (host-side). This is a LAN address, not the
        /// router's public IP — hosting over the internet requires a forwarded UDP port.
        /// </summary>
        public static unsafe string GetListenAddress()
        {
            if (Network_GetListenAddress_Ptr == null) return string.Empty;
            sbyte* buf = stackalloc sbyte[64];
            Network_GetListenAddress_Ptr((char*)buf, 64);
            return new string(buf);
        }

        /// <summary>
        /// Public IP:PORT string for internet hosting (e.g. "203.0.113.5:7777").
        /// Returns "Fetching..." for a second or two after HostGame() while the
        /// engine resolves the public address from api.ipify.org.
        /// </summary>
        public static unsafe string GetPublicAddress()
        {
            if (Network_GetPublicAddress_Ptr == null) return string.Empty;
            sbyte* buf = stackalloc sbyte[64];
            Network_GetPublicAddress_Ptr((char*)buf, 64);
            return new string(buf);
        }


        public static unsafe void BroadcastSceneChange(string scenePath)
        {
            if (Network_BroadcastSceneChange_Ptr == null || string.IsNullOrEmpty(scenePath)) return;
            fixed (char* ptr = scenePath) Network_BroadcastSceneChange_Ptr(ptr);
        }

        /// <summary>True when a scene change packet was received from the host (client-side).</summary>
        public static unsafe bool HasPendingSceneChange => Network_HasPendingSceneChange_Ptr != null && Network_HasPendingSceneChange_Ptr() != 0;

        /// <summary>Returns the pending scene path received from the host, or empty string.</summary>
        public static unsafe string GetPendingSceneChange()
        {
            if (Network_GetPendingSceneChange_Ptr == null) return string.Empty;
            sbyte* buf = stackalloc sbyte[256];
            Network_GetPendingSceneChange_Ptr((char*)buf, 256);
            return new string(buf);
        }

        /// <summary>Clears the pending scene change flag.</summary>
        public static unsafe void ClearPendingSceneChange()
        {
            if (Network_ClearPendingSceneChange_Ptr == null) return;
            Network_ClearPendingSceneChange_Ptr();
        }

        // ── Player List ─────────────────────────────────────────────────

        /// <summary>Sets the local player's name and skin index (call before hosting/connecting).</summary>
        public static unsafe void SetLocalPlayerInfo(string name, byte skinIndex = 0)
        {
            if (Network_SetLocalPlayerInfo_Ptr == null || string.IsNullOrEmpty(name)) return;
            fixed (char* ptr = name) Network_SetLocalPlayerInfo_Ptr(ptr, skinIndex);
        }

        /// <summary>Sends player info to the host (client-side, called automatically on connect).</summary>
        public static unsafe void SendPlayerInfo(string name, byte skinIndex = 0)
        {
            if (Network_SendPlayerInfo_Ptr == null || string.IsNullOrEmpty(name)) return;
            fixed (char* ptr = name) Network_SendPlayerInfo_Ptr(ptr, skinIndex);
        }

        /// <summary>Number of players in the lobby (including host).</summary>
        public static unsafe int PlayerCount => Network_GetPlayerCount_Ptr != null ? Network_GetPlayerCount_Ptr() : 0;

        /// <summary>Returns the player count in the lobby.</summary>
        public static int GetPlayerCount() => PlayerCount;

        /// <summary>Returns the player list as a JSON string.</summary>
        public static unsafe string GetPlayerListJSON()
        {
            if (Network_GetPlayerListJSON_Ptr == null) return "[]";
            sbyte* buf = stackalloc sbyte[4096];
            Network_GetPlayerListJSON_Ptr((char*)buf, 4096);
            int len = 0;
            while (buf[len] != 0 && len < 4095) len++;
            return System.Text.Encoding.UTF8.GetString((byte*)buf, len);
        }

        /// <summary>Returns the local player's network ID (assigned by host).</summary>
        public static unsafe ulong GetLocalNetworkID()
        {
            return Network_GetLocalNetworkID_Ptr != null ? Network_GetLocalNetworkID_Ptr() : 0;
        }

        // ── Chat ────────────────────────────────────────────────────────

        /// <summary>Sends a chat message to all players.</summary>
        public static unsafe void SendChatMessage(string message)
        {
            if (Network_SendChatMessage_Ptr == null || string.IsNullOrEmpty(message)) return;
            fixed (char* ptr = message) Network_SendChatMessage_Ptr(ptr);
        }

        /// <summary>True when there are pending chat messages to display.</summary>
        public static unsafe bool HasPendingChat => Network_HasPendingChat_Ptr != null && Network_HasPendingChat_Ptr() != 0;

        /// <summary>Returns pending chat messages as JSON.</summary>
        public static unsafe string GetPendingChatJSON()
        {
            if (Network_GetPendingChatJSON_Ptr == null) return "[]";
            sbyte* buf = stackalloc sbyte[8192];
            Network_GetPendingChatJSON_Ptr((char*)buf, 8192);
            int len = 0;
            while (buf[len] != 0 && len < 8191) len++;
            return System.Text.Encoding.UTF8.GetString((byte*)buf, len);
        }

        /// <summary>Clears pending chat messages after reading.</summary>
        public static unsafe void ClearPendingChat()
        {
            if (Network_ClearPendingChat_Ptr == null) return;
            Network_ClearPendingChat_Ptr();
        }

        // ── Prefab ──────────────────────────────────────────────────────

        /// <summary>Sets the player prefab path for network avatar spawning.</summary>
        public static unsafe void SetPlayerPrefab(string path)
        {
            if (Network_SetPlayerPrefab_Ptr == null || string.IsNullOrEmpty(path)) return;
            fixed (char* ptr = path) Network_SetPlayerPrefab_Ptr(ptr);
        }

        // ── UPnP ─────────────────────────────────────────────────────────

        /// <summary>True when UPnP port forwarding is available on the local network.</summary>
        public static unsafe bool IsUpnpAvailable => Network_IsUpnpAvailable_Ptr != null && Network_IsUpnpAvailable_Ptr() != 0;

        // ── STUN / NAT Traversal ─────────────────────────────────────────

        /// <summary>True when STUN has successfully queried the public endpoint.</summary>
        public static unsafe bool HasStunResult => Network_HasStunResult_Ptr != null && Network_HasStunResult_Ptr() != 0;

        /// <summary>Returns the STUN-discovered public IP:port (e.g. "203.0.113.5:7777").</summary>
        public static unsafe string GetStunPublicAddress()
        {
            if (Network_GetStunPublicAddress_Ptr == null) return string.Empty;
            sbyte* buf = stackalloc sbyte[64];
            Network_GetStunPublicAddress_Ptr((char*)buf, 64);
            return new string(buf);
        }

        /// <summary>Starts UDP hole punching to the remote host's public IP:port.</summary>
        public static unsafe void StartHolePunch(string remoteIP, ushort remotePort)
        {
            if (Network_StartHolePunch_Ptr == null || string.IsNullOrEmpty(remoteIP)) return;
            fixed (char* ptr = remoteIP) Network_StartHolePunch_Ptr(ptr, remotePort);
        }

        /// <summary>Queries STUN servers for the public endpoint.</summary>
        public static unsafe void QueryStun(ushort localPort)
        {
            if (Network_QueryStun_Ptr == null) return;
            Network_QueryStun_Ptr(localPort);
        }

        // ── Room Code (STUN Hole Punch) ───────────────────────────────────

        /// <summary>
        /// Hosts a game and registers with the signaling server.
        /// Returns a 4-digit room code clients can use to connect without knowing your IP.
        /// </summary>
        public static unsafe uint HostRoom(ushort port = DefaultPort, int maxClients = 4)
        {
            if (Network_HostRoom_Ptr == null) { HostGame(port, maxClients); return 0; }
            return Network_HostRoom_Ptr(port, maxClients);
        }

        /// <summary>
        /// Connects to a room by its 4-digit code (e.g. 4821).
        /// Automatically performs STUN + hole punch via the signaling server.
        /// </summary>
        public static unsafe void ConnectRoom(uint roomCode)
        {
            if (Network_ConnectRoom_Ptr == null) return;
            Network_ConnectRoom_Ptr(roomCode);
        }

        /// <summary>
        /// Returns the current room code (host-side only). Returns 0 if not in a room.
        /// </summary>
        public static unsafe uint GetRoomCode()
        {
            return Network_GetRoomCode_Ptr != null ? Network_GetRoomCode_Ptr() : 0;
        }

        /// <summary>Returns the round-trip time (RTT) in milliseconds to the server. Returns 0 if not connected.</summary>
        public static unsafe uint GetPing()
        {
            if (Network_GetPing_Ptr == null) return 0;
            return Network_GetPing_Ptr();
        }


    }
}
