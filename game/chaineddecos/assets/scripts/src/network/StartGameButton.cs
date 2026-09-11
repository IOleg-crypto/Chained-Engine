using System;
using Chained;

namespace ChainedDecos.Scripts
{
    /// <summary>
    /// Attach directly to "Start Game" button entity.
    /// Broadcasts scene change to all clients and loads the selected map on the host.
    /// </summary>
    public class StartGameButton : Script
    {
        public override void OnUpdate(float deltaTime)
        {
            ButtonControl? btn = Entity.GetComponent<ButtonControl>();

            // Client restriction: Only the host can start the game!
            if (!Network.IsHost)
            {
                if (btn != null && btn.Label != "Waiting for Host...")
                {
                    btn.Label = "Waiting for Host...";
                }
                return;
            }

            if (btn == null || !btn.IsClicked)
                return;

            string map = LobbyManager.SelectedMap;
            int playerCount = Network.ClientCount;
            Log.Info($"[StartGameButton] Host starting game with {playerCount} client(s), map: {map}");

            Network.BroadcastSceneChange(map);
            Scene.LoadScene(map);
        }
    }
}
