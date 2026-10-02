#include "engine/graphics/pipeline/renderer.h"
#include "engine/graphics/pipeline/geometry_generator.h"
#include "engine/graphics/pipeline/shader_uniform_utils.h"
#include "engine/graphics/api/graphics_device.h"
#include "engine/graphics/api/renderer_types.h"
#include "engine/graphics/pipeline/shader_storage.h"
#include "engine/assets/asset_manager.h"

#include "engine/core/service_locator.h"
#include "engine/graphics/api/buffer.h"
#include "engine/graphics/api/storage_buffer.h"
#include "engine/graphics/api/vertex_array.h"
#include "engine/assets/types/environment_asset.h"
#include "engine/assets/types/shader_asset.h"

#include <variant> // Added for type-safe variant visitation

namespace Chained
{

	void Renderer::Initialize()
	{
		if (m_Headless)
		{
			GraphicsDevice::SetAPI(GraphicsDevice::API::None);
			CH_CORE_INFO("[Renderer] Headless mode enabled, skipping OpenGL initialization.");
			return;
		}

		GraphicsDevice::Set(GraphicsDevice::Create());
		GraphicsDevice::Get().Init();

		// Initialize lighting subsystem (SSBO + state)
		m_LightingManager.Initialize();

		// Initialize UBOs (must be before methods that reference them)
		m_Data->CameraUBO = UniformBuffer::Create(sizeof(CameraData), 0);

		// Initialize shared GPU geometry (fullscreen quad, billboard/sprite quad)
		m_GeometryFactory.Initialize();

		InitializeSkybox();
	}

	void Renderer::LoadEngineResources()
	{
		if (m_ResourcesLoaded)
		{
			return;
		}

		auto& shaders = GetShaderLibrary();

		shaders.LoadConfig("engine/resources/config/shaders.yaml");

		// Eager load common shaders if needed, or let them lazy load
		shaders.LoadOrGet("Lighting");
		shaders.LoadOrGet("Skinned");
		shaders.LoadOrGet("Unlit");
		shaders.LoadOrGet("Billboard");

		m_ResourcesLoaded = true;
		CH_CORE_INFO("[Renderer] LoadEngineResources done. {} shader(s) loaded.", shaders.GetNames().size());
	}

	void Renderer::Shutdown()
	{
		CH_CORE_INFO("Shutting down Render System...");

		if (m_Headless)
		{
			return;
		}

		CleanupSkybox();

		m_LightingManager.Shutdown();
		m_GeometryFactory.Shutdown();
		m_Data->Shaders.reset();
		m_Data->CameraUBO.reset();

		m_Data->Instancing.SSBO.reset();
		m_Data->Instancing.Capacity = 0;

		GraphicsDevice::Get().Shutdown();
	}

	Renderer::Renderer()
	{
		m_Data = std::make_unique<RendererData>();
		m_Data->Shaders = std::make_unique<ShaderStorage>();
	}

	Renderer::~Renderer() = default;

	void Renderer::BeginScene(const Camera3D& camera)
	{
		m_LightingManager.Upload();

		// Use precomputed matrices from Camera3D
		m_Data->Frame.CameraPosition = camera.Position;
		m_Data->Frame.View = camera.ViewMatrix;
		m_Data->Frame.Proj = camera.ProjectionMatrix;

		// Upload to UBO
		if (m_Data->CameraUBO)
		{
			CameraData cameraData;
			cameraData.ViewProjection = m_Data->Frame.Proj * m_Data->Frame.View;
			cameraData.Projection = m_Data->Frame.Proj;
			cameraData.View = m_Data->Frame.View;
			m_Data->CameraUBO->SetData(&cameraData, sizeof(CameraData));
			m_Data->CameraUBO->BindBase(0);
		}

		// Track the default lighting shader handle for DrawMesh fallback
		auto lightingShaderAsset = m_Data->Shaders->Exists("Lighting") ? m_Data->Shaders->Get("Lighting") : nullptr;
		if (lightingShaderAsset && lightingShaderAsset->GetShader())
		{
			m_Data->Frame.CurrentShaderId = lightingShaderAsset->GetShader()->GetNativeHandle();
		}
	}

	void Renderer::EndScene()
	{
		m_Data->Frame.CurrentShaderId = 0;
	}

	void Renderer::Clear(const glm::vec4& color)
	{
		Color chColor((unsigned char)(color.r * 255), (unsigned char)(color.g * 255), (unsigned char)(color.b * 255),
					  (unsigned char)(color.a * 255));
		GraphicsDevice::Get().Clear(chColor);
	}

	void Renderer::SetViewport(int x, int y, int width, int height)
	{
		GraphicsDevice::Get().SetViewport(x, y, width, height);
	}

