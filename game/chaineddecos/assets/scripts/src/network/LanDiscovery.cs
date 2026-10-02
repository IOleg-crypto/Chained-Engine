using System;
using System.Collections.Generic;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using Chained;

namespace ChainedDecos.Scripts
{
    public class LanServerInfo
    {
        public string Name         { get; set; } = "Server";
        public string Map          { get; set; } = "Default";
        public int    PlayerCount  { get; set; } = 1;
        public int    MaxPlayers   { get; set; } = 8;
        public ushort Port         { get; set; } = 7777;
        public string IpAddress    { get; set; } = "127.0.0.1";
        public float  LastSeenTime { get; set; } = 0f;
        public int    PingMs       { get; set; } = 0;
    }

    public static class LanDiscoveryBeacon
    {
        public const int DiscoveryPort = 7778;

        private static Thread?      s_BeaconThread;
        private static bool         s_IsRunning;
        private static string       s_ServerName  = "Server";
        private static string       s_MapName     = "Default";
        private static ushort       s_GamePort    = 7777;
        private static int          s_MaxPlayers  = 8;
        private static readonly object s_Lock     = new object();

        public static bool IsRunning => s_IsRunning;

        public static void Start(string serverName, string mapName, ushort gamePort, int maxPlayers)
        {
            Stop();

            lock (s_Lock)
            {
                s_ServerName = string.IsNullOrWhiteSpace(serverName) ? "Chained Server" : serverName.Trim();
                s_MapName    = string.IsNullOrWhiteSpace(mapName) ? "Default" : mapName.Trim();
                s_GamePort   = gamePort;
                s_MaxPlayers = maxPlayers;
                s_IsRunning  = true;
            }

            s_BeaconThread = new Thread(BeaconLoop)
            {
                IsBackground = true,
                Name = "LanDiscoveryBeacon"
            };
            s_BeaconThread.Start();
            Log.Info("[LanDiscovery] Beacon started for '" + s_ServerName + "' on port " + DiscoveryPort);
        }

        public static void UpdateInfo(string? mapName = null, int? maxPlayers = null)
        {
            lock (s_Lock)
            {
                if (mapName != null) s_MapName = mapName;
                if (maxPlayers != null) s_MaxPlayers = maxPlayers.Value;
            }
        }

        public static void Stop()
        {
            s_IsRunning = false;
            if (s_BeaconThread != null && s_BeaconThread.IsAlive)
            {
                try
                {
                    s_BeaconThread.Join(500);
                }
                catch { }
                s_BeaconThread = null;
            }
        }

        private static void BeaconLoop()
        {
            UdpClient? client = null;
            try
            {
                client = new UdpClient();
                client.EnableBroadcast = true;
                IPEndPoint broadcastEp = new IPEndPoint(IPAddress.Broadcast, DiscoveryPort);

                while (s_IsRunning)
                {
                    string name, map;
                    ushort port;
                    int max;
                    lock (s_Lock)
                    {
                        name = s_ServerName;
                        map  = s_MapName;
                        port = s_GamePort;
                        max  = s_MaxPlayers;
                    }

                    int count = Network.PlayerCount > 0 ? Network.PlayerCount : 1;
                    long timestamp = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();

                    string payload = string.Format("CHDISC|{0}|{1}|{2}|{3}|{4}|{5}", name, map, count, max, port, timestamp);
                    byte[] bytes   = Encoding.UTF8.GetBytes(payload);

                    try
                    {
                        client.Send(bytes, bytes.Length, broadcastEp);
                    }
                    catch (Exception ex)
                    {
                        Log.Warn("[LanDiscovery] Beacon send error: " + ex.Message);
                    }

                    Thread.Sleep(1200);
                }
            }
            catch (Exception ex)
            {
                Log.Warn("[LanDiscovery] Beacon loop terminated: " + ex.Message);
            }
            finally
            {
                try { client?.Close(); } catch { }
            }
        }
    }

    public static class LanDiscoveryListener
    {
        public const int DiscoveryPort = 7778;

