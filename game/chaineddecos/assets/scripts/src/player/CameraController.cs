using System;
using Chained;

namespace ChainedDecos.Scripts
{
[AutoAttach("Player")]
public class CameraController : Script
{
    public float LookSensitivity = 0.2f;
    public float Distance = 10.0f;
    public float Pitch = 35.0f;
    public float Yaw = 0.0f;
    public string TargetTag = "Player";

    public float TouchpadSensitivity = 2.0f;
    public float KeyboardRotateSpeed = 90.0f; // degrees per second
    private float _freeFlySpeed = 24.0f;

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

        // ── Spectator / Free Fly Mode ──
        if (SpectatorState.IsFinished)
        {
            UpdateFreeCamera(camera, deltaTime);
            return;
        }

        // ── Normal Orbit Camera ──
        if (camera.TargetEntityTag != TargetTag)
            camera.TargetEntityTag = TargetTag;

        camera.GetOrbit(out float yaw, out float pitch, out float distance);

        bool canProcessInput = !GameHUD.IsPaused && !InGameChat.IsChatOpen;

        if (canProcessInput)
        {
            // ── 1. Mouse & Touchpad Orbit ──
            if (Input.IsMouseButtonDown(MouseButton.Right) || Input.IsMouseButtonDown(MouseButton.Left) || Input.IsMouseButtonDown(MouseButton.Middle))
            {
                Vector3 mouseDelta = Input.MouseDelta;
                yaw -= mouseDelta.X * LookSensitivity;
                pitch -= mouseDelta.Y * LookSensitivity;
            }

            // ── 2. Touchpad Two-Finger Horizontal Swipe ──
            float scrollH = Input.GetMouseWheelHMove();
            if (MathF.Abs(scrollH) > 0.001f)
                yaw -= scrollH * TouchpadSensitivity;

            // ── 3. Mouse Wheel / Touchpad Vertical Zoom ──
            float wheel = Input.GetMouseWheelMove();
            if (MathF.Abs(wheel) > 0.001f)
                distance -= wheel * 1.5f;

            // ── 4. Keyboard Arrow Orbit & Zoom ──
            if (Input.IsKeyDown(Key.Left))  yaw += KeyboardRotateSpeed * deltaTime;
            if (Input.IsKeyDown(Key.Right)) yaw -= KeyboardRotateSpeed * deltaTime;
            if (Input.IsKeyDown(Key.Up))    pitch += KeyboardRotateSpeed * 0.7f * deltaTime;
            if (Input.IsKeyDown(Key.Down))  pitch -= KeyboardRotateSpeed * 0.7f * deltaTime;

            if (Input.IsKeyDown(Key.PageUp))   distance -= 10.0f * deltaTime;
            if (Input.IsKeyDown(Key.PageDown)) distance += 10.0f * deltaTime;
        }

        pitch = Mathf.Clamp(pitch, -20.0f, 80.0f);
        distance = Mathf.Clamp(distance, 2.0f, 25.0f);

        camera.SetOrbit(yaw, pitch, distance);
    }

    private void UpdateFreeCamera(CameraComponent camera, float deltaTime)
    {
        bool canProcessInput = !GameHUD.IsPaused && !InGameChat.IsChatOpen;
        if (!canProcessInput)
        {
            camera.UpdateFreeFly(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, deltaTime);
            return;
        }

        // Mouse look (any mouse button held)
        float deltaYaw = 0.0f;
        float deltaPitch = 0.0f;
        if (Input.IsMouseButtonDown(MouseButton.Right) || Input.IsMouseButtonDown(MouseButton.Left) || Input.IsMouseButtonDown(MouseButton.Middle))
        {
            Vector3 mouseDelta = Input.MouseDelta;
            deltaYaw   -= mouseDelta.X * LookSensitivity;
            deltaPitch -= mouseDelta.Y * LookSensitivity;
        }

        // Touchpad Two-Finger Horizontal Swipe
        float scrollH = Input.GetMouseWheelHMove();
        if (MathF.Abs(scrollH) > 0.001f)
        {
            deltaYaw -= scrollH * TouchpadSensitivity;
        }

        // Arrow key look
        if (Input.IsKeyDown(Key.Left))  deltaYaw += KeyboardRotateSpeed * deltaTime;
        if (Input.IsKeyDown(Key.Right)) deltaYaw -= KeyboardRotateSpeed * deltaTime;
        if (Input.IsKeyDown(Key.Up))    deltaPitch += KeyboardRotateSpeed * 0.7f * deltaTime;
        if (Input.IsKeyDown(Key.Down))  deltaPitch -= KeyboardRotateSpeed * 0.7f * deltaTime;

        // WASD fly movement
        float fwd   = 0.0f;
        float right = 0.0f;
        float up    = 0.0f;

        if (Input.IsKeyDown(Key.W)) fwd += 1.0f;
        if (Input.IsKeyDown(Key.S)) fwd -= 1.0f;
        if (Input.IsKeyDown(Key.D)) right += 1.0f;
        if (Input.IsKeyDown(Key.A)) right -= 1.0f;
        if (Input.IsKeyDown(Key.Space) || Input.IsKeyDown(Key.E)) up += 1.0f;
        if (Input.IsKeyDown(Key.LeftControl) || Input.IsKeyDown(Key.Q) || Input.IsKeyDown(Key.C)) up -= 1.0f;

        // Mouse Wheel speed adjustment
        float wheel = Input.GetMouseWheelMove();
        if (MathF.Abs(wheel) > 0.001f)
        {
            _freeFlySpeed = Mathf.Clamp(_freeFlySpeed + wheel * 4.0f, 4.0f, 120.0f);
        }

        float speed = _freeFlySpeed;
        if (Input.IsKeyDown(Key.LeftShift)) speed *= 2.5f;
        if (Input.IsKeyDown(Key.LeftAlt))   speed *= 0.25f;

        camera.UpdateFreeFly(fwd, right, up, deltaYaw, deltaPitch, speed, deltaTime);
    }
}
}