	void Renderer::DrawMesh(const Mesh& mesh, const Material& material, const glm::mat4& transform)
	{
		uint32_t shaderId = material.ShaderID;
		if (shaderId == 0)
		{
			shaderId = m_Data->Frame.CurrentShaderId;
		}
		if (shaderId == 0)
		{
			return;
		}

		auto shaderAsset = GetShaderLibrary().GetById(shaderId);
		if (!shaderAsset)
		{
			return;
		}

		auto shader = shaderAsset->GetShader();
		if (!shader)
		{
			return;
		}

		shader->Bind();

		// Set model matrix
		shader->SetMatrix("matModel", transform);

		float sx = glm::dot(glm::vec3(transform[0]), glm::vec3(transform[0]));
		float sy = glm::dot(glm::vec3(transform[1]), glm::vec3(transform[1]));
		float sz = glm::dot(glm::vec3(transform[2]), glm::vec3(transform[2]));
		if (std::abs(sx - sy) < 0.001f && std::abs(sx - sz) < 0.001f)
		{
			shader->SetMatrix("matNormal", transform);
		}
		else
		{
			glm::mat4 matNormal = glm::transpose(glm::inverse(transform));
			shader->SetMatrix("matNormal", matNormal);
		}

		// Bind VAO and Draw
		if (mesh.VAO)
		{
			mesh.VAO->Bind();
			if (mesh.TriangleCount > 0 && mesh.VAO->GetIndexBuffer())
			{
				GraphicsDevice::Get().DrawIndexed(mesh.VAO, mesh.TriangleCount * 3);
			}
			else
			{
				GraphicsDevice::Get().DrawArrays(mesh.VertexCount);
			}
		}
	}

	void Renderer::DrawMeshInstanced(const Mesh& mesh, const Material& material,
									 const std::vector<glm::mat4>& transforms)
	{
		if (transforms.empty() || !mesh.VAO)
		{
			return;
		}

		if (transforms.size() == 1)
		{
			DrawMesh(mesh, material, transforms[0]);
			return;
		}

		uint32_t shaderId = material.ShaderID;
		if (shaderId == 0)
		{
			shaderId = m_Data->Frame.CurrentShaderId;
		}
		if (shaderId == 0)
		{
			return;
		}

		auto shaderAsset = GetShaderLibrary().GetById(shaderId);
		if (!shaderAsset)
		{
			return;
		}

		auto shader = shaderAsset->GetShader();
		if (!shader)
		{
			return;
		}

		shader->Bind();

		// Upload instance transforms to SSBO slot 2
		uint32_t dataSize = (uint32_t)(transforms.size() * sizeof(glm::mat4));
		if (!m_Data->Instancing.SSBO || m_Data->Instancing.Capacity < dataSize)
		{
			m_Data->Instancing.Capacity = std::max(dataSize, (uint32_t)(4096 * sizeof(glm::mat4)));
			m_Data->Instancing.SSBO = StorageBuffer::Create(m_Data->Instancing.Capacity);
		}
		m_Data->Instancing.SSBO->SetData(transforms.data(), dataSize);
		m_Data->Instancing.SSBO->BindBase(2);

		shader->SetInt("u_IsInstanced", 1);

		// Bind mesh VAO and draw instanced
		mesh.VAO->Bind();
		if (mesh.TriangleCount > 0 && mesh.VAO->GetIndexBuffer())
		{
			GraphicsDevice::Get().DrawIndexedInstanced(mesh.VAO, (uint32_t)transforms.size(), mesh.TriangleCount * 3);
		}
		else
		{
			GraphicsDevice::Get().DrawArraysInstanced(mesh.VertexCount, (uint32_t)transforms.size());
		}

		shader->SetInt("u_IsInstanced", 0);
	}

