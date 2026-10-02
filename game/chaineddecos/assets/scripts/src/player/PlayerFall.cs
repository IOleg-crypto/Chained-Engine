//========= Copyright Chained Decos, All rights reserved. ============//
using System;
using Chained;

namespace ChainedDecos.Scripts
{
public class PlayerFall : Script
{
    private Entity? m_Camera;
    private AudioComponent? m_Audio;
    private float m_Intensity = 0.0f;
    private float m_LogTimer = 0.0f;

    // Constants for the "natural" feel
    private const float LIGHT_FALL_THRESHOLD = 5.0f;  // Wind feeling starts here
    private const float HEAVY_FALL_THRESHOLD = 15.0f; // Turbulence starts here
    private const float MAX_SHAKE_SPEED = 40.0f;      // Max intensity reached here

    public override void OnCreate()
    {
        Log.Info("[PlayerFall] OnCreate: Initializing natural fall effect.");
        m_Intensity = 0.0f;

        // Reset shaders state
        ulong[]? shaderEntities = Entity.FindAllWithComponent<ShaderComponent>();
        if (shaderEntities != null)
        {
            foreach (ulong id in shaderEntities)
            {
                Entity e = new Entity(id);
                var shader = e.GetComponent<ShaderComponent>();
                if (shader != null)
                {
                    shader.Enabled = false;
                    shader.SetFloat("uIntensity", 0.0f);
                }
            }
        }

        m_Camera = Scene.GetMainCamera();
        m_Audio = Entity.GetComponent<AudioComponent>();
        if (m_Audio != null)
        {
            m_Audio.Loop = true;
            m_Audio.Volume = 0.0f;
        }
    }

    public override void OnUpdate(float deltaTime)
    {
        var netComp = Entity.GetComponent<NetworkIdentityComponent>();
        if (netComp != null && !netComp.IsOwner) return;

        RigidBodyComponent? rb = Entity.GetComponent<RigidBodyComponent>();
        if (rb == null) return;

        float fallSpeed = -rb.Velocity.Y;
        bool isGrounded = rb.IsGrounded;

        float targetIntensity = 0.0f;
        
        if (!isGrounded && fallSpeed > 1.5f) // Slightly lower start threshold
        {
            // 1. Progressive light wind (curved start)
            // Starts very small and grows to 0.15
            float windT = Mathf.Clamp((fallSpeed - 1.5f) / 12.0f, 0.0f, 1.0f);
            float lightWind = (windT * windT) * 0.15f; // Quadratic ease-in for smoother start
            
            // 2. Heavy turbulence (only above heavy threshold)
            float heavyTurbulence = 0.0f;
            if (fallSpeed > HEAVY_FALL_THRESHOLD)
            {
                float turbT = Mathf.Clamp((fallSpeed - HEAVY_FALL_THRESHOLD) / (MAX_SHAKE_SPEED - HEAVY_FALL_THRESHOLD), 0.0f, 1.0f);
                heavyTurbulence = turbT * 0.85f;
            }
            
            targetIntensity = lightWind + heavyTurbulence;
        }

        // --- ASYMMETRIC SMOOTHING ---
        float lerpSpeed;
        if (isGrounded)
        {
            lerpSpeed = 12.0f; // Fast stop on ground
        }
        else if (targetIntensity > m_Intensity)
        {
            lerpSpeed = 1.5f;  // Very gradual build-up in air
        }
        else
        {
            lerpSpeed = 3.0f;  // Smooth fade-out in air
        }

        m_Intensity = Mathf.Lerp(m_Intensity, targetIntensity, deltaTime * lerpSpeed);
        
        // Snap to zero when very low
        if (m_Intensity < 0.001f) m_Intensity = 0.0f;

        if (m_Camera == null) m_Camera = Scene.GetMainCamera();
        if (m_Camera != null)
        {
            var shader = m_Camera.GetComponent<ShaderComponent>();
            if (shader != null)
            {
                bool shouldEnable = m_Intensity > 0.001f;
                if (shader.Enabled != shouldEnable)
                {
                    shader.Enabled = shouldEnable;
                }

                if (shouldEnable)
                {
                    shader.SetFloat("uIntensity", m_Intensity);
                    
                    m_LogTimer += deltaTime;
                    if (m_LogTimer > 1.0f)
                    {
                        Log.Info($"[PlayerFall] State: {(isGrounded ? "Grounded" : "Airborne")}, Speed={fallSpeed:F1}, Intensity={m_Intensity:F2}");
                        m_LogTimer = 0.0f;
                    }
                }
            }
        }

        // --- AUDIO MODULATION ---
        if (m_Audio != null)
        {
            bool shouldPlay = m_Intensity > 0.01f;
            
            if (shouldPlay)
            {
                // Start sound once — guard in C++ prevents duplicates
                if (!m_Audio.IsPlaying) m_Audio.Play();
                
                // Dynamically update volume on the active instance (not creating a new one)
                m_Audio.Volume = Mathf.Clamp(m_Intensity * 2.5f, 0.0f, 1.0f);
            }
            else if (m_Audio.IsPlaying)
            {
                // Fully stop the sound when intensity drops to zero
                m_Audio.Volume = 0.0f;
                m_Audio.Stop();
            }
        }
    }
}
}
