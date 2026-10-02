#include "geometry_pass.h"
#include "engine/graphics/pipeline/scene_renderer.h"
#include "engine/graphics/pipeline/frustum.h"
#include "engine/graphics/api/graphics_device.h"
#include "engine/core/service_locator.h"

namespace Chained
{

	void GeometryPass::Execute(const RenderContext& renderCtx)
	{
		PipelineStateGuard stateGuard;
		auto& renderer = *renderCtx.Renderer;
		auto* device = renderCtx.Device ? renderCtx.Device : ServiceLocator::TryGet<GraphicsDevice>();

		// 1. Opaque Pass — no blending with automatic GPU instancing for matching models
		if (device)
		{
			device->EnableDepthTest();
			device->SetDepthFunc(GraphicsDevice::DepthFunc::LEqual);
			device->EnableDepthMask();
			device->SetBlendEnabled(false);
		}

		const auto& opaqueQueue = renderer.GetOpaqueQueue();
		for (size_t i = 0; i < opaqueQueue.size();)
		{
			const auto& firstItem = opaqueQueue[i];

			// Batch consecutive items: same asset, no bones/shader-override/custom-uniforms,
			// AND identical material overrides (including both-empty case).
			if (firstItem.Asset && firstItem.BoneMatrices.empty() && !firstItem.ShaderOverride &&
				firstItem.CustomUniforms.empty())
			{
				size_t j = i + 1;
				std::vector<glm::mat4> transforms = {firstItem.Transform};
				while (j < opaqueQueue.size() && opaqueQueue[j].Asset == firstItem.Asset &&
					   opaqueQueue[j].BoneMatrices.empty() && !opaqueQueue[j].ShaderOverride &&
					   opaqueQueue[j].CustomUniforms.empty() && opaqueQueue[j].Materials == firstItem.Materials)
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
		if (device)
		{
			device->SetBlendEnabled(true);
			device->SetBlendFunc(GraphicsDevice::BlendFactor::SrcAlpha, GraphicsDevice::BlendFactor::OneMinusSrcAlpha);
			device->DisableDepthMask();
		}

		for (const auto& item : renderer.GetTransparentQueue())
		{
			renderer.DrawModel(item.Asset, item.Transform, item.BoneMatrices, item.Materials, item.ShaderOverride,
							   item.CustomUniforms, RenderPassStage::Transparent);
		}

		if (device)
		{
			device->EnableDepthMask();
			device->SetBlendEnabled(false);
		}
	}

} // namespace Chained
