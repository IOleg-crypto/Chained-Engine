#include "engine/graphics/pipeline/lighting_manager.h"
#include "engine/graphics/api/graphics_device.h"
#include "engine/graphics/api/shader.h"
#include "engine/graphics/api/storage_buffer.h"

namespace Chained
{

	void LightingManager::Initialize()
	{
		m_Lighting.LightSSBO = StorageBuffer::Create(sizeof(RenderLight) * LightingData::MaxLights);
		m_Lighting.LightsDirty = true;
	}

	void LightingManager::Shutdown()
	{
		m_Lighting.LightSSBO.reset();
	}

	void LightingManager::Clear()
	{
		for (int i = 0; i < LightingData::MaxLights; i++)
		{
			m_Lighting.Lights[i].enabled = 0;
		}
		m_Lighting.LightCount = 0;
		m_Lighting.LightsDirty = true;
	}

	void LightingManager::SetLight(int index, const RenderLight& light)
	{
		if (index >= 0 && index < LightingData::MaxLights)
		{
			m_Lighting.Lights[index] = light;
			m_Lighting.LightsDirty = true;
		}
	}

	void LightingManager::SetLightCount(int count)
	{
		m_Lighting.LightCount = count;
	}

	void LightingManager::ApplyEnvironment(const EnvironmentSettings& settings)
	{
		m_Lighting.CurrentLighting = settings.Lighting;
		m_Lighting.CurrentFog = settings.Fog;
	}

	void LightingManager::SetMainLight(const LightingSettings& settings)
	{
		m_Lighting.CurrentLighting = settings;
	}

	void LightingManager::SetShadowState(bool enabled, uint32_t mapTextureID, const glm::mat4& lightSpaceMatrix,
										 float bias)
	{
		m_Shadow.Enabled = enabled;
		m_Shadow.MapTextureID = mapTextureID;
		m_Shadow.LightSpaceMatrix = lightSpaceMatrix;
		m_Shadow.Bias = bias;
	}

	void LightingManager::Upload()
	{
		if (m_Lighting.LightsDirty && m_Lighting.LightSSBO)
		{
			m_Lighting.LightSSBO->SetData(m_Lighting.Lights, sizeof(RenderLight) * LightingData::MaxLights);
			m_Lighting.LightsDirty = false;
		}
	}

	void LightingManager::ApplyUniforms(Shader* shader, const FrameState& frame)
	{
		if (!shader)
		{
			return;
		}

		shader->Bind();

		// ── Scene Lighting ──────────────────────────────────────────────────────
		const auto& lighting = m_Lighting.CurrentLighting;

		glm::vec4 lightColor = {lighting.LightColor.r / 255.0f, lighting.LightColor.g / 255.0f,
								lighting.LightColor.b / 255.0f, lighting.LightColor.a / 255.0f};
		glm::vec4 skyColor = lightColor;
		skyColor.w = lighting.Ambient * 0.35f;

		glm::vec3 lightDirNorm = glm::length(lighting.Direction) > 0.0001f ? glm::normalize(lighting.Direction)
																		   : glm::vec3(0.0f, -1.0f, 0.0f);

		// Camera / frame state
		shader->SetVec3("viewPos", frame.CameraPosition);
		shader->SetFloat("uTime", frame.Time);
		shader->SetFloat("uMode", frame.DiagnosticMode);
		shader->SetFloat("uExposure", lighting.Exposure);
		shader->SetFloat("uGamma", lighting.Gamma);

		// Legacy flat uniforms (kept for older shaders and unlit/sprite/postfx paths)
		shader->SetVec3("lightDir", lightDirNorm);
		shader->SetVec4("lightColor", lightColor);
		shader->SetFloat("ambient", lighting.Ambient);
		shader->SetVec4("skyAmbientColor", skyColor);
		shader->SetInt("uLightCount", m_Lighting.LightCount);

		// Struct-style uniform for the generic directional light
		// (used by the new lighting.glsl / lighting.frag).
		// Note: u_MainLight represents the scene's primary directional light source
		// (could be sun, moon, or any global directional light set per scene).
		shader->SetVec3("u_MainLight.direction", lightDirNorm);
		shader->SetVec4("u_MainLight.color", lightColor);
		shader->SetFloat("u_MainLight.intensity", 1.0f);
		shader->SetFloat("u_MainLight.ambient", lighting.Ambient);

		// ── SSBO ────────────────────────────────────────────────────────────────
		if (m_Lighting.LightSSBO)
		{
			m_Lighting.LightSSBO->BindBase(0);
		}

		// ── Shadows ─────────────────────────────────────────────────────────────
		shader->SetInt("u_ShadowsEnabled", m_Shadow.Enabled ? 1 : 0);
		shader->SetMatrix("u_LightSpaceMatrix", m_Shadow.LightSpaceMatrix);
		shader->SetFloat("u_ShadowBias", m_Shadow.Bias);
		if (m_Shadow.Enabled && m_Shadow.MapTextureID > 0)
		{
			GraphicsDevice::Get().SetTexture(6, m_Shadow.MapTextureID);
			shader->SetInt("u_ShadowMap", 6);
		}

		// ── Fog ─────────────────────────────────────────────────────────────────
		const auto& fog = m_Lighting.CurrentFog;
		glm::vec4 fogColorVec = {fog.FogColor.r / 255.0f, fog.FogColor.g / 255.0f, fog.FogColor.b / 255.0f,
								 fog.FogColor.a / 255.0f};

		shader->SetInt("fogEnabled", fog.Enabled ? 1 : 0);
		shader->SetVec4("fogColor", fogColorVec);
		shader->SetFloat("fogDensity", fog.Density);
		shader->SetFloat("fogStart", fog.Start);
		shader->SetFloat("fogEnd", fog.End);
		shader->SetInt("fogMode", (int)fog.Mode);
		shader->SetFloat("fogHeightFalloff", fog.HeightFalloff);
	}

} // namespace Chained
