using System;
using Chained;

namespace ChainedDecos.Scripts
{
public class CameraController : Script
{
    public float LookSensitivity = 0.2f;
    public float Distance = 10.0f;
    public float Pitch = 35.0f;
    public float Yaw = 0.0f;
    public string TargetTag = "Player";

    public float TouchpadSensitivity = 2.0f;
    public float KeyboardRotateSpeed = 90.0f; // degrees per second

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
        
        // Initialize camera orbit with reasonable script defaults
        camera.SetOrbit(Yaw, Pitch, Distance);

        _localPlayerId = FindLocalPlayerId();
    }

    public override void OnUpdate(float deltaTime)
    {
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

        bool canProcessInput = !GameHUD.IsPaused && !InGameChat.IsChatOpen;

        if (canProcessInput)
        {
            // ── 1. Mouse & Touchpad Orbit ──
            // Hold Right Mouse Button OR Left Mouse Button OR Middle Mouse Button to rotate camera view
            if (Input.IsMouseButtonDown(MouseButton.Right) || Input.IsMouseButtonDown(MouseButton.Left) || Input.IsMouseButtonDown(MouseButton.Middle))
            {
                Vector3 mouseDelta = Input.MouseDelta;
                yaw -= mouseDelta.X * LookSensitivity;
                pitch -= mouseDelta.Y * LookSensitivity;
            }

            // ── 2. Touchpad Two-Finger Horizontal Swipe (Pan/Orbit) ──
            float scrollH = Input.GetMouseWheelHMove();
            if (MathF.Abs(scrollH) > 0.001f)
            {
                yaw -= scrollH * TouchpadSensitivity;
            }

            // ── 3. Mouse Wheel / Touchpad Vertical Zoom ──
            float wheel = Input.GetMouseWheelMove();
            if (MathF.Abs(wheel) > 0.001f)
            {
                distance -= wheel * 1.5f;
            }

            // ── 4. Keyboard Controls (Arrow keys & Zoom) ──
            float keyYaw = 0.0f;
            if (Input.IsKeyDown(Key.Left))  keyYaw += 1.0f;
            if (Input.IsKeyDown(Key.Right)) keyYaw -= 1.0f;

            float keyPitch = 0.0f;
            if (Input.IsKeyDown(Key.Up))   keyPitch += 1.0f;
            if (Input.IsKeyDown(Key.Down)) keyPitch -= 1.0f;

            if (keyYaw != 0.0f || keyPitch != 0.0f)
            {
                yaw += keyYaw * KeyboardRotateSpeed * deltaTime;
                pitch += keyPitch * (KeyboardRotateSpeed * 0.7f) * deltaTime;
            }

            // Keyboard zoom (PageUp / PageDown)
            if (Input.IsKeyDown(Key.PageUp))
            {
                distance -= 10.0f * deltaTime;
            }
            if (Input.IsKeyDown(Key.PageDown))
            {
                distance += 10.0f * deltaTime;
            }
        }

        // Always clamp pitch & distance every frame to keep camera in a sane 3rd person view
        pitch = Mathf.Clamp(pitch, -20.0f, 80.0f);
        distance = Mathf.Clamp(distance, 2.0f, 25.0f);

        camera.SetOrbit(yaw, pitch, distance);
    }

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
