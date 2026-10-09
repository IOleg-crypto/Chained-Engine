using Chained;

namespace ChainedDecos.Scripts
{
    public class ResumeButtonScript : Script
    {
        public override void OnCreate()
        {
            UpdateVisibility();
        }

        public override void OnStart()
        {
            UpdateVisibility();
        }

        public override void OnUpdate(float deltaTime)
        {
            ButtonControl? btn = Entity.GetComponent<ButtonControl>();
            if (btn == null) return;

            bool hasSession = Scene.HasActiveSession();
            
            // Sync active state to ECS ControlComponent
            if (btn.IsActive != hasSession)
            {
                btn.IsActive = hasSession;
            }

            if (hasSession && btn.IsActive && btn.IsClicked)
            {
                Log.Info("[ResumeButton] Resuming active game session...");
                Scene.ResumeSession();
            }
        }

        private void UpdateVisibility()
        {
            ButtonControl? btn = Entity.GetComponent<ButtonControl>();
            if (btn != null)
            {
                btn.IsActive = Scene.HasActiveSession();
            }
        }
    }
}
