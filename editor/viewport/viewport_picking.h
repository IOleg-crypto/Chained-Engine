#ifndef CH_VIEWPORT_PICKING_H
#define CH_VIEWPORT_PICKING_H

#include "engine/graphics/camera_types.h"
#include "engine/physics/raycast_result.h"
#include "engine/scene/entity.h"
#include <imgui.h>
#include <glm/glm.hpp>

namespace Chained
{
	class Scene;
	class EditorGizmo;
	class EditorUIManipulator;

	// Handles object picking in the viewport: UI widget picking, billboard icon
	// picking, and 3D raycast picking. Returns the selected entity.
	class ViewportPicking
	{
	public:
		// Performs picking when the left mouse button is clicked.
		void HandlePicking(Scene* scene, const ImVec2& viewportSize, const ImVec2& viewportScreenPos,
						   EditorGizmo& gizmo, EditorUIManipulator& uiManipulator, const Camera3D& camera);

		// Creates a ray from the mouse position in viewport-local coordinates.
		Ray GetMouseRay(Scene* scene, const glm::vec2& mousePosition, const glm::vec2& viewportSize,
						const Camera3D& camera);

	private:
		// Screen-space hit test against billboard editor icons.
		Entity HandleIconPicking(Scene* scene, const Camera3D& camera, const ImVec2& mousePos,
								 const ImVec2& viewportSize, const ImVec2& viewportScreenPos);
	};

} // namespace Chained

#endif // CH_VIEWPORT_PICKING_H
