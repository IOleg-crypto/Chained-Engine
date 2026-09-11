#include "entity_collector.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/types/material_asset.h"
#include "engine/assets/types/model_asset.h"
#include "engine/assets/types/shader_asset.h"
#include "engine/core/service_locator.h"
#include "engine/scene/components.h"
#include "engine/scene/entity.h"

#include <cmath>

#include <glm/gtx/matrix_decompose.hpp>

namespace Chained
{

	static glm::mat4 InterpolateBoneMatrices(const glm::mat4& a, const glm::mat4& b, float t)
	{
		glm::vec3 scaleA, translationA, skewA;
		glm::quat rotationA;
		glm::vec4 perspectiveA;
		glm::decompose(a, scaleA, rotationA, translationA, skewA, perspectiveA);

		glm::vec3 scaleB, translationB, skewB;
		glm::quat rotationB;
		glm::vec4 perspectiveB;
		glm::decompose(b, scaleB, rotationB, translationB, skewB, perspectiveB);

		glm::vec3 translation = glm::mix(translationA, translationB, t);
		glm::quat rotation = glm::slerp(rotationA, rotationB, t);
		glm::vec3 scale = glm::mix(scaleA, scaleB, t);

		return glm::translate(glm::mat4(1.0f), translation) * glm::mat4_cast(rotation) *
			   glm::scale(glm::mat4(1.0f), scale);
	}

	EntityCollector::EntityCollector(MaterialManager& materialManager)
		: m_MaterialManager(materialManager)
	{
	}

	void EntityCollector::Clear()
	{
		m_OpaqueQueue.clear();
		m_TransparentQueue.clear();
	}

	void EntityCollector::Collect(entt::registry& registry, const Frustum& frustum, const glm::vec3& cameraPos)
	{
		Clear();

		auto* assets = ServiceLocator::TryGet<AssetManager>();

		auto meshView = registry.view<TransformComponent, ModelComponent>();
		for (auto entity : meshView)
		{
			auto [transform, mesh] = meshView.get<TransformComponent, ModelComponent>(entity);
			if (mesh.ModelPath.empty())
			{
				continue;
			}

			auto modelAsset = assets->Get<ModelAsset>(mesh.ModelPath);
			if (!modelAsset || modelAsset->GetState() != AssetState::Ready)
			{
				continue;
			}

			EnqueueModelAsset(registry, entity, modelAsset.get(), transform.WorldTransform, frustum, cameraPos);
		}
	}

