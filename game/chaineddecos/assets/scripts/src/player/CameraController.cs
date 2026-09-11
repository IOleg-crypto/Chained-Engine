using System;
using Chained;

namespace ChainedDecos.Scripts
{
public class CameraController : Script
{
    public float LookSensitivity = 0.5f;
    public float Distance = 10.0f;
    public float Pitch = 45.0f;
    public float Yaw = 0.0f;
    public string TargetTag = "Player";

    private ulong _localPlayerId = 0;

    public override void OnCreate()
    {
        Priority = -10;

        Entity? camEntity = Scene.GetMainCamera();
        if (camEntity == null)
        {
            Log.Error($"[CameraController] FAILED: No main camera found in scene!");
            return;
        }

        CameraComponent? camera = camEntity.GetComponent<CameraComponent>();
        if (camera == null)
        {
            Log.Error($"[CameraController] FAILED: Entity '{camEntity}' has no CameraComponent!");
            return;
        }

        // One-time setup: make this the primary orbit camera
        if (!camera.Primary)
        {
            Log.Warn($"[CameraController] Setting '{camEntity}' as Primary camera.");
            camera.Primary = true;
        }

        camera.IsOrbitCamera = true;
        camera.TargetEntityTag = TargetTag;
        
        // Initialize camera orbit with reasonable script defaults (Pitch=25, Distance=10)
        camera.SetOrbit(Yaw, Pitch, Distance);

        _localPlayerId = FindLocalPlayerId();
    }

    public override void OnUpdate(float deltaTime)
    {
        if (GameHUD.IsPaused || InGameChat.IsChatOpen)
            return;

        Entity? camEntity = Scene.GetMainCamera();
        if (camEntity == null)
            return;

        // If this script is attached to a non-camera entity (e.g. Player prefab):
        if (Entity.ID != camEntity.ID)
        {
            var netComp = Entity.GetComponent<NetworkIdentityComponent>();
            if (netComp != null && !netComp.IsOwner)
                return;
        }

        CameraComponent? camera = camEntity.GetComponent<CameraComponent>();
        if (camera == null)
            return;

        // Find the player entity to follow.
        // Networked: use the entity we own (IsOwner). Offline: fall back to tag.
        Entity? player = null;

        if (Network.IsConnected)
        {
            _localPlayerId = FindLocalPlayerId();

            if (_localPlayerId != 0)
                player = new Entity(_localPlayerId);
        }

        if (player == null)
            player = Scene.FindEntityByTag(TargetTag);

        if (player == null)
            return;

        camera.GetOrbit(out float yaw, out float pitch, out float distance);

        // Handle Input (Orbit & Zoom)
        if (Input.IsMouseButtonDown(MouseButton.Right))
        {
            Vector3 mouseDelta = Input.MouseDelta;
            yaw -= mouseDelta.X * LookSensitivity;
            pitch -= mouseDelta.Y * LookSensitivity;
        }

        // Always clamp pitch & distance every frame to keep camera in a sane 3rd person view
        pitch = Mathf.Clamp(pitch, -10.0f, 85.0f);

        float wheel = Input.GetMouseWheelMove();
        distance -= wheel * 2.0f;
        distance = Mathf.Clamp(distance, 2.0f, 30.0f);

        camera.SetOrbit(yaw, pitch, distance);
    }
    // public override void OnUpdate(float deltaTime)
    // {
    //     Entity? camEntity = Scene.GetMainCamera();
    //     if (camEntity == null)
    //         return;

    //     CameraComponent? camera = camEntity.GetComponent<CameraComponent>();
    //     if (camera == null)
    //         return;

    //     Entity? player = Scene.FindEntityByTag("Player");
    //     if (player == null)
    //         return;

    //     camera.GetOrbit(out float yaw, out float pitch, out float distance);

    //     // Handle Input (Orbit & Zoom)
    //     if (Input.IsMouseButtonDown(MouseButton.Right))
    //     {
    //         Vector3 mouseDelta = Input.MouseDelta;
    //         yaw -= mouseDelta.X * LookSensitivity;
    //         pitch -= mouseDelta.Y * LookSensitivity;

    //         // Clamp pitch
    //         pitch = Mathf.Clamp(pitch, -10.0f, 85.0f);
    //     }

    //     float wheel = Input.GetMouseWheelMove();
    //     distance -= wheel * 2.0f;
    //     distance = Mathf.Clamp(distance, 0.0f, 40.0f);

    //     camera.SetOrbit(yaw, pitch, distance);
    // }

    private static ulong FindLocalPlayerId()
    {
        var ids = Entity.FindAllWithComponent<NetworkIdentityComponent>();
        foreach (var id in ids)
        {
            var entity = new Entity(id);
            var netId = entity.GetComponent<NetworkIdentityComponent>();
            if (netId != null && netId.IsOwner)
                return id;
        }
        return 0;
    }
}
}
