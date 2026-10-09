using System;
using Chained;

namespace ChainedDecos.Scripts
{
    public enum MenuState
    {
        Main,
        Host,
        Join,
        Settings,
        Lobby
    }

    /// <summary>
    /// Єдиний менеджер для Головного меню. 
    /// Дозволяє перемикати панелі без завантаження нових сцен.
    /// </summary>
    public class MainMenuManager : Script
    {
        public MenuState CurrentState = MenuState.Main;

        // Теги панелей (повинні бути вказані на сутностях з UI)
        public string TagPanelMain = "PanelMain";
        public string TagPanelHost = "PanelHost";
        public string TagPanelJoin = "PanelJoin";
        public string TagPanelSettings = "PanelSettings";
        public string TagPanelLobby = "PanelLobby";

        // Теги кнопок навігації
        public string TagBtnGoHost = "BtnGoHost";
        public string TagBtnGoJoin = "BtnGoJoin";
        public string TagBtnGoSettings = "BtnGoSettings";
        public string TagBtnBack = "BtnBack";
        public string TagBtnExit = "BtnExit";

        public override void OnCreate()
        {
            Log.Info("MainMenuManager: Initialized.");
            SetState(MenuState.Main);
        }

        public override void OnUpdate(float deltaTime)
        {
            // Обробка кнопок навігації
            if (IsButtonClicked(TagBtnGoHost)) SetState(MenuState.Host);
            if (IsButtonClicked(TagBtnGoJoin)) SetState(MenuState.Join);
            if (IsButtonClicked(TagBtnGoSettings)) SetState(MenuState.Settings);
            if (IsButtonClicked(TagBtnBack)) SetState(MenuState.Main);
            
            if (IsButtonClicked(TagBtnExit))
            {
                // Вихід з гри (потрібен API движка або просто крашнути для тесту, 
                // але краще залишити тут місце для Application.Quit())
                Log.Info("Exit clicked!");
            }

            // Якщо ми підключились або створили сервер — автоматично переходимо в Лобі
            if (Network.IsConnected && CurrentState != MenuState.Lobby)
            {
                SetState(MenuState.Lobby);
            }
            // Якщо сервер впав / від'єдналися — повертаємось у головне меню
            else if (!Network.IsConnected && CurrentState == MenuState.Lobby)
            {
                SetState(MenuState.Main);
            }
        }

        private void SetState(MenuState newState)
        {
            Log.Info($"MainMenuManager: Switching state to {newState}");
            CurrentState = newState;

            SetPanelActive(TagPanelMain, newState == MenuState.Main);
            SetPanelActive(TagPanelHost, newState == MenuState.Host);
            SetPanelActive(TagPanelJoin, newState == MenuState.Join);
            SetPanelActive(TagPanelSettings, newState == MenuState.Settings);
            SetPanelActive(TagPanelLobby, newState == MenuState.Lobby);
        }

        private void SetPanelActive(string tag, bool active)
        {
            if (string.IsNullOrEmpty(tag)) return;
            var entity = Scene.FindEntityByTag(tag);
            if (entity != null && entity.IsValid)
            {
                var widget = entity.GetComponent<WidgetControl>();
                if (widget != null)
                {
                    widget.IsActive = active;
                }
                else
                {
                    // Якщо немає WidgetControl, можливо це батьківський об'єкт
                    // Приховаємо його через зміщення масштабу (хитрий хак, якщо IsActive не працює для групи)
                    var transform = entity.GetComponent<TransformComponent>();
                    if (transform != null)
                    {
                        transform.Scale = active ? new Vector3(1, 1, 1) : new Vector3(0, 0, 0);
                    }
                }
            }
        }

        private bool IsButtonClicked(string tag)
        {
            if (string.IsNullOrEmpty(tag)) return false;
            var entity = Scene.FindEntityByTag(tag);
            if (entity != null && entity.IsValid)
            {
                var btn = entity.GetComponent<ButtonControl>();
                return btn != null && btn.IsClicked;
            }
            return false;
        }
    }
}
