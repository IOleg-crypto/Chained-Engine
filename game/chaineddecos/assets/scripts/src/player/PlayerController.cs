using System;
using Chained;

namespace ChainedDecos.Scripts
{
    public class PlayerController : Script
    {
        private float MovementSpeed => Entity.GetComponent<PlayerComponent>()?.MovementSpeed ?? 15.0f;
        private float JumpForce    => Entity.GetComponent<PlayerComponent>()?.JumpForce ?? 15.0f;
        public  float Gravity;  
        public  string MenuScene   = "scenes/start_menu.chscene";

        public int IdleAnim = 0;
        public int RunAnim = 1;
        public int JumpAnim = 2;
        public float CrossFadeTime = 8.5f;

        private bool m_WasConnected = false;
        private int m_DisconnectGraceFrames = 0;
        private const int DisconnectGraceLimit = 15;
        private int m_NetIdWaitFrames = 0;
        private const int NetIdGraceFrames = 60;

        private RigidBodyComponent? _rb;
        private TransformComponent? _transform;
        private AnimationComponent? _anim;

        public override void OnCreate()
        {
            Gravity = Physics.GetGravity();
            _rb        = Entity.GetComponent<RigidBodyComponent>();
            _transform = Entity.GetComponent<TransformComponent>();
            _anim      = Entity.GetComponent<AnimationComponent>();
            m_WasConnected = false;
            m_DisconnectGraceFrames = 0;
        }

        public override void OnUpdate(float deltaTime)
        {
            if (GameHUD.IsPaused || InGameChat.IsChatOpen)
            {
                StopHorizontalMovement();
                return;
            }

            if (m_WasConnected && !Network.IsConnected)
            {
                m_DisconnectGraceFrames++;
                if (m_DisconnectGraceFrames > DisconnectGraceLimit)
                {
                    Log.Info("[PlayerController] Host disconnected — returning to menu.");
                    Scene.LoadScene(MenuScene);
                    return;
                }
            }
            else if (Network.IsConnected)
            {
                m_WasConnected = true;
                m_DisconnectGraceFrames = 0;
            }

            _rb        ??= Entity.GetComponent<RigidBodyComponent>();
            _transform ??= Entity.GetComponent<TransformComponent>();
            _anim      ??= Entity.GetComponent<AnimationComponent>();

            if (!Network.IsConnected && SessionState.SavedPlayerPosition.HasValue && _transform != null)
            {
                string currentScene = Scene.GetCurrentScenePath();
                if (string.Equals(currentScene, SessionState.LastGameplayScene, StringComparison.OrdinalIgnoreCase))
                {
                    _transform.Translation = SessionState.SavedPlayerPosition.Value;
                    if (SessionState.SavedPlayerRotation.HasValue)
                    {
                        _transform.Rotation = SessionState.SavedPlayerRotation.Value;
                    }
                    if (_rb != null)
                    {
                        _rb.ForceSetVelocity(Vector3.Zero);
                    }
                }
                SessionState.SavedPlayerPosition = null;
                SessionState.SavedPlayerRotation = null;
            }

            if (_rb == null || _transform == null) return;

            // ── Network awareness ──
            var netId = Entity.GetComponent<NetworkIdentityComponent>();

            // Scene-authored Player in a networked session: skip entirely.
            // NetworkIdentityComponent is added by C++ *after* the first OnUpdate, so we
            // give it up to NetIdGraceFrames frames to appear before enforcing this check.
            if (Network.IsConnected && netId == null)
            {
                m_NetIdWaitFrames++;
                if (m_NetIdWaitFrames > NetIdGraceFrames)
                {
                    return; // confirmed scene-authored entity with no network role
                }
                // Still waiting for C++ to attach NetworkIdentityComponent — continue as owner
            }

            // Non-owner avatars (host-driven): skip local input entirely.
            // The host simulates physics for them; InterpolateEntities handles position.
            if (Network.IsConnected && netId != null && !netId.IsOwner)
            {
                // Not our avatar — just update animation from replicated velocity
                if (_anim != null)
                {
                    bool isMoving = _rb.Velocity.LengthSquared() > 0.05f || (netId.RemoteActionFlags & 0x01) != 0;
                    bool isSprinting = (netId.RemoteActionFlags & 0x02) != 0;
                    _anim.IsPlaying = true;
                    _anim.SetBool("isMoving", isMoving);
                    _anim.SetBool("isGrounded", _rb.IsGrounded);
                    _anim.SetFloat("speed", isMoving ? (isSprinting ? 1.0f : 0.5f) : 0.0f);
                }
                return;
            }

            // ── Local input (owner or offline) ──
            float speed = MovementSpeed;
            if (Input.IsKeyDown(Key.LeftShift)) speed *= 2.0f;

            // Camera-relative basis
            var (forward, right) = GetCameraBasis();

            // Input
            var movementDir = GetMovementInput(forward, right);

            // Movement & rotation
            if (movementDir.LengthSquared() > 0.0001f)
            {
                movementDir = Vector3.Normalize(movementDir);
                ApplyHorizontalMovement(movementDir, speed);
                RotateTowardsMovement(movementDir);
            }
            else
            {
                StopHorizontalMovement();
            }

            HandleVerticalMovement(deltaTime);

            // Animation
            if (_anim != null) UpdateAnimation(movementDir);
        }

