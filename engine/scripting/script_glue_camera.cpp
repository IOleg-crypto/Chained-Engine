#include "script_glue_camera.h"

#include "engine/app/application.h"
#include "engine/core/service_locator.h"
#include "engine/physics/physics.h"
#include "engine/physics/raycast_result.h"
#include "engine/scene/components.h"

namespace Chained
{
	void Camera_GetForward(uint64_t entityID, glm::vec3* outForward)
	{
		if (!outForward)
		{
			return;
		}
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<TransformComponent>())
		{
			auto& tc = entity.GetComponent<TransformComponent>();
			glm::quat rotation = tc.RotationQuat;
			*outForward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
		}
		else
		{
			CH_CORE_WARN("[Diag Camera_GetForward] entityID={} NOT FOUND or no TransformComponent!", entityID);
			*outForward = {};
		}
	}
	void Camera_GetRight(uint64_t entityID, glm::vec3* outRight)
	{
		if (!outRight)
		{
			return;
		}
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<TransformComponent>())
		{
			auto& tc = entity.GetComponent<TransformComponent>();
			glm::quat rotation = tc.RotationQuat;
			*outRight = rotation * glm::vec3(1.0f, 0.0f, 0.0f);
		}
		else
		{
			*outRight = {};
		}
	}
	void Camera_SetOrbit(uint64_t entityID, float yaw, float pitch, float distance)
	{
		Entity entity = GetEntity(entityID);
		Scene* scene = GetActiveScene();

		if (entity && entity.HasComponent<CameraComponent>() && entity.HasComponent<TransformComponent>() && scene)
		{
			auto& camera = entity.GetComponent<CameraComponent>();

			// 1. Clamp pitch
			pitch = glm::clamp(pitch, 5.0f, 75.0f);

			camera.OrbitYaw = yaw;
			camera.OrbitPitch = pitch;
			camera.OrbitDistance = distance;

			auto& tc = entity.GetComponent<TransformComponent>();
			Entity target;

			// Find the player entity (networked: IsOwner, offline: by tag)
			auto netView = scene->GetRegistry().view<NetworkIdentityComponent>();
			for (auto e : netView)
			{
				const auto& netID = netView.get<NetworkIdentityComponent>(e);
				if (netID.IsOwner)
				{
					target = Entity(e, scene->GetRegistryPtr());
					break;
				}
			}

			if (!target)
			{
				target = scene->FindEntityByTag(camera.TargetEntityTag);
			}

			glm::vec3 targetPos = camera.SmoothedPivot;
			if (target && target.HasComponent<TransformComponent>())
			{
				const auto& targetTC = target.GetComponent<TransformComponent>();

				// Use the local Translation (not WorldTransform[3]) so the camera
				// tracks the player's CURRENT position rather than the position that
				// was computed at the start of the frame. For root entities (no parent)
				// these are identical; for parented entities we read from the parent's
				// WorldTransform instead.
				glm::vec3 rawPos;
				const auto& hier = scene->GetRegistry().try_get<HierarchyComponent>(target);
				if (hier && hier->Parent != entt::null && scene->GetRegistry().all_of<TransformComponent>(hier->Parent))
				{
					// Parented: reconstruct world position from parent world + local translation
					const auto& parentTC = scene->GetRegistry().get<TransformComponent>(hier->Parent);
					rawPos = glm::vec3(parentTC.WorldTransform * glm::vec4(targetTC.Translation, 1.0f));
				}
				else
				{
					rawPos = targetTC.Translation;
				}

				// Smoothly follow player position so the camera never lags by 1 frame.
				// Use a very high lerp speed (30) so it's effectively instant for owned
				// players (physics-driven), while still smoothing out any jitter for
				// remote/interpolated entities.
				constexpr float kPivotSmoothSpeed = 30.0f;
				float dt = Application::Get().GetFrameTime();
				float lerpT = glm::clamp(dt * kPivotSmoothSpeed, 0.0f, 1.0f);

				if (!camera.PivotInitialized || glm::distance2(camera.SmoothedPivot, rawPos) > 25.0f)
				{
					camera.SmoothedPivot = rawPos;
					camera.PivotInitialized = true;
				}
				else
				{
					camera.SmoothedPivot = glm::mix(camera.SmoothedPivot, rawPos, lerpT);
				}

				targetPos = camera.SmoothedPivot;
			}

			// 2. Pivot at eye/chest height (+1.8m)
			glm::vec3 pivot = targetPos + glm::vec3(0.0f, 1.8f, 0.0f);

			// 3. Compute rotation quaternion
			float yawRad = glm::radians(yaw);
			float pitchRad = glm::radians(pitch);

			glm::quat yawQuat = glm::angleAxis(yawRad, glm::vec3(0.0f, 1.0f, 0.0f));
			glm::quat pitchQuat = glm::angleAxis(-pitchRad, glm::vec3(1.0f, 0.0f, 0.0f));
			glm::quat rotation = yawQuat * pitchQuat;

			glm::vec3 offset = rotation * glm::vec3(0.0f, 0.0f, distance);
			glm::vec3 newPos = pivot + offset;

			TransformSystem::SetTranslation(tc, newPos);
			TransformSystem::SetRotationQuat(tc, rotation);

			tc.WorldTransform =
				glm::translate(glm::mat4(1.0f), newPos) * glm::toMat4(rotation) * glm::scale(glm::mat4(1.0f), tc.Scale);
			tc.InverseWorldTransform = glm::inverse(tc.WorldTransform);
			tc.TransformChanged = false;
		}
	}
	void Camera_GetOrbit(uint64_t entityID, float* yaw, float* pitch, float* distance)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<CameraComponent>())
		{
			auto& camera = entity.GetComponent<CameraComponent>();
			if (yaw)
			{
				*yaw = camera.OrbitYaw;
			}
			if (pitch)
			{
				*pitch = camera.OrbitPitch;
			}
			if (distance)
			{
				*distance = camera.OrbitDistance;
			}
		}
	}
	uint8_t Camera_GetPrimary(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		return entity && entity.HasComponent<CameraComponent>() ? entity.GetComponent<CameraComponent>().Primary
																: false;
	}
	void Camera_SetPrimary(uint64_t entityID, uint8_t primary)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<CameraComponent>())
		{
			CH_CORE_TRACE("[ScriptGlue] Camera_SetPrimary: Entity={}, Primary={}", (uint32_t)entityID, primary);
			entity.GetComponent<CameraComponent>().Primary = primary;
		}
	}
	void Camera_SetIsOrbit(uint64_t entityID, uint8_t isOrbit)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<CameraComponent>())
		{
			entity.GetComponent<CameraComponent>().IsOrbitCamera = isOrbit;
		}
	}
	const Coral::UCChar* Camera_GetTargetTag(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		std::string tag = entity && entity.HasComponent<CameraComponent>()
							  ? entity.GetComponent<CameraComponent>().TargetEntityTag
							  : "";
		return GlueStringPool::ReturnString(tag);
	}
	void Camera_SetTargetTag(uint64_t entityID, const Coral::UCChar* tag)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<CameraComponent>() && tag)
		{
			entity.GetComponent<CameraComponent>().TargetEntityTag = ch_u16_to_string(tag);
		}
	}

	uint8_t Camera_GetIsOrbit(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		return entity && entity.HasComponent<CameraComponent>() ? entity.GetComponent<CameraComponent>().IsOrbitCamera
																: false;
	}
} // namespace Chained
