#include "geometry_pass.h"
#include "engine/graphics/pipeline/scene_renderer.h"
#include "engine/graphics/pipeline/frustum.h"
#include "engine/graphics/api/graphics_device.h"

namespace Chained
{

	void GeometryPass::Execute(const RenderContext& renderCtx)
	{
		PipelineStateGuard stateGuard;
		auto& renderer = *renderCtx.Renderer;

		// 1. Opaque Pass — no blending with automatic GPU instancing for matching models
		GraphicsDevice::Get().EnableDepthTest();
		GraphicsDevice::Get().SetDepthFunc(GraphicsDevice::DepthFunc::LEqual);
		GraphicsDevice::Get().EnableDepthMask();
		GraphicsDevice::Get().SetBlendEnabled(false);

		const auto& opaqueQueue = renderer.GetOpaqueQueue();
		for (size_t i = 0; i < opaqueQueue.size();)
		{
			const auto& firstItem = opaqueQueue[i];

			// Check for consecutive identical static instances (same asset, materials, shader override)
			if (firstItem.Asset && firstItem.BoneMatrices.empty() && firstItem.Materials.empty() &&
				!firstItem.ShaderOverride && firstItem.CustomUniforms.empty())
			{
				size_t j = i + 1;
				std::vector<glm::mat4> transforms = {firstItem.Transform};
				while (j < opaqueQueue.size() && opaqueQueue[j].Asset == firstItem.Asset &&
					   opaqueQueue[j].BoneMatrices.empty() && !opaqueQueue[j].ShaderOverride &&
					   opaqueQueue[j].CustomUniforms.empty() && opaqueQueue[j].Materials.empty())
				{
					transforms.push_back(opaqueQueue[j].Transform);
					++j;
				}

				if (transforms.size() > 1)
				{
					renderer.DrawModelInstanced(firstItem.Asset, transforms, firstItem.Materials, nullptr,
												RenderPassStage::Opaque);
					i = j;
					continue;
				}
			}

			renderer.DrawModel(firstItem.Asset, firstItem.Transform, firstItem.BoneMatrices, firstItem.Materials,
							   firstItem.ShaderOverride, firstItem.CustomUniforms, RenderPassStage::Opaque);
			++i;
		}

		// 2. Transparent Pass — enable blending
		GraphicsDevice::Get().SetBlendEnabled(true);
		GraphicsDevice::Get().SetBlendFunc(GraphicsDevice::BlendFactor::SrcAlpha,
										   GraphicsDevice::BlendFactor::OneMinusSrcAlpha);
		GraphicsDevice::Get().DisableDepthMask();
		for (const auto& item : renderer.GetTransparentQueue())
		{
			renderer.DrawModel(item.Asset, item.Transform, item.BoneMatrices, item.Materials, item.ShaderOverride,
							   item.CustomUniforms, RenderPassStage::Transparent);
		}
		GraphicsDevice::Get().EnableDepthMask();
		GraphicsDevice::Get().SetBlendEnabled(false);
	}

} // namespace Chained
