#include "engine/assets/loaders/assimp_importer.h"
#include "engine/assets/loaders/pack_io.h"
#include "engine/assets/loaders/assimp_helpers.h"
#include "engine/assets/loaders/material_extractor.h"
#include "engine/assets/loaders/animation_sampler.h"
#include "engine/assets/asset_manager.h"

#include "engine/core/profiler.h"
#include "engine/common/thread_pool.h"

#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include "engine/core/service_locator.h"
#include <cstring>

#include <stb_image.h>

namespace Chained
{

	static bool DecodeEmbeddedTexture(const aiTexture* texture, EmbeddedTextureData& out)
	{
		if (!texture)
		{
			return false;
		}

		if (texture->mHeight == 0)
		{
			const unsigned char* bytes = reinterpret_cast<const unsigned char*>(texture->pcData);
			int byteCount = (int)texture->mWidth;
			if (stbi_is_hdr_from_memory(bytes, byteCount))
			{
				CH_CORE_WARN("Assimp embedded HDR texture is not supported by the current texture pipeline. Skipping.");
				return false;
			}

			int width = 0;
			int height = 0;
			int channels = 0;
			unsigned char* decoded = stbi_load_from_memory(bytes, byteCount, &width, &height, &channels, 4);
			if (!decoded)
			{
				return false;
			}

			out.width = width;
			out.height = height;
			out.channels = 4;
			out.isHDR = false;
			out.data.resize((size_t)width * (size_t)height * 4);
			std::memcpy(out.data.data(), decoded, out.data.size());
			stbi_image_free(decoded);
			return true;
		}

		out.width = (int)texture->mWidth;
		out.height = (int)texture->mHeight;
		out.channels = 4;
		out.isHDR = false;
		const size_t pixelCount = (size_t)out.width * (size_t)out.height;
		out.data.resize(pixelCount * 4);
		const aiTexel* src = texture->pcData;
		for (size_t i = 0; i < pixelCount; ++i)
		{
			out.data[i * 4 + 0] = src[i].r;
			out.data[i * 4 + 1] = src[i].g;
			out.data[i * 4 + 2] = src[i].b;
			out.data[i * 4 + 3] = src[i].a;
		}
		return true;
	}

