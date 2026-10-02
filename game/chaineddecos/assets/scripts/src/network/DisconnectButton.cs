using Chained;

namespace ChainedDecos.Scripts
{
    /// <summary>
    /// Attach directly to the "Disconnect" button entity in the lobby.
    /// Tears down the network session and returns to the multiplayer menu.
    /// </summary>
    public class DisconnectButton : Script
    {
        public string MenuScene = "scenes/multiplayer_menu.chscene";

        public override void OnUpdate(float deltaTime)
        {
            ButtonControl? btn = Entity.GetComponent<ButtonControl>();
            if (btn == null || !btn.IsClicked)
                return;

            Log.Info("[DisconnectButton] Leaving session");
            Network.Disconnect();
            Scene.LoadScene(MenuScene);
        }
    }
}
