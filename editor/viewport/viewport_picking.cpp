#include "viewport_picking.h"
#include "editor/layer.h"
#include "editor/scene_picking.h"
#include "editor/viewport/gizmo.h"
#include "editor/viewport/ui_manipulator.h"
#include "editor/events.h"
#include "engine/scene/components.h"
#include "engine/scene/scene.h"
#include "engine/ui/widget_renderer.h"
#include "engine/core/service_locator.h"
#include <limits>

namespace Chained
{

	static constexpr float kMinIconClickRadius = 14.0f;

	Ray ViewportPicking::GetMouseRay(Scene* scene, const glm::vec2& mousePosition, const glm::vec2& viewportSize,
									 const Camera3D& camera)
	{
		if (!scene)
		{
			return {};
		}
		return ScenePicker::CreateRayFromViewport(camera, mousePosition, viewportSize);
	}

	Entity ViewportPicking::HandleIconPicking(Scene* scene, const Camera3D& camera, const ImVec2& mousePos,
											  const ImVec2& viewportSize, const ImVec2& viewportScreenPos)
	{
		if (!EditorLayer::Get().GetConfig().ShowEditorIcons)
		{
			return {};
		}

		const auto& editorCfg = EditorLayer::Get().GetConfig();
		const float iconMin = editorCfg.IconSizeMin;
		const float iconMax = editorCfg.IconSizeMax;
		const float iconScale = editorCfg.IconSizeScale;

		const glm::mat4 vp = camera.ProjectionMatrix * camera.ViewMatrix;

		auto worldToScreen = [&](const glm::vec3& wp) -> glm::vec2 {
			glm::vec4 clip = vp * glm::vec4(wp, 1.0f);
			if (clip.w <= 0.0f)
			{
				return {-1.f, -1.f};
			}
			const glm::vec3 ndc = glm::vec3(clip) / clip.w;
			return {(ndc.x * 0.5f + 0.5f) * viewportSize.x + viewportScreenPos.x,
					(1.0f - (ndc.y * 0.5f + 0.5f)) * viewportSize.y + viewportScreenPos.y};
		};

		auto iconPixelRadius = [&](const glm::vec3& wp) -> float {
			const float dist = glm::distance(wp, camera.Position);
			const float worldSz = std::clamp(dist * iconScale, iconMin, iconMax);
			float ppu;
			if (camera.Projection == ProjectionType::Perspective && dist > 0.001f)
			{
				ppu = (viewportSize.y * 0.5f) / (std::tan(glm::radians(camera.FovDegrees) * 0.5f) * dist);
			}
			else
			{
				ppu = (viewportSize.y * 0.5f) / std::max(camera.OrthographicSize, 0.001f);
			}
			return std::max(worldSz * ppu * 0.5f, kMinIconClickRadius);
		};

		Entity bestHit = {};
		float bestIconDist = FLT_MAX;

		auto testIcon = [&](entt::entity id, const glm::vec3& wp) {
			const glm::vec2 sp = worldToScreen(wp);
			if (sp.x < 0.f)
			{
				return;
			}
			const float r = iconPixelRadius(wp);
			const float dx = mousePos.x - sp.x;
			const float dy = mousePos.y - sp.y;
			if (dx * dx + dy * dy <= r * r)
			{
				const float d = glm::distance(wp, camera.Position);
				if (d < bestIconDist)
				{
					bestIconDist = d;
					bestHit = Entity(id, &scene->GetRegistry());
				}
			}
		};

		auto& reg = scene->GetRegistry();

		reg.view<TransformComponent, CameraComponent>().each(
			[&](entt::entity id, TransformComponent& tc, CameraComponent&) {
				const glm::vec3 wp = glm::vec3(tc.WorldTransform[3]);
				if (glm::distance(wp, camera.Position) >= 0.25f)
				{
					testIcon(id, wp);
				}
			});

		reg.view<TransformComponent, LightComponent>().each(
			[&](entt::entity id, TransformComponent& tc, LightComponent&) {
				testIcon(id, glm::vec3(tc.WorldTransform[3]));
			});

		reg.view<TransformComponent, SpawnComponent>().each(
			[&](entt::entity id, TransformComponent& tc, SpawnComponent&) {
				testIcon(id, glm::vec3(tc.WorldTransform[3]));
			});

		reg.view<TransformComponent, AudioComponent>().each(
			[&](entt::entity id, TransformComponent& tc, AudioComponent&) {
				testIcon(id, glm::vec3(tc.WorldTransform[3]));
			});

		return bestHit;
	}

	void ViewportPicking::HandlePicking(Scene* scene, const ImVec2& viewportSize, const ImVec2& viewportScreenPos,
										EditorGizmo& gizmo, EditorUIManipulator& uiManipulator, const Camera3D& camera)
	{
		if (EditorLayer::Get().GetSceneManager().IsTransitioning())
		{
			return;
		}

		bool isClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
		bool isDragging = uiManipulator.IsActive();
		bool isGizmoDragging = gizmo.IsDragging();
		bool isGizmoHovered = gizmo.IsHovered();
		SceneState sceneState = EditorLayer::Get().GetSceneManager().GetSceneState();
		ImVec2 mousePos = ImGui::GetMousePos();
		bool mouseInViewport =
			(mousePos.x >= viewportScreenPos.x && mousePos.x <= viewportScreenPos.x + viewportSize.x &&
			 mousePos.y >= viewportScreenPos.y && mousePos.y <= viewportScreenPos.y + viewportSize.y);

		if ((sceneState == SceneState::Edit || sceneState == SceneState::Simulate) && mouseInViewport && isClicked &&
			!isGizmoDragging && !isGizmoHovered && !isDragging)
		{
			ImVec2 localMouseImGui = {mousePos.x - viewportScreenPos.x, mousePos.y - viewportScreenPos.y};

			Ray ray =
				GetMouseRay(scene, {localMouseImGui.x, localMouseImGui.y}, {viewportSize.x, viewportSize.y}, camera);

			Entity bestHit = {};

			// UI Picking
			auto uiView = scene->GetRegistry().view<ControlComponent>();
			int bestZOrder = std::numeric_limits<int>::min();
			for (auto entityID : uiView)
			{
				Entity entity(entityID, &scene->GetRegistry());
				auto& cc = uiView.get<ControlComponent>(entityID);
				if (!cc.IsActive || cc.HiddenInHierarchy)
				{
					continue;
				}

				auto* widgetRenderer = ServiceLocator::TryGet<WidgetRenderer>();
				auto rect = widgetRenderer ? widgetRenderer->GetEntityRect(entity) : UIRect{0, 0, 0, 0};
				if (mousePos.x >= rect.x && mousePos.x <= rect.x + rect.width && mousePos.y >= rect.y &&
					mousePos.y <= rect.y + rect.height)
				{
					if (cc.ZOrder >= bestZOrder)
					{
						bestZOrder = cc.ZOrder;
						bestHit = entity;
					}
				}
			}

			// Icon Picking
			if (!bestHit)
			{
				bestHit = HandleIconPicking(scene, camera, mousePos, viewportSize, viewportScreenPos);
			}

			// 3D Picking
			if (!bestHit)
			{
				RaycastResult result = ScenePicker::Raycast(scene, ray);
				if (result.Hit)
				{
					bestHit = Entity(result.Entity, &scene->GetRegistry());
				}
			}

			if (bestHit)
			{
				SelectEntity(bestHit, scene);
			}
			else
			{
				if (mouseInViewport)
				{
					DeselectEntity(scene);
				}
			}
		}
	}

} // namespace Chained