	void Renderer::DrawSkybox(uint32_t textureId, int skyboxMode, bool isHDR, float exposure, float brightness,
							  float contrast, const Camera3D& camera, bool flipped)
	{
		if (textureId == 0)
		{
			return;
		}

		skyboxMode = std::clamp(skyboxMode, 0, 2);

		auto shaderAsset = (skyboxMode == 2) ? m_Data->Shaders->LoadOrGet("SkyboxCubemap")
											 : (skyboxMode == 1 ? m_Data->Shaders->LoadOrGet("SkyboxCross")
																: m_Data->Shaders->LoadOrGet("Skybox"));
		if (!shaderAsset || !shaderAsset->GetShader())
		{
			return;
		}

		// 1. Prepare Render State
		GraphicsDevice::Get().SetDepthFunc(GraphicsDevice::DepthFunc::LEqual);
		GraphicsDevice::Get().SetCullMode(GraphicsDevice::CullMode::None);
		GraphicsDevice::Get().DisableDepthMask();

		// 2. Setup Uniforms
		shaderAsset->GetShader()->Bind();

		// Always remove translation from view matrix for skybox
		glm::mat4 view = glm::mat4(glm::mat3(m_Data->Frame.View));
		shaderAsset->GetShader()->SetMatrix("u_View", view);
		shaderAsset->GetShader()->SetMatrix("u_Projection", m_Data->Frame.Proj);

		shaderAsset->GetShader()->SetFloat("u_Exposure", exposure);
		shaderAsset->GetShader()->SetFloat("u_Brightness", brightness);
		shaderAsset->GetShader()->SetFloat("u_Contrast", contrast);
		shaderAsset->GetShader()->SetInt("u_IsHDR", isHDR ? 1 : 0);
		shaderAsset->GetShader()->SetInt("u_VFlipped", flipped ? 1 : 0);

		// 3. Bind Textures and Draw Mesh
		const char* texUniform = "u_Panorama";
		uint32_t texFlags = 0; // 0 = 2D, true = cubemap
		if (skyboxMode == 2)
		{
			texUniform = "u_Cubemap";
			texFlags = true;
		}
		else if (skyboxMode == 1)
		{
			texUniform = "u_CrossMap";
		}

		GraphicsDevice::Get().SetTexture(0, textureId, texFlags != 0);
		shaderAsset->GetShader()->SetInt(texUniform, 0);

		auto& model = (skyboxMode == 0) ? m_Data->Skybox.SkyboxSphereModel : m_Data->Skybox.SkyboxCubeModel;
		if (model && !model->Meshes.empty())
		{
			auto& mesh = model->Meshes[0];
			mesh.VAO->Bind();
			uint32_t indexCount = (skyboxMode == 0) ? mesh.TriangleCount * 3 : 36;
			GraphicsDevice::Get().DrawIndexed(mesh.VAO, indexCount);
			mesh.VAO->Unbind();
		}

		// 4. Restore Render State
		GraphicsDevice::Get().SetDepthFunc(GraphicsDevice::DepthFunc::LEqual);
		GraphicsDevice::Get().SetCullMode(GraphicsDevice::CullMode::Back);
		GraphicsDevice::Get().EnableDepthMask();
	}

	void Renderer::DrawBillboard(const Camera3D& camera, uint32_t textureId, const glm::vec3& position, float size,
								 const glm::vec4& tint)
	{
		auto billboardShaderAsset = m_Data->Shaders->LoadOrGet("Billboard");
		if (!billboardShaderAsset || !billboardShaderAsset->GetShader() || textureId == 0)
		{
			return;
		}

		auto shader = billboardShaderAsset->GetShader();
		shader->Bind();

		glm::vec3 look = glm::normalize(camera.Position - position);
		glm::vec3 right = glm::cross(camera.Up, look);
		if (glm::length(right) < 0.0001f)
		{
			right = glm::vec3(1.0f, 0.0f, 0.0f);
		}
		else
		{
			right = glm::normalize(right);
		}
		glm::vec3 up = glm::normalize(glm::cross(look, right));

		glm::mat4 model = glm::mat4(1.0f);
		model[0] = glm::vec4(right * size, 0.0f);
		model[1] = glm::vec4(up * size, 0.0f);
		model[2] = glm::vec4(look * size, 0.0f);
		model[3] = glm::vec4(position, 1.0f);

		shader->SetMatrix("mvp", m_Data->Frame.Proj * m_Data->Frame.View * model);
		shader->SetVec4("colDiffuse", tint);

		GraphicsDevice::Get().SetTexture(0, textureId);
		shader->SetInt("texture0", 0);

		PipelineStateGuard stateGuard;
		stateGuard.WithBlend().WithCullNone();

		auto& quadVAO = m_GeometryFactory.GetQuad();
		quadVAO->Bind();
		GraphicsDevice::Get().DrawIndexed(quadVAO, 6);
		quadVAO->Unbind();
	}

