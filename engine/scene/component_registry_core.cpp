// Core component registrations (Transform, Tag, Camera, ID, Name, Hierarchy)
// Split into its own TU to reduce obj file size in MinGW Clang Debug builds.
#include "component_registry.h"
#include "engine/reflection/reflection_rfl_impl.h"
#include "components/render/camera_component.h"
#include "components/core/hierarchy_component.h"
#include "components/core/id_component.h"
#include "components/core/tag_component.h"
#include "components/core/transform_component.h"
#include "thirdparty/IconsFontAwesome6.h"

namespace Chained
{
	void RegisterCoreComponents()
	{
		ComponentRegistry::RegisterReflective<TransformComponent>("Transform", ICON_FA_ARROWS_UP_DOWN_LEFT_RIGHT,
																  "Core");
		auto& transMeta = ComponentRegistry::GetMetadataMutable(entt::type_hash<TransformComponent>::value());
		transMeta.NotifyUpdate = [](Entity e) {
			if (e.HasComponent<TransformComponent>())
			{
				auto& tc = e.GetComponent<TransformComponent>();
				tc.RotationQuat = glm::quat(tc.Rotation);
				tc.TransformChanged = true;
			}
		};

		ComponentRegistry::RegisterReflective<TagComponent>("Tag", ICON_FA_TAG, "Core");
		ComponentRegistry::RegisterReflective<CameraComponent>("Camera", ICON_FA_VIDEO, "Core");
		ComponentRegistry::RegisterReflective<IDComponent>("ID", nullptr, "Core");
		auto& idMeta = ComponentRegistry::GetMetadataMutable(entt::type_hash<IDComponent>::value());
		idMeta.Visible = false;
		idMeta.AllowAdd = false;

		ComponentRegistry::RegisterReflective<NameComponent>("Name", nullptr, "Core");
		auto& nameMeta = ComponentRegistry::GetMetadataMutable(entt::type_hash<NameComponent>::value());
		nameMeta.Visible = false;
		nameMeta.AllowAdd = false;

		ComponentRegistry::RegisterReflective<HierarchyComponent>("Hierarchy", nullptr, "Core");
		auto& hierMeta = ComponentRegistry::GetMetadataMutable(entt::type_hash<HierarchyComponent>::value());
		hierMeta.Visible = false;
		hierMeta.AllowAdd = false;
	}
} // namespace Chained
