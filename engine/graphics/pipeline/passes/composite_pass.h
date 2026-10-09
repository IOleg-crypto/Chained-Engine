#ifndef COMPOSITE_PASS_H
#define COMPOSITE_PASS_H

#include "engine/graphics/pipeline/render_pass.h"
#include "engine/graphics/api/framebuffer.h"

namespace Chained
{

	// Final composite / post-processing pass.
	// Uploads per-frame post-process uniforms (fog, tone-mapping) and, in the
	// future, can blit from an HDR intermediate framebuffer to the back buffer.
	class CompositePass : public IRenderPass
	{
	public:
		void Init() override;
		void Execute(const RenderContext& ctx) override;
		void Shutdown() override;

		std::string_view GetName() const override
		{
			return "CompositePass";
		}

		// Provide an HDR intermediate target produced by earlier passes.
		// If null, the pass operates in a passthrough mode.
		void SetHDRTarget(std::shared_ptr<Framebuffer> target)
		{
			m_HDRTarget = target;
		}

	private:
		std::shared_ptr<Framebuffer> m_HDRTarget;
		bool m_Initialized = false;
	};

} // namespace Chained
#endif