	PendingModelData AssimpImporter::Import(const std::filesystem::path& path, int samplingFPS)
	{
		CH_PROFILE_FUNCTION();
		PendingModelData data{};

		Assimp::Importer importer;
		importer.SetPropertyInteger(AI_CONFIG_PP_SBP_REMOVE, aiPrimitiveType_POINT | aiPrimitiveType_LINE);

		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });

		// Let Assimp merge compatible meshes and improve vertex cache locality during import.
		unsigned int flags = aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace |
							 aiProcess_JoinIdenticalVertices | aiProcess_LimitBoneWeights |
							 aiProcess_ImproveCacheLocality | aiProcess_OptimizeMeshes |
							 aiProcess_RemoveRedundantMaterials | aiProcess_FindInvalidData;

		if (ext != ".gltf" && ext != ".glb")
		{
			flags |= aiProcess_FlipUVs;
		}

		const aiScene* scene = nullptr;

		auto tryLoad = [&](const char* desc, auto loadFn) -> bool {
			try
			{
				scene = loadFn();
				return scene && scene->mRootNode;
			} catch (const std::exception& e)
			{
				CH_CORE_WARN("AssimpImporter: {} threw for '{}': {}", desc, path.string(), e.what());
				return false;
			} catch (...)
			{
				CH_CORE_WARN("AssimpImporter: {} threw for '{}' with unknown exception", desc, path.string());
				return false;
			}
		};

		tryLoad("ReadFile", [&]() { return importer.ReadFile(path.string(), flags); });

		if ((!scene || !scene->mRootNode) && ext == ".gltf")
		{
			auto* am = ServiceLocator::TryGet<AssetManager>();
			if (am && am->IsPacked())
			{
				auto packData = am->ReadAssetData(path.string());
				if (!packData.empty())
				{
					std::string baseDir = path.parent_path().string();
					auto* ioSystem = new PackIOSystem(baseDir, true);
					importer.SetIOHandler(ioSystem);

					tryLoad("ReadFile(PackIO)", [&]() { return importer.ReadFile(path.string(), flags); });

					if (!scene || !scene->mRootNode)
					{
						importer.SetIOHandler(nullptr);
						tryLoad("ReadFileFromMemory", [&]() {
							return importer.ReadFileFromMemory(packData.data(), packData.size(), flags,
															   path.extension().string().c_str());
						});
					}
				}
			}
		}

		if (!scene || !scene->mRootNode)
		{
			CH_CORE_WARN("AssimpImporter: ReadFile failed for '{}', trying memory fallback...", path.string());

			std::vector<char> modelFileData;
			if (auto* am = ServiceLocator::TryGet<AssetManager>())
			{
				if (am->IsPacked())
				{
					auto packData = am->ReadAssetData(path.string());
					if (packData.empty())
					{
						std::string pStr = path.generic_string();
						auto pos = pStr.find("assets/");
						if (pos != std::string::npos)
						{
							packData = am->ReadAssetData(pStr.substr(pos));
						}
					}
					if (!packData.empty())
					{
						modelFileData.assign(packData.begin(), packData.end());
					}
				}
			}

			if (modelFileData.empty())
			{
				std::ifstream file(path, std::ios::binary | std::ios::ate);
				if (file.is_open())
				{
					std::streamsize size = file.tellg();
					if (size > 0)
					{
						file.seekg(0);
						modelFileData.resize(static_cast<size_t>(size), 0);
						file.read(modelFileData.data(), size);
					}
				}
			}

			if (!modelFileData.empty())
			{
				tryLoad("ReadFileFromMemory", [&]() {
					return importer.ReadFileFromMemory(modelFileData.data(), modelFileData.size(), flags,
													   path.extension().string().c_str());
				});
			}
		}

		if (!scene || !scene->mRootNode)
		{
			CH_CORE_ERROR("Assimp Model Load Failed: {} | Error: {}", path.filename().string(),
						  importer.GetErrorString());
			return data;
		}

		AssimpImporter instance(path, samplingFPS, scene);
		return instance.Execute();
	}

	AssimpImporter::AssimpImporter(const std::filesystem::path& path, int samplingFPS, const aiScene* scene)
		: m_Path(path),
		  m_SamplingFPS(samplingFPS),
		  m_Scene(scene)
	{
		m_ModelDir = path.parent_path();
	}

	PendingModelData AssimpImporter::Execute()
	{
		ProcessHierarchy();
		ProcessMeshes();
		DecodeEmbeddedTextures();

		MaterialExtractor::Process(m_Scene, m_ModelDir, m_Data.materials, m_Data.meshes);
		AnimationSampler::Process(m_Scene, m_SamplingFPS, m_Data.nodeNames, m_NameToIndex, m_Data.animations);
		m_Data.isValid = true;
		return std::move(m_Data);
	}

	void AssimpImporter::ProcessNode(aiNode* node, int parentIdx)
	{
		if (!node)
		{
			return;
		}

		int currentIdx = (int)m_Data.nodeNames.size();
		m_Data.nodeNames.push_back(node->mName.C_Str());
		m_Data.nodeParents.push_back(parentIdx);

		glm::mat4 local = ToMat4(node->mTransformation);
		m_Data.nodeLocalTransforms.push_back(local);
		m_Data.globalBindPoses.push_back((parentIdx == -1) ? local : m_Data.globalBindPoses[parentIdx] * local);

		for (unsigned int i = 0; i < node->mNumMeshes; ++i)
		{
			unsigned int meshIndex = node->mMeshes[i];
			if (meshIndex < m_Scene->mNumMeshes)
			{
				m_Data.instances.push_back({(int)meshIndex, m_Data.globalBindPoses[currentIdx]});
			}
		}

		for (unsigned int i = 0; i < node->mNumChildren; ++i)
		{
			ProcessNode(node->mChildren[i], currentIdx);
		}
	}

	void AssimpImporter::ProcessHierarchy()
	{
		CH_PROFILE_SCOPE("AssimpImporter::ProcessHierarchy");
		ProcessNode(m_Scene->mRootNode, -1);

		m_Data.nodeCount = (int)m_Data.nodeNames.size();
		m_NameToIndex.reserve(m_Data.nodeNames.size());
		for (int i = 0; i < (int)m_Data.nodeNames.size(); ++i)
		{
			m_NameToIndex[m_Data.nodeNames[i]] = i;
		}
	}

	void AssimpImporter::ProcessSingleMesh(uint32_t m)
	{
		aiMesh* am = m_Scene->mMeshes[m];
		MeshData rm;
		rm.materialIndex = am->mMaterialIndex;

		rm.vertices.reserve((size_t)am->mNumVertices * 3);
		if (am->mTextureCoords[0])
		{
			rm.texcoords.reserve((size_t)am->mNumVertices * 2);
		}
		if (am->mNormals)
		{
			rm.normals.reserve((size_t)am->mNumVertices * 3);
		}
		if (am->mTangents)
		{
			rm.tangents.reserve((size_t)am->mNumVertices * 3);
		}
		if (am->mColors[0])
		{
			rm.colors.reserve((size_t)am->mNumVertices * 4);
		}
		rm.indices.reserve((size_t)am->mNumFaces * 3);

		for (unsigned int v = 0; v < am->mNumVertices; ++v)
		{
			rm.vertices.insert(rm.vertices.end(), {am->mVertices[v].x, am->mVertices[v].y, am->mVertices[v].z});
			if (am->mTextureCoords[0])
			{
				rm.texcoords.insert(rm.texcoords.end(), {am->mTextureCoords[0][v].x, am->mTextureCoords[0][v].y});
			}
			if (am->mNormals)
			{
				rm.normals.insert(rm.normals.end(), {am->mNormals[v].x, am->mNormals[v].y, am->mNormals[v].z});
			}
			if (am->mTangents)
			{
				rm.tangents.insert(rm.tangents.end(), {am->mTangents[v].x, am->mTangents[v].y, am->mTangents[v].z});
			}
			if (am->mColors[0])
			{
				rm.colors.insert(rm.colors.end(), {(unsigned char)(am->mColors[0][v].r * 255.0f),
												   (unsigned char)(am->mColors[0][v].g * 255.0f),
												   (unsigned char)(am->mColors[0][v].b * 255.0f),
												   (unsigned char)(am->mColors[0][v].a * 255.0f)});
			}
		}

		for (unsigned int f = 0; f < am->mNumFaces; ++f)
		{
			const aiFace& face = am->mFaces[f];
			if (face.mNumIndices != 3)
			{
				continue;
			}
			rm.indices.insert(rm.indices.end(), {face.mIndices[0], face.mIndices[1], face.mIndices[2]});
		}

		rm.MinBounds = {FLT_MAX, FLT_MAX, FLT_MAX};
		rm.MaxBounds = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
		for (size_t v = 0; v < rm.vertices.size(); v += 3)
		{
			glm::vec3 pos = {rm.vertices[v], rm.vertices[v + 1], rm.vertices[v + 2]};
			rm.MinBounds = glm::min(rm.MinBounds, pos);
			rm.MaxBounds = glm::max(rm.MaxBounds, pos);
		}

		if (am->mNumBones > 0)
		{
			rm.joints.resize((size_t)am->mNumVertices * 4, 0);
			rm.weights.resize((size_t)am->mNumVertices * 4, 0.0f);
			std::vector<int> jointCounts(am->mNumVertices, 0);
			auto& offsetWrites = m_MeshOffsetWrites[m];
			offsetWrites.reserve(am->mNumBones);

			for (unsigned int b = 0; b < am->mNumBones; ++b)
			{
				aiBone* bone = am->mBones[b];
				auto boneIt = m_NameToIndex.find(bone->mName.C_Str());
				if (boneIt == m_NameToIndex.end())
				{
					continue;
				}

				const int boneIdx = boneIt->second;
				offsetWrites.emplace_back(boneIdx, ToMat4(bone->mOffsetMatrix));

				if (boneIdx > 255)
				{
					CH_CORE_WARN("AssimpImporter: Bone '{}' node index {} exceeds uint8 max — vertex weights skipped",
								 bone->mName.C_Str(), boneIdx);
					continue;
				}

				for (unsigned int w = 0; w < bone->mNumWeights; ++w)
				{
					const int vIdx = bone->mWeights[w].mVertexId;
					if (vIdx < 0 || vIdx >= (int)am->mNumVertices)
					{
						continue;
					}

					if (jointCounts[vIdx] < 4)
					{
						const int slot = vIdx * 4 + jointCounts[vIdx]++;
						rm.joints[slot] = (unsigned char)boneIdx;
						rm.weights[slot] = bone->mWeights[w].mWeight;
					}
				}
			}
		}
		m_Data.meshes[m] = std::move(rm);
	}

	void AssimpImporter::ProcessMeshes()
	{
		CH_PROFILE_SCOPE("AssimpImporter::ProcessMeshes");
		m_Data.meshes.resize(m_Scene->mNumMeshes);
		m_MeshOffsetWrites.resize(m_Scene->mNumMeshes);

		if (m_Scene->mNumMeshes == 0)
		{
			return;
		}
		else if (m_Scene->mNumMeshes == 1)
		{
			ProcessSingleMesh(0);
		}
		else
		{
			std::vector<std::future<void>> futures;
			futures.reserve(m_Scene->mNumMeshes);
			for (uint32_t m = 0; m < m_Scene->mNumMeshes; ++m)
			{
				if (auto* tp = ServiceLocator::TryGet<ThreadPool>())
				{
					futures.push_back(tp->Enqueue([this, m]() { ProcessSingleMesh(m); }));
				}
			}
			for (auto& ft : futures)
			{
				ft.wait();
			}
		}

		m_Data.offsetMatrices.assign(m_Data.nodeNames.size(), glm::mat4(1.0f));
		for (const auto& meshOffsets : m_MeshOffsetWrites)
		{
			for (const auto& [boneIdx, offset] : meshOffsets)
			{
				if (boneIdx >= 0 && boneIdx < (int)m_Data.offsetMatrices.size())
				{
					m_Data.offsetMatrices[boneIdx] = offset;
				}
			}
		}
	}

	void AssimpImporter::DecodeEmbeddedTextures()
	{
		CH_PROFILE_SCOPE("AssimpImporter::DecodeEmbeddedTextures");
		for (unsigned int i = 0; i < m_Scene->mNumTextures; ++i)
		{
			const aiTexture* texture = m_Scene->mTextures[i];
			if (!texture)
			{
				continue;
			}

			EmbeddedTextureData embedded;
			if (!DecodeEmbeddedTexture(texture, embedded))
			{
				continue;
			}

			m_Data.embeddedTextures.emplace("*" + std::to_string(i), std::move(embedded));
		}
	}

} // namespace Chained
