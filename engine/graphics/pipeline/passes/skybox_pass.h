#ifndef SKYBOX_PASS_H
#define SKYBOX_PASS_H

#include "engine/graphics/pipeline/render_pass.h"

namespace Chained
{

	class Texture;

	class SkyboxPass : public IRenderPass
	{
	public:
		void Execute(const RenderContext& ctx) override;
		std::string_view GetName() const override
		{
			return "SkyboxPass";
		}

	private:
		std::shared_ptr<Texture> m_CachedSixFacesCubemap;
		std::string m_CachedFacesKey;
	};

} // namespace Chained
#endif