	void Renderer::ApplyPostProcessing(uint32_t screenTextureId, uint32_t depthTextureId, const Camera3D& camera,
									   ShaderAsset* overrideShader, const std::vector<ShaderUniform>& uniforms)
	{
		std::shared_ptr<ShaderAsset> shaderAsset = nullptr;

		if (overrideShader)
		{
			if (auto* am = ServiceLocator::TryGet<AssetManager>())
			{
				auto handle = am->ResolveToHandle(overrideShader->GetPath());
				shaderAsset = am->Get<ShaderAsset>(handle);
			}
		}
		else
		{
			shaderAsset = m_Data->Shaders->LoadOrGet("PostProcess");
		}

		if (shaderAsset && shaderAsset->GetShader())
		{
			auto shader = shaderAsset->GetShader();
			shader->Bind();

			// Safe extraction of diagnostic float values from variant
			float diagIntensity = 0.0f;
			for (const auto& u : uniforms)
			{
				if (u.Name == "uIntensity")
				{
					if (auto* fVal = std::get_if<float>(&u.Value))
					{
						diagIntensity = *fVal;
					}
				}
			}
			if (diagIntensity > 0.001f)
			{
				CH_CORE_TRACE("[RENDER DIAG] Applying shader '{}', Intensity={}", shaderAsset->GetPath(),
							  diagIntensity);
			}

			// 1. Set System Uniforms
			glm::mat4 identity = glm::mat4(1.0f);
			shader->SetMatrix("mvp", identity);

			glm::mat4 invViewProj = glm::inverse(m_Data->Frame.Proj * m_Data->Frame.View);
			shader->SetMatrix("matInverseViewProj", invViewProj);
			shader->SetVec3("viewPos", camera.Position);

			float currentSeconds = (float)m_Data->Frame.Time.GetSeconds();
			shader->SetFloat("uTimeF", currentSeconds);
			shader->SetFloat("uTime", currentSeconds);
			shader->SetFloat("time", currentSeconds);
			shader->SetFloat("uExposure", m_LightingManager.GetLighting().CurrentLighting.Exposure);
			shader->SetFloat("uGamma", m_LightingManager.GetLighting().CurrentLighting.Gamma);

			// 2. Set Custom Uniforms using type-safe std::visit
			ApplyShaderUniforms(shader.get(), uniforms);

			// 3. Bind Textures
			GraphicsDevice::Get().SetTexture(0, screenTextureId);
			shader->SetInt("texture0", 0);

			GraphicsDevice::Get().SetTexture(1, depthTextureId);
			shader->SetInt("texture1", 1);

			GraphicsDevice::Get().DisableDepthTest();

			auto& fsQuad = m_GeometryFactory.GetFullscreenQuad();
			fsQuad->Bind();
			GraphicsDevice::Get().DrawIndexed(fsQuad, 6);
			fsQuad->Unbind();

			GraphicsDevice::Get().EnableDepthTest();
			GraphicsDevice::Get().SetCullMode(GraphicsDevice::CullMode::Back);
		}
	}

	void Renderer::Update(Timestep ts)
	{
		m_Data->Frame.Time = ts;
	}

	ShaderAsset* Renderer::BindShader(const std::string& name)
	{
		auto shaderAsset = m_Data->Shaders->LoadOrGet(name);
		if (!shaderAsset || !shaderAsset->GetShader())
		{
			return nullptr;
		}
		shaderAsset->GetShader()->Bind();
		return shaderAsset.get();
	}

	void Renderer::InitializeSkybox()
	{
		m_Data->Skybox.SkyboxCubeModel = std::make_unique<Model>();
		m_Data->Skybox.SkyboxCubeModel->Meshes.push_back(GeometryGenerator::GenerateUnitCube());

		m_Data->Skybox.SkyboxSphereModel = std::make_unique<Model>();
		m_Data->Skybox.SkyboxSphereModel->Meshes.push_back(GeometryGenerator::GenerateSphere(50.0f, 64, 64));
	}

	void Renderer::CleanupSkybox()
	{
		m_Data->Skybox.SkyboxCubeModel.reset();
		m_Data->Skybox.SkyboxSphereModel.reset();
		m_Data->Skybox.CachedCubemap.reset();
	}

	void Renderer::DrawSprite(uint32_t textureId, const glm::mat4& transform, const glm::vec4& tint, bool flipX,
							  bool flipY)
	{
		if (textureId == 0)
		{
			return;
		}

		auto shaderAsset = m_Data->Shaders->LoadOrGet("Sprite");
		if (!shaderAsset || !shaderAsset->GetShader())
		{
			return;
		}

		auto shader = shaderAsset->GetShader();
		shader->Bind();

		shader->SetMatrix("mvp", m_Data->Frame.Proj * m_Data->Frame.View * transform);
		shader->SetMatrix("matModel", transform);
		shader->SetVec4("u_Tint", tint);
		shader->SetVec2("u_Flip", glm::vec2(flipX ? 1.0f : 0.0f, flipY ? 1.0f : 0.0f));

		PipelineStateGuard stateGuard;
		stateGuard.WithBlend().WithCullNone();

		auto& quadVAO = m_GeometryFactory.GetQuad();
		quadVAO->Bind();
		GraphicsDevice::Get().DrawIndexed(quadVAO, 6);
		quadVAO->Unbind();
	}

	// --- Frame methods (formerly in FrameManager) ---

	void Renderer::SetDiagnosticMode(float mode)
	{
		m_Data->Frame.DiagnosticMode = mode;
	}

	void Renderer::UpdateTime(Timestep time)
	{
		m_Data->Frame.Time = time;
	}

} // namespace Chained