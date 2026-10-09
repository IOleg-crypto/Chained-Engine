using Chained;

namespace ChainedDecos.Scripts
{
    /// <summary>
    /// Attach to the Options button entity INSTEAD of SceneTransitionComponent.
    /// Opens the OptionsOverlay without loading a new scene.
    /// </summary>
    public class ShowSettingsScript : Script
    {
        public override void OnUpdate(float deltaTime)
        {
            ButtonControl? btn = Entity.GetComponent<ButtonControl>();
            if (btn != null && btn.IsClicked)
            {
                OptionsOverlay.IsOpen = true;
            }
        }
    }
}
