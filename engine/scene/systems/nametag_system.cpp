#include "nametag_system.h"
#include "engine/core/service_locator.h"
#include "engine/graphics/pipeline/renderer.h"
#include "engine/graphics/pipeline/scene_renderer.h"
#include "engine/graphics/api/graphics_device.h"
#include "engine/graphics/api/vertex_array.h"
#include "engine/graphics/api/buffer.h"
#include "engine/graphics/text_renderer.h"
#include "engine/assets/types/font_asset.h"
#include "engine/assets/asset_manager.h"
#include "engine/networking/network_service.h"
#include "engine/scene/components/gameplay/network_identity_component.h"
#include "engine/scene/components/core/transform_component.h"

namespace Chained::NametagSystem
{
	namespace
	{
		TextRenderer s_TextRenderer;
		std::shared_ptr<VertexArray> s_QuadVAO;
		std::shared_ptr<VertexBuffer> s_DynVBO;
		bool s_Initialized = false;

		constexpr int kMaxGlyphs = 256;
		constexpr int kVertsPerGlyph = 4;
		constexpr int kIndicesPerGlyph = 6;
		constexpr float kNametagScale = 0.007f;

		struct GlyphVertex
		{
			float x, y;
			float u, v;
		};

		void EnsureInitialized()
		{
			if (s_Initialized)
			{
				return;
			}

			s_TextRenderer.Init();

			uint32_t vboSize = kMaxGlyphs * kVertsPerGlyph * static_cast<uint32_t>(sizeof(GlyphVertex));
			s_DynVBO = VertexBuffer::Create(vboSize);
			s_DynVBO->SetLayout(
				{{VertexAttributeType::Float2, "a_Position"}, {VertexAttributeType::Float2, "a_TexCoords"}});

			s_QuadVAO = VertexArray::Create();
			s_QuadVAO->AddVertexBuffer(s_DynVBO);

			std::vector<uint32_t> indices;
			indices.reserve(kMaxGlyphs * kIndicesPerGlyph);
			for (int i = 0; i < kMaxGlyphs; ++i)
			{
				uint32_t base = i * kVertsPerGlyph;
				indices.push_back(base + 0);
				indices.push_back(base + 1);
				indices.push_back(base + 2);
				indices.push_back(base + 2);
				indices.push_back(base + 3);
				indices.push_back(base + 0);
			}
			auto ibo = IndexBuffer::Create(indices.data(), static_cast<uint32_t>(indices.size()));
			s_QuadVAO->SetIndexBuffer(ibo);

			s_Initialized = true;
		}
	} // namespace

	void Init()
	{
		EnsureInitialized();
		SceneRenderer::SetNametagRenderer(&DrawNametags);
	}

	struct AutoRegisterNametagRenderer
	{
		AutoRegisterNametagRenderer()
		{
			SceneRenderer::SetNametagRenderer(&DrawNametags);
		}
	} s_AutoRegisterNametagRenderer;

