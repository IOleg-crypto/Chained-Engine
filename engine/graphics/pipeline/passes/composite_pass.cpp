#include "composite_pass.h"
#include "engine/core/service_locator.h"
#include "engine/graphics/pipeline/scene_renderer.h"
#include "engine/graphics/pipeline/renderer.h"
#include "engine/graphics/pipeline/lighting_manager.h"

namespace Chained
{

	void CompositePass::Init()
	{
		m_Initialized = true;
	}

	void CompositePass::Execute(const RenderContext& ctx)
	{
		if (!m_Initialized)
		{
			return;
		}

		const auto& env = ctx.Renderer->GetEnvironment();

		// Delegate fog uniform upload to LightingManager to avoid duplication.
		auto* renderer = ServiceLocator::TryGet<Renderer>();
		if (!renderer)
		{
			return;
		}
		if (!renderer->GetShaderLibrary().Exists("Lighting"))
		{
			return;
		}

		auto lightingAsset = renderer->GetShaderLibrary().Get("Lighting");
		if (!lightingAsset)
		{
			return;
		}

		auto shader = lightingAsset->GetShader();
		if (!shader)
		{
			return;
		}

		shader->Bind();
		renderer->GetLightingManager().ApplyUniforms(shader.get(), renderer->GetFrame());

		// Future: full-screen HDR blit with tone-mapping when m_HDRTarget is set.
	}

	void CompositePass::Shutdown()
	{
		m_HDRTarget.reset();
		m_Initialized = false;
	}

} // namespace Chained
