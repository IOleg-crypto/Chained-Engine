using System;
using Chained;

namespace ChainedDecos.Scripts
{
    public class ResumeButtonScript : Script
    {
        public string DefaultScene = "scenes/game_mode_selection.chscene";

        public override void OnCreate()
        {
            UpdateButtonVisibility();
        }

        public override void OnUpdate(float deltaTime)
        {
            ButtonControl? btn = Entity.GetComponent<ButtonControl>();
            if (btn == null) return;

            UpdateButtonVisibility();

            if (btn.IsActive && btn.IsClicked)
            {
                string target = SessionState.HasActiveSession ? SessionState.LastGameplayScene! : DefaultScene;
                Log.Info($"[ResumeButton] Resuming scene: {target} (HasSession={SessionState.HasActiveSession})");
                Scene.LoadScene(target);
            }
        }

        private void UpdateButtonVisibility()
        {
            ButtonControl? btn = Entity.GetComponent<ButtonControl>();
            if (btn != null)
            {
                btn.IsActive = SessionState.HasActiveSession;
            }
        }
    }
}
