#include "engine/graphics/pipeline/material_manager.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/types/texture_asset.h"
#include "engine/core/service_locator.h"
#include "engine/graphics/api/graphics_device.h"
#include "engine/graphics/api/shader.h"
#include "engine/assets/types/model_asset.h"

namespace Chained
{

	Material MaterialManager::Resolve(int meshIndex, const Model& model, const std::vector<Material>& materials,
									  ModelAsset* modelAsset) const
	{
		if (meshIndex < 0 || meshIndex >= (int)model.Meshes.size())
		{
			return {};
		}

		int matIdx = model.Meshes[meshIndex].MaterialIndex;

		// Tier 1: Caller-supplied materials vector
		if (matIdx >= 0 && matIdx < (int)materials.size())
		{
			return materials[matIdx];
		}

		// Tier 2: ModelAsset embedded materials
		if (modelAsset && modelAsset->IsReady())
		{
			const auto& assetMaterials = modelAsset->GetMaterials();
			if (matIdx >= 0 && matIdx < (int)assetMaterials.size())
			{
				return assetMaterials[matIdx];
			}
		}

		// Tier 3: Model struct embedded materials
		if (matIdx < 0 || matIdx >= (int)model.Materials.size())
		{
			return Material();
		}

		return model.Materials[matIdx];
	}

	void MaterialManager::Bind(Shader* shader, const Material& material, int meshIndex, const Model& model) const
	{
		if (!shader)
		{
			return;
		}

		shader->Bind();

		auto resolveMap = [](const std::shared_ptr<Texture>& currentTex, const std::string& path) -> uint32_t {
			if (currentTex)
			{
				return currentTex->GetNativeHandle();
			}
			if (path.empty() || path.front() == '*')
			{
				return 0;
			}

			auto* am = ServiceLocator::TryGet<AssetManager>();
			auto texAsset = am ? am->Get<TextureAsset>(path) : nullptr;
			if (texAsset && texAsset->GetTexture())
			{
				return texAsset->GetTexture()->GetNativeHandle();
			}
			return 0;
		};

		uint32_t albedoMap = resolveMap(material.AlbedoMap, material.AlbedoPath);
		uint32_t normalMap = resolveMap(material.NormalMap, material.NormalPath);
		uint32_t metallicMap = resolveMap(material.MetallicRoughnessMap, material.MetallicRoughnessPath);
		uint32_t emissiveMap = resolveMap(material.EmissiveMap, material.EmissivePath);
		uint32_t occlusionMap = resolveMap(material.OcclusionMap, material.OcclusionPath);

		// 1. Albedo (Texture Unit 0)
		if (albedoMap > 0)
		{
			GraphicsDevice::Get().SetTexture(0, albedoMap);
			shader->SetInt("texture0", 0);
			shader->SetInt("useTexture", 1);
		}
		else
		{
			shader->SetInt("useTexture", 0);
		}
		shader->SetVec4("colDiffuse", material.AlbedoColor);

		// 2. Metallic-Roughness Packed Map (Texture Unit 1)
		if (metallicMap > 0)
		{
			GraphicsDevice::Get().SetTexture(1, metallicMap);
			shader->SetInt("texture1", 1);
			shader->SetInt("useMetallicMap", 1);
			shader->SetInt("useRoughnessMap", 1);
		}
		else
		{
			shader->SetInt("useMetallicMap", 0);
			shader->SetInt("useRoughnessMap", 0);
		}

		// 3. Normal Map (Texture Unit 2)
		if (normalMap > 0)
		{
			GraphicsDevice::Get().SetTexture(2, normalMap);
			shader->SetInt("texture2", 2);
			shader->SetInt("useNormalMap", 1);
		}
		else
		{
			shader->SetInt("useNormalMap", 0);
		}

		// 4. Occlusion Map (Texture Unit 4)
		if (occlusionMap > 0)
		{
			GraphicsDevice::Get().SetTexture(4, occlusionMap);
			shader->SetInt("texture4", 4);
			shader->SetInt("useOcclusionMap", 1);
		}
		else
		{
			shader->SetInt("useOcclusionMap", 0);
		}

		// 5. Emissive Map (Texture Unit 5)
		if (emissiveMap > 0)
		{
			GraphicsDevice::Get().SetTexture(5, emissiveMap);
			shader->SetInt("texture5", 5);
			shader->SetInt("useEmissiveTexture", 1);
		}
		else
		{
			shader->SetInt("useEmissiveTexture", 0);
		}

		shader->SetFloat("metalness", material.Metalness);
		shader->SetFloat("roughness", material.Roughness);
		shader->SetVec4("colEmissive", material.EmissiveColor);
		shader->SetFloat("emissiveIntensity", material.EmissiveIntensity);
	}

} // namespace Chained
