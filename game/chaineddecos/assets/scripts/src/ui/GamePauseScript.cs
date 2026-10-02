using System;
using Chained;

namespace ChainedDecos.Scripts
{
    public class GamePauseScript : Script
    {
        public string MenuScene = "scenes/start_menu.chscene";
        public string CurrentScene = "";

        public override void OnCreate()
        {
            Priority = 100;
        }

        public override void OnUpdate(float deltaTime)
        {
            if (Input.IsKeyPressed(Key.Escape))
            {
                Log.Info("Returning to main menu...");
                string sceneToSave = !string.IsNullOrEmpty(CurrentScene) ? CurrentScene : Scene.GetCurrentScenePath();
                if (!string.IsNullOrEmpty(sceneToSave))
                {
                    SessionState.LastGameplayScene = sceneToSave;
                }

                var player = Scene.FindEntityByTag("Player");
                if (player != null)
                {
                    var transform = player.GetComponent<TransformComponent>();
                    if (transform != null)
                    {
                        SessionState.SavedPlayerPosition = transform.Translation;
                        SessionState.SavedPlayerRotation = transform.Rotation;
                    }
                }
                SessionState.SaveToDisk();

                if (Network.IsConnected)
                {
                    Network.Disconnect();
                }
                Scene.LoadScene(MenuScene);
            }
        }
    }
}
