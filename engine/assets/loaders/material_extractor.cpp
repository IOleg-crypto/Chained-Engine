#include "engine/assets/loaders/material_extractor.h"
#include "engine/assets/loaders/assimp_helpers.h"

#include "engine/core/profiler.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/service_locator.h"

#include <set>

namespace Chained
{

	void MaterialExtractor::Process(const aiScene* scene, const std::filesystem::path& modelDir,
									std::vector<MaterialData>& materials, std::vector<MeshData>& meshes)
	{
		CH_PROFILE_SCOPE("MaterialExtractor::Process");
		materials.resize(scene->mNumMaterials);
		for (unsigned int i = 0; i < scene->mNumMaterials; ++i)
		{
			ExtractMaterial(scene->mMaterials[i], modelDir, materials[i]);
		}
		RemoveUnreferenced(materials, meshes);
	}

	void MaterialExtractor::ExtractMaterial(aiMaterial* am, const std::filesystem::path& modelDir, MaterialData& out)
	{
		aiString matName;
		if (am->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS)
		{
			out.name = matName.C_Str();
		}

		aiColor4D col(1.0f, 1.0f, 1.0f, 1.0f);
		if (aiGetMaterialColor(am, AI_MATKEY_BASE_COLOR, &col) == AI_SUCCESS)
		{
			out.albedoColor = ToColor(col);
		}
		else if (aiGetMaterialColor(am, AI_MATKEY_COLOR_DIFFUSE, &col) == AI_SUCCESS)
		{
			out.albedoColor = ToColor(col);
		}

		if (out.albedoColor.a < 0.001f)
		{
			out.albedoColor.a = 1.0f;
		}

		float opacity = 1.0f;
		if (aiGetMaterialFloat(am, AI_MATKEY_OPACITY, &opacity) == AI_SUCCESS)
		{
			out.albedoColor.a *= opacity;
		}

		if (aiGetMaterialColor(am, AI_MATKEY_COLOR_EMISSIVE, &col) == AI_SUCCESS)
		{
			out.emissiveColor = ToColor(col);
		}

		aiGetMaterialFloat(am, AI_MATKEY_EMISSIVE_INTENSITY, &out.emissiveIntensity);
		aiGetMaterialFloat(am, AI_MATKEY_METALLIC_FACTOR, &out.metalness);
		aiGetMaterialFloat(am, AI_MATKEY_ROUGHNESS_FACTOR, &out.roughness);

		auto* assetManager = ServiceLocator::TryGet<AssetManager>();
		auto resolvePath = [&](const std::string& texPath) -> std::string {
			if (texPath.empty())
			{
				return "";
			}
			if (assetManager && assetManager->FileExists(texPath))
			{
				return texPath;
			}
			if (std::filesystem::exists(texPath))
			{
				return texPath;
			}
			std::filesystem::path p1 = modelDir / texPath;
			if (assetManager && assetManager->FileExists(p1.string()))
			{
				return p1.string();
			}
			if (std::filesystem::exists(p1))
			{
				return p1.string();
			}
			std::string filename = std::filesystem::path(texPath).filename().string();
			std::filesystem::path p2 = modelDir / filename;
			if (assetManager && assetManager->FileExists(p2.string()))
			{
				return p2.string();
			}
			if (std::filesystem::exists(p2))
			{
				return p2.string();
			}
			std::filesystem::path p3 = modelDir / "textures" / filename;
			if (assetManager && assetManager->FileExists(p3.string()))
			{
				return p3.string();
			}
			if (std::filesystem::exists(p3))
			{
				return p3.string();
			}
			return texPath;
		};

		auto getTex = [&](aiTextureType type) -> std::string {
			aiString str;
			if (am->GetTexture(type, 0, &str) == AI_SUCCESS)
			{
				return resolvePath(str.C_Str());
			}
			return "";
		};

		auto getTexWithFallback = [&](aiTextureType primary, aiTextureType fallback) -> std::string {
			std::string path = getTex(primary);
			return path.empty() ? getTex(fallback) : path;
		};

		out.albedoPath = getTexWithFallback(aiTextureType_DIFFUSE, aiTextureType_BASE_COLOR);
		out.normalPath = getTexWithFallback(aiTextureType_NORMALS, aiTextureType_HEIGHT);
		out.metallicRoughnessPath = getTexWithFallback(aiTextureType_METALNESS, aiTextureType_UNKNOWN);
		out.occlusionPath = getTexWithFallback(aiTextureType_LIGHTMAP, aiTextureType_AMBIENT_OCCLUSION);

		// Only assign emissive texture if there is genuine emissive contribution.
		// Assimp can erroneously map the shared embedded atlas (*0) to aiTextureType_EMISSIVE
		// for glTF voxel/atlas models that have no actual emissive channel, making the
		// Material Editor show the atlas in the Emissive slot and rendering bloom/glow artifacts.
		{
			std::string rawEmissive = getTex(aiTextureType_EMISSIVE);
			bool hasEmissiveColor =
				(out.emissiveColor.r > 0.001f || out.emissiveColor.g > 0.001f || out.emissiveColor.b > 0.001f);
			bool hasEmissiveIntensity = (out.emissiveIntensity > 0.001f);
			bool isEmbedded = (!rawEmissive.empty() && rawEmissive.front() == '*');
			// Allow embedded-atlas emissive only when the material explicitly has emissive color or intensity
			if (!rawEmissive.empty() && (!isEmbedded || hasEmissiveColor || hasEmissiveIntensity))
			{
				out.emissivePath = rawEmissive;
			}
		}

		int blendMode = 0;
		if (aiGetMaterialInteger(am, AI_MATKEY_BLEND_FUNC, &blendMode) == AI_SUCCESS)
		{
			if (blendMode != aiBlendMode_Default && blendMode != aiBlendMode_Additive)
			{
				out.transparent = true;
			}
		}

		if (out.albedoColor.a < 0.999f || opacity < 0.999f)
		{
			out.transparent = true;
		}

		unsigned int isTransparent = 0;
		if (aiGetMaterialInteger(am, "$mat.isTransparent", 0, 0, (int*)&isTransparent) == AI_SUCCESS)
		{
			if (isTransparent)
			{
				out.transparent = true;
			}
		}
	}

	void MaterialExtractor::RemoveUnreferenced(std::vector<MaterialData>& materials, std::vector<MeshData>& meshes)
	{
		if (materials.size() <= 1)
		{
			return;
		}

		std::set<int> usedIndices;
		for (const auto& mesh : meshes)
		{
			usedIndices.insert(mesh.materialIndex);
		}

		std::vector<int> remap(materials.size(), -1);
		std::vector<MaterialData> filtered;
		filtered.reserve(usedIndices.size());

		int newIdx = 0;
		for (int oldIdx = 0; oldIdx < (int)materials.size(); ++oldIdx)
		{
			if (usedIndices.count(oldIdx))
			{
				remap[oldIdx] = newIdx++;
				filtered.push_back(materials[oldIdx]);
			}
		}

		if (filtered.size() < materials.size())
		{
			CH_CORE_INFO("MaterialExtractor: Removed {} unreferenced material(s) ({} -> {})",
						 materials.size() - filtered.size(), materials.size(), filtered.size());

			materials = std::move(filtered);

			for (auto& mesh : meshes)
			{
				if (mesh.materialIndex >= 0 && mesh.materialIndex < (int)remap.size())
				{
					mesh.materialIndex = remap[mesh.materialIndex];
				}
			}
		}
	}

} // namespace Chained