	bool EntityCollector::EnqueueModelAsset(entt::registry& registry, entt::entity entity, ModelAsset* modelAsset,
											const glm::mat4& worldTransform, const Frustum& frustum,
											const glm::vec3& cameraPos)
	{
		BoundingBox bbox = modelAsset->GetBoundingBox();

		if (registry.all_of<AnimationComponent>(entity))
		{
			glm::vec3 center = (bbox.Max + bbox.Min) * 0.5f;
			glm::vec3 size = (bbox.Max - bbox.Min) * 2.0f;
			bbox.Min = center - size * 0.5f;
			bbox.Max = center + size * 0.5f;
		}

		glm::vec3 localCenter = (bbox.Max + bbox.Min) * 0.5f;
		glm::vec3 localExtents = (bbox.Max - bbox.Min) * 0.5f;

		glm::vec3 worldCenter = glm::vec3(worldTransform * glm::vec4(localCenter, 1.0f));
		glm::vec3 worldExtents = {
			std::abs(worldTransform[0][0]) * localExtents.x + std::abs(worldTransform[1][0]) * localExtents.y +
				std::abs(worldTransform[2][0]) * localExtents.z,
			std::abs(worldTransform[0][1]) * localExtents.x + std::abs(worldTransform[1][1]) * localExtents.y +
				std::abs(worldTransform[2][1]) * localExtents.z,
			std::abs(worldTransform[0][2]) * localExtents.x + std::abs(worldTransform[1][2]) * localExtents.y +
				std::abs(worldTransform[2][2]) * localExtents.z,
		};
		if (!IsBoxVisible(frustum, worldCenter, worldExtents))
		{
			return false;
		}

		std::shared_ptr<ShaderAsset> shaderOver;
		std::vector<ShaderUniform> uniforms;
		if (registry.all_of<ShaderComponent>(entity))
		{
			auto& sc = registry.get<ShaderComponent>(entity);
			if (sc.Enabled && !sc.ShaderPath.empty())
			{
				if (auto* am = ServiceLocator::TryGet<AssetManager>())
				{
					shaderOver = am->Get<ShaderAsset>(sc.ShaderPath);
				}
				uniforms = sc.Uniforms;
			}
		}

		std::vector<Material> materials;
		if (registry.all_of<ModelComponent>(entity))
		{
			auto& modelComponent = registry.get<ModelComponent>(entity);
			bool hasExplicitOverrides = false;
			for (const auto& p : modelComponent.MaterialPaths)
			{
				if (!p.empty())
				{
					hasExplicitOverrides = true;
					break;
				}
			}

			if (hasExplicitOverrides)
			{
				materials = modelAsset->GetMaterials();
				if (auto* materialAssets = ServiceLocator::TryGet<AssetManager>())
				{
					for (size_t i = 0; i < modelComponent.MaterialPaths.size() && i < materials.size(); ++i)
					{
						const auto& matPath = modelComponent.MaterialPaths[i];
						if (!matPath.empty())
						{
							auto materialAsset = materialAssets->Get<MaterialAsset>(matPath);
							if (materialAsset && materialAsset->IsReady())
							{
								materials[i] = materialAsset->GetMaterial();
							}
						}
					}
				}
			}
		}

		RenderItem item;
		item.Asset = modelAsset;
		item.Transform = worldTransform;
		item.Materials = std::move(materials);
		item.ShaderOverride = shaderOver ? shaderOver->GetShader().get() : nullptr;
		item.CustomUniforms = std::move(uniforms);

		if (registry.all_of<AnimationComponent>(entity))
		{
			auto& anim = registry.get<AnimationComponent>(entity);
			if (anim.CurrentAnimationIndex >= 0)
			{
				if (anim.Blending && anim.TargetAnimationIndex >= 0)
				{
					float alpha = (anim.BlendDuration > 0.0f)
									  ? glm::clamp(anim.BlendTimer / anim.BlendDuration, 0.0f, 1.0f)
									  : 1.0f;
					auto matricesA = modelAsset->GetBoneMatrices(anim.CurrentAnimationIndex, anim.CurrentFrame);
					auto matricesB = modelAsset->GetBoneMatrices(anim.TargetAnimationIndex, anim.TargetFrame);

					if (!matricesA.empty() && !matricesB.empty())
					{
						size_t count = glm::min(matricesA.size(), matricesB.size());
						item.BoneMatrices.resize(count);
						for (size_t i = 0; i < count; ++i)
						{
							item.BoneMatrices[i] = InterpolateBoneMatrices(matricesA[i], matricesB[i], alpha);
						}
					}
					else if (!matricesA.empty())
					{
						item.BoneMatrices = std::move(matricesA);
					}
					else
					{
						item.BoneMatrices = std::move(matricesB);
					}
				}
				else
				{
					item.BoneMatrices = modelAsset->GetBoneMatrices(anim.CurrentAnimationIndex, anim.CurrentFrame);
				}
			}
		}

		bool hasOpaque = false;
		bool hasTransparent = false;
		const auto& model = modelAsset->GetModel();
		for (int i = 0; i < (int)model.Meshes.size(); ++i)
		{
			Material mat = m_MaterialManager.Resolve(i, model, materials, modelAsset);
			if (mat.Transparent || mat.AlbedoColor.a < 0.99f)
			{
				hasTransparent = true;
			}
			else
			{
				hasOpaque = true;
			}
		}

		glm::vec3 worldPos = glm::vec3(worldTransform[3]);
		item.Distance = glm::length(cameraPos - worldPos);

		if (hasTransparent)
		{
			m_TransparentQueue.push_back(item);
		}
		if (hasOpaque)
		{
			m_OpaqueQueue.push_back(std::move(item));
		}
		return hasOpaque || hasTransparent;
	}

} // namespace Chained
