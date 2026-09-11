#include "scene_picking.h"
#include "engine/app/application.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/types/model_asset.h"
#include "engine/core/service_locator.h"
#include "engine/physics/physics.h"
#include "engine/scene/components.h"
#include "engine/scene/systems/transform_system.h"
#include "engine/scene/scene.h"
#include <algorithm>
#include <cmath>
#include <float.h>
#include <limits>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace Chained
{

	static bool RayAABBInternal(const glm::vec3& origin, const glm::vec3& dir, const glm::vec3& min,
								const glm::vec3& max, float& t)
	{
		auto SafeInv = [](float d) -> float {
			return (std::abs(d) < std::numeric_limits<float>::epsilon())
					   ? std::copysign(std::numeric_limits<float>::infinity(), d)
					   : 1.0f / d;
		};
		glm::vec3 invDir(SafeInv(dir.x), SafeInv(dir.y), SafeInv(dir.z));
		glm::vec3 t0 = (min - origin) * invDir;
		glm::vec3 t1 = (max - origin) * invDir;

		glm::vec3 tMin = glm::min(t0, t1);
		glm::vec3 tMax = glm::max(t0, t1);

		float nearT = std::max({tMin.x, tMin.y, tMin.z});
		float farT = std::min({tMax.x, tMax.y, tMax.z});

		if (farT < std::max(0.0f, nearT))
		{
			return false;
		}
		t = nearT;
		return true;
	}

	RaycastResult ScenePicker::Raycast(Scene* scene, const Ray& ray)
	{
		RaycastResult finalResult;
		finalResult.Hit = false;
		finalResult.Distance = FLT_MAX;

		if (!scene)
		{
			return finalResult;
		}

		if (auto* physics = ServiceLocator::TryGet<Physics>())
		{
			RaycastResult physResult = physics->Raycast(ray);
			if (physResult.Hit)
			{
				finalResult = physResult;
			}
		}

		auto modelView = scene->GetRegistry().view<TransformComponent, ModelComponent>();
		modelView.each([&](entt::entity entityID, TransformComponent& tc, ModelComponent& modelComp) {
			if (finalResult.Hit && finalResult.Entity == entityID)
			{
				return;
			}

			if (modelComp.ModelPath.empty())
			{
				return;
			}

			AssetManager* assetManager = ServiceLocator::TryGet<AssetManager>();
			if (!assetManager)
			{
				return;
			}
			auto modelAsset = assetManager->Get<ModelAsset>(modelComp.ModelPath);
			if (!modelAsset || !modelAsset->IsReady())
			{
				return;
			}

			glm::mat4 modelTransform = TransformSystem::ComputeLocalMatrix(tc);
			glm::mat4 invTransform = glm::inverse(modelTransform);

			Ray localRay;
			localRay.position = glm::vec3(invTransform * glm::vec4(ray.position, 1.0f));
			glm::vec3 localTarget = glm::vec3(invTransform * glm::vec4(ray.position + ray.direction, 1.0f));
			localRay.direction = glm::normalize(localTarget - localRay.position);

			float aabbT = 0.0f;
			glm::vec3 modelMin = modelAsset->GetBoundingBox().Min;
			glm::vec3 modelMax = modelAsset->GetBoundingBox().Max;

			if (modelMin != modelMax)
			{
				if (!RayAABBInternal(localRay.position, localRay.direction, modelMin, modelMax, aabbT))
				{
					return;
				}
			}

			float t_local = FLT_MAX;
			glm::vec3 localNormal = {0, 0, 0};
			bool hit = false;

			const auto& instances = modelAsset->GetInstances();
			const auto& rawMeshes = modelAsset->GetMeshes();

			for (const auto& inst : instances)
			{
				if (inst.meshIndex < 0 || inst.meshIndex >= (int)rawMeshes.size())
				{
					continue;
				}

				const MeshData& raw = rawMeshes[inst.meshIndex];
				if (raw.indices.size() < 3)
				{
					continue;
				}

				glm::mat4 invLocalInst = glm::inverse(inst.localTransform);
				glm::vec3 meshSpaceOrigin = glm::vec3(invLocalInst * glm::vec4(localRay.position, 1.0f));
				glm::vec3 meshSpaceDir = glm::normalize(glm::vec3(invLocalInst * glm::vec4(localRay.direction, 0.0f)));

				for (size_t i = 0; i + 2 < raw.indices.size(); i += 3)
				{
					uint32_t i0 = raw.indices[i];
					uint32_t i1 = raw.indices[i + 1];
					uint32_t i2 = raw.indices[i + 2];

					size_t v0Idx = (size_t)i0 * 3;
					size_t v1Idx = (size_t)i1 * 3;
					size_t v2Idx = (size_t)i2 * 3;

					if (v0Idx + 2 >= raw.vertices.size() || v1Idx + 2 >= raw.vertices.size() ||
						v2Idx + 2 >= raw.vertices.size())
					{
						continue;
					}

					glm::vec3 v0 = {raw.vertices[v0Idx], raw.vertices[v0Idx + 1], raw.vertices[v0Idx + 2]};
					glm::vec3 v1 = {raw.vertices[v1Idx], raw.vertices[v1Idx + 1], raw.vertices[v1Idx + 2]};
					glm::vec3 v2 = {raw.vertices[v2Idx], raw.vertices[v2Idx + 1], raw.vertices[v2Idx + 2]};

					glm::vec3 e1 = v1 - v0, e2 = v2 - v0;
					glm::vec3 h = glm::cross(meshSpaceDir, e2);
					float a = glm::dot(e1, h);
					if (std::abs(a) < 1e-7f)
					{
						continue;
					}
					float f = 1.0f / a;
					glm::vec3 s = meshSpaceOrigin - v0;
					float u = f * glm::dot(s, h);
					if (u < 0.0f || u > 1.0f)
					{
						continue;
					}
					glm::vec3 q = glm::cross(s, e1);
					float v = f * glm::dot(meshSpaceDir, q);
					if (v < 0.0f || u + v > 1.0f)
					{
						continue;
					}

					float triT = f * glm::dot(e2, q);
					if (triT > 0.0f)
					{

						glm::vec3 hitMeshSpace = meshSpaceOrigin + meshSpaceDir * triT;
						glm::vec3 hitLocalSpace = glm::vec3(inst.localTransform * glm::vec4(hitMeshSpace, 1.0f));

						float currentLocalT = glm::distance(localRay.position, hitLocalSpace);

						if (currentLocalT < t_local)
						{
							t_local = currentLocalT;
							hit = true;

							glm::vec3 meshNormal = glm::normalize(glm::cross(e1, e2));
							localNormal =
								glm::normalize(glm::vec3(glm::transpose(invLocalInst) * glm::vec4(meshNormal, 0.0f)));
						}
					}
				}
			}

			if (hit)
			{
				glm::vec3 hitPosLocal = localRay.position + localRay.direction * t_local;
				glm::vec3 hitPosWorld = glm::vec3(modelTransform * glm::vec4(hitPosLocal, 1.0f));
				float distWorld = glm::distance(ray.position, hitPosWorld);

				if (distWorld < finalResult.Distance)
				{
					finalResult.Distance = distWorld;
					finalResult.Hit = true;
					finalResult.Entity = entityID;
					finalResult.Position = hitPosWorld;
				}
			}
		});

		return finalResult;
	}

	Ray ScenePicker::CreateRayFromViewport(const Chained::Camera3D& camera, const glm::vec2& mousePosition,
										   const glm::vec2& viewportSize)
	{
		float ndc_x = (2.0f * mousePosition.x) / viewportSize.x - 1.0f;
		float ndc_y = 1.0f - (2.0f * mousePosition.y) / viewportSize.y;

		glm::mat4 invVP = glm::inverse(camera.ProjectionMatrix * camera.ViewMatrix);

		auto Unproject = [&](float x, float y, float z) -> glm::vec3 {
			glm::vec4 ndc(x, y, z, 1.0f);
			glm::vec4 world = invVP * ndc;
			if (std::abs(world.w) > 1e-6f)
			{
				return glm::vec3(world) / world.w;
			}
			return glm::vec3(0.0f);
		};

#ifdef GLM_FORCE_DEPTH_ZERO_TO_ONE
		float nearZ = 0.0f;
#else
		float nearZ = -1.0f;
#endif
		glm::vec3 nearPoint = Unproject(ndc_x, ndc_y, nearZ);
		glm::vec3 farPoint = Unproject(ndc_x, ndc_y, 1.0f);

		Ray ray;
		ray.position = nearPoint;
		ray.direction = glm::normalize(farPoint - nearPoint);

		return ray;
	}

} // namespace Chained