        private static Thread?                    s_ListenThread;
        private static bool                       s_IsListening;
        private static readonly List<LanServerInfo> s_DiscoveredServers = new List<LanServerInfo>();
        private static readonly object            s_ListLock = new object();

        public static bool IsListening => s_IsListening;

        public static void Start()
        {
            if (s_IsListening) return;

            lock (s_ListLock)
            {
                s_DiscoveredServers.Clear();
            }

            s_IsListening = true;
            s_ListenThread = new Thread(ListenLoop)
            {
                IsBackground = true,
                Name = "LanDiscoveryListener"
            };
            s_ListenThread.Start();
            Log.Info("[LanDiscovery] Listener active on port " + DiscoveryPort);
        }

        public static void Stop()
        {
            s_IsListening = false;
            if (s_ListenThread != null && s_ListenThread.IsAlive)
            {
                try
                {
                    s_ListenThread.Join(500);
                }
                catch { }
                s_ListenThread = null;
            }
            lock (s_ListLock)
            {
                s_DiscoveredServers.Clear();
            }
        }

        public static void Refresh()
        {
            lock (s_ListLock)
            {
                s_DiscoveredServers.Clear();
            }
        }

        public static List<LanServerInfo> GetActiveServers(float timeoutSeconds = 4.0f)
        {
            lock (s_ListLock)
            {
                float now = (float)DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() / 1000f;
                s_DiscoveredServers.RemoveAll(s => (now - s.LastSeenTime) > timeoutSeconds);
                return new List<LanServerInfo>(s_DiscoveredServers);
            }
        }

        private static void ListenLoop()
        {
            UdpClient? client = null;
            try
            {
                client = new UdpClient();
                client.Client.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
                client.Client.Bind(new IPEndPoint(IPAddress.Any, DiscoveryPort));
                client.Client.ReceiveTimeout = 800;

                while (s_IsListening)
                {
                    try
                    {
                        IPEndPoint remoteEp = new IPEndPoint(IPAddress.Any, 0);
                        byte[] bytes = client.Receive(ref remoteEp);
                        if (bytes.Length == 0) continue;

                        string message = Encoding.UTF8.GetString(bytes);
                        if (!message.StartsWith("CHDISC|")) continue;

                        string[] parts = message.Split('|');
                        if (parts.Length < 7) continue;

                        string name = parts[1];
                        string map  = parts[2];
                        int.TryParse(parts[3], out int players);
                        int.TryParse(parts[4], out int max);
                        ushort.TryParse(parts[5], out ushort port);
                        long.TryParse(parts[6], out long sentTime);

                        long nowMs = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();
                        int ping = (int)Math.Max(0, Math.Min(999, nowMs - sentTime));

                        string ip = remoteEp.Address.ToString();
                        if (ip == "::ffff:127.0.0.1") ip = "127.0.0.1";

                        float nowSec = (float)nowMs / 1000f;

                        lock (s_ListLock)
                        {
                            var existing = s_DiscoveredServers.Find(s => s.IpAddress == ip && s.Port == port);
                            if (existing != null)
                            {
                                existing.Name         = name;
                                existing.Map          = map;
                                existing.PlayerCount  = players;
                                existing.MaxPlayers   = max;
                                existing.LastSeenTime = nowSec;
                                existing.PingMs       = ping;
                            }
                            else
                            {
                                s_DiscoveredServers.Add(new LanServerInfo
                                {
                                    Name         = name,
                                    Map          = map,
                                    PlayerCount  = players,
                                    MaxPlayers   = max,
                                    Port         = port,
                                    IpAddress    = ip,
                                    LastSeenTime = nowSec,
                                    PingMs       = ping
                                });
                            }
                        }
                    }
                    catch (SocketException ex)
                    {
                        if (ex.SocketErrorCode == SocketError.TimedOut)
                        {
                            continue;
                        }
                    }
                    catch
                    {
                    }
                }
            }
            catch (Exception ex)
            {
                Log.Warn("[LanDiscovery] Listener error: " + ex.Message);
            }
            finally
            {
                try { client?.Close(); } catch { }
            }
        }
    }
}
