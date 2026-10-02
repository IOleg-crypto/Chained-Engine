#ifndef CH_RENDER_PASS_H
#define CH_RENDER_PASS_H

#include "engine/common/base.h"
#include "engine/graphics/camera_types.h"
#include "engine/scene/scene_settings.h"
#include <entt/entt.hpp>
#include <string>

namespace Chained
{
	class SceneRenderer;
	struct SceneRenderOptions;

	struct RenderContext
	{
		entt::registry& Registry;
		const SceneSettings& Settings;
		const Camera3D& Camera;
		const SceneRenderOptions& Options;
		SceneRenderer* Renderer;
	};

	class IRenderPass
	{
	public:
		virtual ~IRenderPass() = default;

		virtual void Init()
		{
		}
		virtual void Execute(const RenderContext& ctx) = 0;
		virtual void Shutdown()
		{
		}

		virtual const std::string& GetName() const = 0;
	};
} // namespace Chained
#endif
