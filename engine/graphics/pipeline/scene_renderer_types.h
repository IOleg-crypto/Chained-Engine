#ifndef CH_SCENE_RENDERER_TYPES_H
#define CH_SCENE_RENDERER_TYPES_H

#include "engine/graphics/api/renderer_types.h"
#include "engine/scene/components/animation/animation_component.h"
#include "engine/assets/types/model_asset.h"
#include "engine/assets/types/environment_asset.h"
#include <vector>
#include <memory>
#include <glm/glm.hpp>

namespace Chained
{

	class Shader;

	enum class RenderPassStage
	{
		Opaque,
		Transparent,
		Both
	};

	struct SceneRenderOptions
	{
		std::shared_ptr<EnvironmentAsset> EnvironmentOverride = nullptr;

		bool ShowDebugColliders = false;
		bool ShowDebugCollisionModelBox = false;
		bool ShowDebugSpawnZones = false;
		bool DrawGrid = false;
		int SetCollisionWireframeMode = 0;
	};

	struct AnimatedEntry
	{
		ModelAsset* Asset;
		glm::mat4 WorldTransform;
		Shader* ShaderOverride;
		std::vector<ShaderUniform> CustomUniforms;
		AnimationComponent Animation;
	};

	struct RenderItem
	{
		ModelAsset* Asset;
		glm::mat4 Transform;
		std::vector<glm::mat4> BoneMatrices;
		std::vector<Material> Materials;
		Shader* ShaderOverride;
		std::vector<ShaderUniform> CustomUniforms;
		float Distance = 0.0f;
	};

} // namespace Chained

#endif // CH_SCENE_RENDERER_TYPES_H