	void DrawNametags(entt::registry& registry, const Camera3D& camera)
	{
		auto* net = ServiceLocator::TryGet<Network>();
		if (!net || net->GetRole() == Role::Offline)
		{
			return;
		}

		auto view = registry.view<NetworkIdentityComponent, TransformComponent>();
		if (view.begin() == view.end())
		{
			return;
		}

		auto* renderer = ServiceLocator::TryGet<Renderer>();
		if (!renderer)
		{
			return;
		}

		auto* am = ServiceLocator::TryGet<AssetManager>();
		if (!am)
		{
			return;
		}

		EnsureInitialized();

		auto& shaderStorage = renderer->GetShaderLibrary();
		auto shaderAsset = shaderStorage.LoadOrGet("Nametag", "engine/resources/shaders/materials/nametag.chshader");
		if (!shaderAsset || !shaderAsset->IsReady())
		{
			return;
		}
		auto shader = shaderAsset->GetShader();
		if (!shader)
		{
			return;
		}

		auto fontAsset = am->Get<FontAsset>("font/lato/lato-regular.ttf");
		if (!fontAsset || !fontAsset->IsReady())
		{
			fontAsset = am->Get<FontAsset>("engine/resources/font/lato/lato-regular.ttf");
		}
		if (!fontAsset || !fontAsset->IsReady())
		{
			fontAsset = am->Get<FontAsset>("resources/font/lato/lato-regular.ttf");
		}
		if (!fontAsset || !fontAsset->IsReady())
		{
			return;
		}
		const auto& font = fontAsset->GetFont();
		if (!font.textureAtlas)
		{
			return;
		}

		const auto players = net->GetPlayerList();
		uint64_t localID = net->GetLocalNetworkID();

		shader->Bind();
		shader->SetMatrix("projection", camera.ProjectionMatrix);
		shader->SetMatrix("view", camera.ViewMatrix);
		shader->SetFloat("useBackground", 0.0f);
		shader->SetVec4("backgroundColor", glm::vec4(0.0f, 0.0f, 0.0f, 0.7f));
		shader->SetVec2("size", glm::vec2(1.0f, 1.0f));

		GraphicsDevice::Get().SetTexture(0, font.textureAtlas->GetNativeHandle());
		shader->SetInt("textTexture", 0);

		PipelineStateGuard stateGuard;
		stateGuard.WithoutDepthTest().WithBlend().WithCullNone();

		std::vector<GlyphVertex> verts;

		for (auto entity : view)
		{
			if (!registry.valid(entity))
			{
				continue;
			}

			const auto& netId = view.get<NetworkIdentityComponent>(entity);
			if (netId.NetworkID == localID)
			{
				continue;
			}

			const auto& tc = view.get<TransformComponent>(entity);
			if (tc.Scale.x < 0.001f || tc.Translation.y < -500.0f)
			{
				continue;
			}

			std::string name;
			bool isHost = false;

			for (const auto& p : players)
			{
				if (p.NetworkID == netId.NetworkID)
				{
					name = p.Name;
					isHost = (p.IsHost != 0);
					break;
				}
			}
			if (name.empty())
			{
				name = "Player " + std::to_string(netId.NetworkID);
			}

			auto quads = s_TextRenderer.LayoutGlyphs(name, font, 48.0f);
			if (quads.empty())
			{
				continue;
			}

			if (static_cast<int>(quads.size()) > kMaxGlyphs)
			{
				quads.resize(kMaxGlyphs);
			}

			float minX = 1e9f, maxX = -1e9f;
			float minY = 1e9f, maxY = -1e9f;
			for (const auto& q : quads)
			{
				minX = std::min(minX, q.x);
				maxX = std::max(maxX, q.x + q.w);
				minY = std::min(minY, q.y);
				maxY = std::max(maxY, q.y + q.h);
			}

			float centerX = (minX + maxX) * 0.5f;
			float centerY = (minY + maxY) * 0.5f;

			glm::vec3 worldPos = tc.Translation;
			if (tc.WorldTransform != glm::mat4(1.0f) && tc.WorldTransform != glm::mat4(0.0f))
			{
				worldPos = glm::vec3(tc.WorldTransform[3]);
			}

			float distToCamera = glm::length(worldPos - camera.Position);
			float distanceScaleFactor = glm::clamp(distToCamera / 8.0f, 1.0f, 6.0f);
			float currentScale = kNametagScale * distanceScaleFactor;
			float heightOffset = 2.2f + (distanceScaleFactor - 1.0f) * 0.35f;
			glm::vec3 pos = worldPos + glm::vec3(0.0f, heightOffset, 0.0f);

			verts.clear();
			verts.reserve(quads.size() * kVertsPerGlyph);

			for (const auto& q : quads)
			{
				float lx = (q.x - centerX) * currentScale;
				float rx = (q.x + q.w - centerX) * currentScale;
				float topY = -(q.y - centerY) * currentScale;
				float botY = -(q.y + q.h - centerY) * currentScale;

				verts.push_back({lx, topY, q.s0, q.t0});
				verts.push_back({rx, topY, q.s1, q.t0});
				verts.push_back({rx, botY, q.s1, q.t1});
				verts.push_back({lx, botY, q.s0, q.t1});
			}

			if (verts.empty())
			{
				continue;
			}

			s_DynVBO->SetData(verts.data(), static_cast<uint32_t>(verts.size() * sizeof(GlyphVertex)));
			shader->SetVec3("modelPosition", pos);

			bool isLocal = (netId.NetworkID == localID);
			shader->SetVec4("textColor",
							isLocal ? glm::vec4(1.0f, 0.86f, 0.0f, 1.0f)
									: (isHost ? glm::vec4(0.4f, 0.8f, 1.0f, 1.0f) : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)));

			uint32_t idxCount = static_cast<uint32_t>(quads.size() * kIndicesPerGlyph);
			s_QuadVAO->Bind();
			GraphicsDevice::Get().DrawIndexed(s_QuadVAO, idxCount);
			s_QuadVAO->Unbind();
		}

		shader->Unbind();
	}

	void Shutdown()
	{
		if (!s_Initialized)
		{
			return;
		}
		s_TextRenderer.Shutdown();
		s_DynVBO.reset();
		s_QuadVAO.reset();
		s_Initialized = false;
	}

} // namespace Chained::NametagSystem
