#include "nametag_system.h"
#include "engine/core/service_locator.h"
#include "engine/graphics/pipeline/renderer.h"
#include "engine/graphics/pipeline/renderer_data.h"
#include "engine/graphics/api/graphics_device.h"
#include "engine/graphics/api/vertex_array.h"
#include "engine/graphics/api/buffer.h"
#include "engine/graphics/text_renderer.h"
#include "engine/assets/types/font_asset.h"
#include "engine/assets/asset_manager.h"
#include "engine/networking/network_service.h"
#include "engine/scene/components/gameplay/network_identity_component.h"
#include "engine/scene/components/core/transform_component.h"
#include "engine/scene/scene.h"

namespace Chained
{
	namespace NametagSystem
	{

		static TextRenderer s_TextRenderer;
		static std::shared_ptr<VertexArray> s_QuadVAO;
		static std::shared_ptr<VertexBuffer> s_DynVBO;
		static bool s_Initialized = false;

		static constexpr int kMaxGlyphs = 256;
		static constexpr int kVertsPerGlyph = 4;
		static constexpr int kIndicesPerGlyph = 6;

		struct GlyphVertex
		{
			float x, y;
			float u, v;
		};

		static void EnsureInitialized()
		{
			if (s_Initialized)
			{
				return;
			}

			s_TextRenderer.Init();

			// Pre-allocate a dynamic VBO large enough for up to kMaxGlyphs
			uint32_t vboSize = kMaxGlyphs * kVertsPerGlyph * static_cast<uint32_t>(sizeof(GlyphVertex));
			s_DynVBO = VertexBuffer::Create(vboSize);
			s_DynVBO->SetLayout(
				{{VertexAttributeType::Float2, "a_Position"}, {VertexAttributeType::Float2, "a_TexCoords"}});

			s_QuadVAO = VertexArray::Create();
			s_QuadVAO->AddVertexBuffer(s_DynVBO);

			// Index buffer for up to kMaxGlyphs quads
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

		void DrawNametags(entt::registry& registry, const Camera3D& camera)
		{
			auto* renderer = ServiceLocator::TryGet<Renderer>();
			if (!renderer)
			{
				return;
			}

			auto* net = ServiceLocator::TryGet<Network>();
			if (!net || net->GetRole() == Role::Offline)
			{
				return;
			}

			EnsureInitialized();

			auto& shaderStorage = renderer->GetShaderLibrary();
			auto shaderAsset =
				shaderStorage.LoadOrGet("Nametag", "engine/resources/shaders/materials/nametag.chshader");
			if (!shaderAsset || !shaderAsset->IsReady())
			{
				return;
			}
			auto shader = shaderAsset->GetShader();
			if (!shader)
			{
				return;
			}

			auto* am = ServiceLocator::TryGet<AssetManager>();
			if (!am)
			{
				return;
			}

			auto fontAsset = am->Get<FontAsset>("font/lato/lato-regular.ttf");
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

			auto view = registry.view<NetworkIdentityComponent, TransformComponent>();

			shader->Bind();
			shader->SetMatrix("projection", camera.ProjectionMatrix);
			shader->SetMatrix("view", camera.ViewMatrix);
			shader->SetFloat("useBackground", 0.0f);
			shader->SetVec4("backgroundColor", glm::vec4(0.0f, 0.0f, 0.0f, 0.7f));
			shader->SetVec2("size", glm::vec2(1.0f, 1.0f));

			// Bind atlas texture once for all nametags
			GraphicsDevice::Get().SetTexture(0, font.textureAtlas->GetNativeHandle());
			shader->SetInt("textTexture", 0);

			PipelineStateGuard stateGuard;
			stateGuard.WithoutDepthTest().WithBlend().WithCullNone();

			constexpr float nametagScale = 0.007f; // world units per pixel (approx. 0.35m height)

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

				std::string label = name;

				// Layout glyphs
				auto quads = s_TextRenderer.LayoutGlyphs(label, font, 48.0f);
				if (quads.empty())
				{
					continue;
				}

				// Clamp to kMaxGlyphs
				if (static_cast<int>(quads.size()) > kMaxGlyphs)
				{
					quads.resize(kMaxGlyphs);
				}

				// Measure tight bounding box for exact centering
				float minX = 1e9f, maxX = -1e9f;
				float minY = 1e9f, maxY = -1e9f;
				for (const auto& q : quads)
				{
					if (q.x < minX)
					{
						minX = q.x;
					}
					if (q.x + q.w > maxX)
					{
						maxX = q.x + q.w;
					}
					if (q.y < minY)
					{
						minY = q.y;
					}
					if (q.y + q.h > maxY)
					{
						maxY = q.y + q.h;
					}
				}

				float centerX = (minX + maxX) * 0.5f;
				float centerY = (minY + maxY) * 0.5f;

				// Position above avatar head in world space
				glm::vec3 worldPos = tc.Translation;
				if (tc.WorldTransform != glm::mat4(1.0f) && tc.WorldTransform != glm::mat4(0.0f))
				{
					worldPos = glm::vec3(tc.WorldTransform[3]);
				}

				// Distance-compensated scaling: scales with distance so names remain legible from far away
				float distToCamera = glm::length(worldPos - camera.Position);
				float distanceScaleFactor = glm::clamp(distToCamera / 8.0f, 1.0f, 6.0f);
				float currentScale = nametagScale * distanceScaleFactor;

				float heightOffset = 2.2f + (distanceScaleFactor - 1.0f) * 0.35f;
				glm::vec3 pos = worldPos + glm::vec3(0.0f, heightOffset, 0.0f);

				// Build vertex data centered at origin in View Space (+Y is UP, -Y is DOWN)
				std::vector<GlyphVertex> verts;
				verts.reserve(quads.size() * kVertsPerGlyph);

				for (const auto& q : quads)
				{
					float lx = (q.x - centerX) * currentScale;
					float rx = (q.x + q.w - centerX) * currentScale;
					float topY = -(q.y - centerY) * currentScale;
					float botY = -(q.y + q.h - centerY) * currentScale;

					verts.push_back({lx, topY, q.s0, q.t0}); // TL
					verts.push_back({rx, topY, q.s1, q.t0}); // TR
					verts.push_back({rx, botY, q.s1, q.t1}); // BR
					verts.push_back({lx, botY, q.s0, q.t1}); // BL
				}

				if (verts.empty())
				{
					continue;
				}

				// Upload glyph vertices to dynamic VBO
				s_DynVBO->SetData(verts.data(), static_cast<uint32_t>(verts.size() * sizeof(GlyphVertex)));

				shader->SetVec3("modelPosition", pos);

				bool isLocal = (netId.NetworkID == localID);
				shader->SetVec4("textColor", isLocal ? glm::vec4(1.0f, 0.86f, 0.0f, 1.0f)
													 : (isHost ? glm::vec4(0.4f, 0.8f, 1.0f, 1.0f)
															   : glm::vec4(1.0f, 1.0f, 1.0f, 1.0f)));

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

	} // namespace NametagSystem
} // namespace Chained