        private (Vector3 forward, Vector3 right) GetCameraBasis()
        {
            var forward = new Vector3(0.0f, 0.0f, -1.0f);
            var right   = new Vector3(1.0f, 0.0f, 0.0f);

            var camEntity = Scene.GetMainCamera();
            if (camEntity != null)
            {
                var camera = camEntity.GetComponent<CameraComponent>();
                if (camera != null)
                {
                    var camFwd = new Vector3(camera.Forward.X, 0.0f, camera.Forward.Z);
                    if (camFwd.LengthSquared() > 0.0001f)
                    {
                        forward = Vector3.Normalize(camFwd);
                        right = Vector3.Normalize(new Vector3(-forward.Z, 0.0f, forward.X));
                    }
                }
            }
            return (forward, right);
        }

        private Vector3 GetMovementInput(Vector3 forward, Vector3 right)
        {
            var dir = Vector3.Zero;
            if (Input.IsKeyDown(Key.W)) dir += forward;
            if (Input.IsKeyDown(Key.S)) dir -= forward;
            if (Input.IsKeyDown(Key.A)) dir -= right;
            if (Input.IsKeyDown(Key.D)) dir += right;
            return dir;
        }

        private void ApplyHorizontalMovement(Vector3 dir, float speed)
        {
            if (_rb == null) return;
            _rb.Velocity = new Vector3(
                dir.X * speed,
                _rb.Velocity.Y,
                dir.Z * speed
            );
        }

        private void StopHorizontalMovement()
        {
            if (_rb == null) return;
            _rb.Velocity = new Vector3(0, _rb.Velocity.Y, 0);
        }

        private void RotateTowardsMovement(Vector3 dir)
        {
            if (_transform == null) return;
            float yaw = Mathf.Atan2(dir.X, dir.Z);
            _transform.Rotation = new Vector3(0, yaw, 0);
        }

        private void HandleVerticalMovement(float deltaTime)
        {
            if (_rb == null || _transform == null) return;

            if (Input.IsKeyPressed(Key.Space) && _rb.IsGrounded)
            {
                _rb.Velocity = new Vector3(_rb.Velocity.X, JumpForce, _rb.Velocity.Z);
            }

            if (_rb.IsKinematic)
            {
                const float terminalVelocity = -50.0f;
                if (!_rb.IsGrounded) _rb.Velocity = new Vector3(_rb.Velocity.X, _rb.Velocity.Y - Gravity * deltaTime, _rb.Velocity.Z);
                if (_rb.Velocity.Y < terminalVelocity) _rb.Velocity = new Vector3(_rb.Velocity.X, terminalVelocity, _rb.Velocity.Z);
                if (_rb.IsGrounded && _rb.Velocity.Y < 0) _rb.Velocity = new Vector3(_rb.Velocity.X, 0, _rb.Velocity.Z);
                _transform.Translation += _rb.Velocity * deltaTime;
            }
        }

        private void UpdateAnimation(Vector3 movementDir)
        {
            if (_anim == null) return;
            bool isMoving = movementDir.LengthSquared() > 0.0001f;
            bool isSprinting = Input.IsKeyDown(Key.LeftShift);
            _anim.IsPlaying = true;
            _anim.SetFloat("speed", isMoving ? (isSprinting ? 1.0f : 0.5f) : 0.0f);
            _anim.SetBool("isMoving", isMoving);
            _anim.SetBool("isGrounded", _rb?.IsGrounded ?? true);
        }
    }
}