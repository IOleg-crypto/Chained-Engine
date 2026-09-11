#ifndef CH_SCENE_PICKING_H
#define CH_SCENE_PICKING_H

#include "engine/graphics/camera_types.h"
#include "engine/physics/raycast_result.h"
#include "engine/scene/entity.h"

namespace Chained
{
	class Scene;

	namespace ScenePicker
	{
		RaycastResult Raycast(Scene* scene, const Ray& ray);
		Ray CreateRayFromViewport(const Chained::Camera3D& camera, const glm::vec2& mousePosition,
								  const glm::vec2& viewportSize);
	} // namespace ScenePicker

} // namespace Chained

#endif // CH_SCENE_PICKING_H
