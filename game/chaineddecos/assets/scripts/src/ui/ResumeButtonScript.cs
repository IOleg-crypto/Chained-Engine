using System;
using Chained;

namespace ChainedDecos.Scripts
{
    public class ResumeButtonScript : Script
    {
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
                Log.Info("[ResumeButton] Resuming active game session...");
                Scene.ResumeSession();
            }
        }

        private void UpdateButtonVisibility()
        {
            ButtonControl? btn = Entity.GetComponent<ButtonControl>();
            if (btn != null)
            {
                btn.IsActive = Scene.HasActiveSession();
            }
        }
    }
}
