#include "engine/assets/types/model_asset.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/types/texture_asset.h"

#include "engine/core/profiler.h"
#include "engine/core/service_locator.h"
#include "engine/project/project.h"
#include <cstring>
#include <unordered_map>
#include <glm/gtc/matrix_inverse.hpp>
#include <stb_image.h>

namespace Chained
{
	std::string ModelAsset::GetAnimationName(int index) const
	{
		return (index >= 0 && index < (int)m_Animations.size()) ? m_Animations[index].name : "";
	}

	std::vector<glm::mat4> ModelAsset::GetBoneMatrices(int animationIndex, int frame) const
	{
		if (animationIndex < 0 || animationIndex >= (int)m_Animations.size())
		{
			return {};
		}
		const auto& anim = m_Animations[animationIndex];
		if (frame < 0 || frame >= anim.frameCount)
		{
			return {};
		}

		int boneCount = anim.boneCount;
		if (boneCount == 0)
		{
			return {};
		}

		std::vector<glm::mat4> globalTransforms(boneCount);
		std::vector<glm::mat4> finalMatrices;
		finalMatrices.reserve(boneCount);

		for (int boneIndex = 0; boneIndex < boneCount; ++boneIndex)
		{
			const auto& pose = anim.framePoses[frame * boneCount + boneIndex];

			glm::mat4 local = glm::translate(glm::mat4(1.0f), pose.translation) * glm::mat4_cast(pose.rotation) *
							  glm::scale(glm::mat4(1.0f), pose.scale);

			if (m_NodeParents[boneIndex] == -1)
			{
				globalTransforms[boneIndex] = local;
			}
			else
			{
				globalTransforms[boneIndex] = globalTransforms[m_NodeParents[boneIndex]] * local;
			}

			glm::mat4 offset =
				(boneIndex < (int)m_OffsetMatrices.size()) ? m_OffsetMatrices[boneIndex] : glm::mat4(1.0f);
			finalMatrices.push_back(globalTransforms[boneIndex] * offset);
		}

		return finalMatrices;
	}

	void ModelAsset::OnLoaded()
	{
		CH_PROFILE_FUNCTION();

		if (!m_PendingData.isValid)
		{
			return;
		}

		CH_CORE_INFO("ModelAsset: Uploading model to GPU: '{}' ({} meshes, {} materials)", GetPath(),
					 static_cast<uint32_t>(m_PendingData.meshes.size()),
					 static_cast<uint32_t>(m_PendingData.materials.size()));

		Model newModel;
		newModel.Materials.resize(m_PendingData.materials.empty() ? 1 : m_PendingData.materials.size());

		m_EmbeddedTextures.clear();
		if (!m_PendingData.embeddedTextures.empty())
		{
			size_t maxTexIndex = 0;
			for (const auto& [name, data] : m_PendingData.embeddedTextures)
			{
				if (!name.empty() && name.front() == '*')
				{
					try
					{
						size_t idx = std::stoul(name.substr(1));
						if (idx > maxTexIndex)
						{
							maxTexIndex = idx;
						}
					} catch (...)
					{
						CH_CORE_ERROR("ModelAsset: Invalid embedded texture key: {0}", name);
					}
				}
			}
			m_EmbeddedTextures.resize(maxTexIndex + 1);

			for (const auto& [name, embedded] : m_PendingData.embeddedTextures)
			{
				if (name.empty() || name.front() != '*')
				{
					continue;
				}

				try
				{
					size_t idx = std::stoul(name.substr(1));

					if (embedded.data.empty() || embedded.isHDR)
					{
						continue;
					}

					if (embedded.width > 0 && embedded.height > 0)
					{
						// Legacy uncompressed RGBA8 data
						auto texture =
							Texture::Create((uint32_t)embedded.width, (uint32_t)embedded.height, TextureFormat::RGBA8);
						if (texture)
						{
							texture->SetData((void*)embedded.data.data(), 0);
							m_EmbeddedTextures[idx] = texture;
						}
					}
					else
					{
						// Optimized compressed PNG/JPEG buffer: decode on the fly for GPU upload
						int w = 0, h = 0, ch = 0;
						unsigned char* pixels = stbi_load_from_memory(
							embedded.data.data(), static_cast<int>(embedded.data.size()), &w, &h, &ch, 4);
						if (pixels && w > 0 && h > 0)
						{
							auto texture = Texture::Create(static_cast<uint32_t>(w), static_cast<uint32_t>(h),
														   TextureFormat::RGBA8);
							if (texture)
							{
								texture->SetData((void*)pixels, 0);
								m_EmbeddedTextures[idx] = texture;
							}
							stbi_image_free(pixels);
						}
					}
				} catch (...)
				{
					CH_CORE_ERROR("ModelAsset: Failed to process embedded texture key: {0}", name);
				}
			}
		}

		auto project = Project::GetActive();

		auto loadTex = [&](int matIdx, const std::string& path, int mapIndex) {
			if (path.empty())
			{
				return;
			}

			if (!path.empty() && path.front() == '*')
			{
				try
				{
					size_t texIdx = std::stoul(path.substr(1));
					if (texIdx < m_EmbeddedTextures.size() && m_EmbeddedTextures[texIdx])
					{
						std::shared_ptr<Texture> tex = m_EmbeddedTextures[texIdx];
						switch (mapIndex)
						{
						case 0:
							newModel.Materials[matIdx].AlbedoMap = tex;
							break;
						case 2:
							newModel.Materials[matIdx].NormalMap = tex;
							break;
						case 3:
							newModel.Materials[matIdx].MetallicRoughnessMap = tex;
							break;
						case 4:
							newModel.Materials[matIdx].EmissiveMap = tex;
							break;
						case 5:
							newModel.Materials[matIdx].OcclusionMap = tex;
							break;
						}
					}
				} catch (...)
				{
					CH_CORE_ERROR("ModelAsset: Invalid embedded texture path reference: {0}", path);
				}
				return;
			}

			if (!project)
			{
				return;
			}

			auto* am = ServiceLocator::TryGet<AssetManager>();
			if (!am)
			{
				return;
			}

			auto tex = am->Get<TextureAsset>(path);
			if (!tex)
			{
				return;
			}

			std::shared_ptr<Texture> texPtr;
			if (tex->IsReady() && tex->GetTexture())
			{
				texPtr = tex->GetTexture();
			}

			switch (mapIndex)
			{
			case 0:
				newModel.Materials[matIdx].AlbedoMap = texPtr;
				break;
			case 2:
				newModel.Materials[matIdx].NormalMap = texPtr;
				break;
			case 3:
				newModel.Materials[matIdx].MetallicRoughnessMap = texPtr;
				break;
			case 4:
				newModel.Materials[matIdx].EmissiveMap = texPtr;
				break;
			case 5:
				newModel.Materials[matIdx].OcclusionMap = texPtr;
				break;
			}
		};

		for (int materialIndex = 0; materialIndex < (int)newModel.Materials.size(); ++materialIndex)
		{
			if (!m_PendingData.materials.empty())
			{
				const auto& rawMaterial = m_PendingData.materials[materialIndex];
				newModel.Materials[materialIndex].AlbedoColor = rawMaterial.albedoColor;
				newModel.Materials[materialIndex].EmissiveColor = rawMaterial.emissiveColor;
				newModel.Materials[materialIndex].EmissiveIntensity = rawMaterial.emissiveIntensity;
				newModel.Materials[materialIndex].Metalness = rawMaterial.metalness;
				newModel.Materials[materialIndex].Roughness = rawMaterial.roughness;
				newModel.Materials[materialIndex].Transparent = rawMaterial.transparent;
				newModel.Materials[materialIndex].Alpha = rawMaterial.albedoColor.a;
				newModel.Materials[materialIndex].Name = rawMaterial.name;

				loadTex(materialIndex, rawMaterial.albedoPath, 0);
				if (!rawMaterial.albedoPath.empty() && rawMaterial.albedoPath.front() != '*')
				{
					newModel.Materials[materialIndex].AlbedoPath = rawMaterial.albedoPath;
				}

				loadTex(materialIndex, rawMaterial.normalPath, 2);
				if (!rawMaterial.normalPath.empty() && rawMaterial.normalPath.front() != '*')
				{
					newModel.Materials[materialIndex].NormalPath = rawMaterial.normalPath;
				}

				loadTex(materialIndex, rawMaterial.occlusionPath, 5);
				if (!rawMaterial.occlusionPath.empty() && rawMaterial.occlusionPath.front() != '*')
				{
					newModel.Materials[materialIndex].OcclusionPath = rawMaterial.occlusionPath;
				}

				loadTex(materialIndex, rawMaterial.emissivePath, 4);
				if (!rawMaterial.emissivePath.empty() && rawMaterial.emissivePath.front() != '*')
				{
					newModel.Materials[materialIndex].EmissivePath = rawMaterial.emissivePath;
				}

				loadTex(materialIndex, rawMaterial.metallicRoughnessPath, 3);
				if (!rawMaterial.metallicRoughnessPath.empty() && rawMaterial.metallicRoughnessPath.front() != '*')
				{
					newModel.Materials[materialIndex].MetallicRoughnessPath = rawMaterial.metallicRoughnessPath;
				}
			}
		}

		for (int meshIndex = 0; meshIndex < (int)m_PendingData.meshes.size(); ++meshIndex)
		{
			const auto& rawMesh = m_PendingData.meshes[meshIndex];
			Mesh mesh;
			mesh.VertexCount = (uint32_t)rawMesh.vertices.size() / 3;
			mesh.TriangleCount = (uint32_t)rawMesh.indices.size() / 3;
			mesh.MaterialIndex = (rawMesh.materialIndex >= 0 && rawMesh.materialIndex < (int)newModel.Materials.size())
									 ? rawMesh.materialIndex
									 : 0;
			mesh.HasSkinning = !rawMesh.joints.empty() && !rawMesh.weights.empty();

			if (mesh.VertexCount > 0)
			{
				mesh.VAO = VertexArray::Create();

				if (mesh.VAO)
				{
					auto vboPos = VertexBuffer::Create(rawMesh.vertices.data(),
													   (uint32_t)rawMesh.vertices.size() * sizeof(float));
					vboPos->SetLayout({{VertexAttributeType::Float3, "a_Position"}});
					mesh.VAO->AddVertexBuffer(vboPos);

					if (!rawMesh.texcoords.empty())
					{
						auto vboTex = VertexBuffer::Create(rawMesh.texcoords.data(),
														   (uint32_t)rawMesh.texcoords.size() * sizeof(float));
						vboTex->SetLayout({{VertexAttributeType::Float2, "a_TexCoord"}});
						mesh.VAO->AddVertexBuffer(vboTex);
					}

					if (!rawMesh.normals.empty())
					{
						auto vboNorm = VertexBuffer::Create(rawMesh.normals.data(),
															(uint32_t)rawMesh.normals.size() * sizeof(float));
						vboNorm->SetLayout({{VertexAttributeType::Float3, "a_Normal"}});
						mesh.VAO->AddVertexBuffer(vboNorm);
					}

					if (!rawMesh.joints.empty())
					{
						std::vector<int32_t> jointsInt;
						jointsInt.reserve(rawMesh.joints.size());
						for (auto jointId : rawMesh.joints)
						{
							jointsInt.push_back(static_cast<int32_t>(jointId));
						}

						auto vboJoints = VertexBuffer::Create((float*)jointsInt.data(),
															  (uint32_t)jointsInt.size() * sizeof(int32_t));
						vboJoints->SetLayout({{VertexAttributeType::Int4, "a_JointIDs"}});
						mesh.VAO->AddVertexBuffer(vboJoints);
					}

					if (!rawMesh.weights.empty())
					{
						auto vboWeights = VertexBuffer::Create(rawMesh.weights.data(),
															   (uint32_t)rawMesh.weights.size() * sizeof(float));
						vboWeights->SetLayout({{VertexAttributeType::Float4, "a_Weights"}});
						mesh.VAO->AddVertexBuffer(vboWeights);
					}

					if (!rawMesh.indices.empty())
					{
						auto ibo = IndexBuffer::Create(rawMesh.indices.data(), (uint32_t)rawMesh.indices.size());
						mesh.VAO->SetIndexBuffer(ibo);
					}
				}

				mesh.MinBounds = {FLT_MAX, FLT_MAX, FLT_MAX};
				mesh.MaxBounds = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
				for (size_t vertexOffset = 0; vertexOffset < rawMesh.vertices.size(); vertexOffset += 3)
				{
					mesh.MinBounds =
						glm::min(mesh.MinBounds, {rawMesh.vertices[vertexOffset], rawMesh.vertices[vertexOffset + 1],
												  rawMesh.vertices[vertexOffset + 2]});
					mesh.MaxBounds =
						glm::max(mesh.MaxBounds, {rawMesh.vertices[vertexOffset], rawMesh.vertices[vertexOffset + 1],
												  rawMesh.vertices[vertexOffset + 2]});
				}
			}
			newModel.Meshes.push_back(mesh);
		}

		BoundingBox totalBox = {{FLT_MAX, FLT_MAX, FLT_MAX}, {-FLT_MAX, -FLT_MAX, -FLT_MAX}};
		bool anyMesh = false;
		for (const auto& inst : m_PendingData.instances)
		{
			if (inst.meshIndex < 0 || inst.meshIndex >= (int)newModel.Meshes.size())
			{
				continue;
			}

			const Mesh& mesh = newModel.Meshes[inst.meshIndex];
			glm::vec3 corners[8] = {{mesh.MinBounds.x, mesh.MinBounds.y, mesh.MinBounds.z},
									{mesh.MaxBounds.x, mesh.MinBounds.y, mesh.MinBounds.z},
									{mesh.MinBounds.x, mesh.MaxBounds.y, mesh.MinBounds.z},
									{mesh.MaxBounds.x, mesh.MaxBounds.y, mesh.MinBounds.z},
									{mesh.MinBounds.x, mesh.MinBounds.y, mesh.MaxBounds.z},
									{mesh.MaxBounds.x, mesh.MinBounds.y, mesh.MaxBounds.z},
									{mesh.MinBounds.x, mesh.MaxBounds.y, mesh.MaxBounds.z},
									{mesh.MaxBounds.x, mesh.MaxBounds.y, mesh.MaxBounds.z}};

			for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex)
			{
				glm::vec4 transformed = inst.localTransform * glm::vec4(corners[cornerIndex], 1.0f);
				totalBox.Min = glm::min(totalBox.Min, glm::vec3(transformed));
				totalBox.Max = glm::max(totalBox.Max, glm::vec3(transformed));
			}
			anyMesh = true;
		}

		if (!anyMesh)
		{
			totalBox = {{0, 0, 0}, {0, 0, 0}};
		}

		m_Model = std::move(newModel);
		m_Materials = m_Model.Materials;
		m_BoundingBox = totalBox;
		m_Meshes = std::move(m_PendingData.meshes);
		m_Animations = std::move(m_PendingData.animations);
		m_Instances = std::move(m_PendingData.instances);
		m_OffsetMatrices = std::move(m_PendingData.offsetMatrices);
		m_NodeNames = std::move(m_PendingData.nodeNames);
		m_NodeParents = std::move(m_PendingData.nodeParents);

		// --- Static submesh merging by material ---
		// For models with no skeletal animation, merge all MeshInstances that share the
		// same materialIndex into a single GPU mesh. This collapses N-node models (e.g. a
		// rail with 26 Blender objects all sharing one material) into M draw calls where
		// M == number of unique materials. Skinned meshes are left untouched.
		if (m_Animations.empty())
		{
			MergeStaticMeshesByMaterial();
		}

		m_PendingData = PendingModelData();
		SetState(AssetState::Ready);
	}

	uint32_t ModelAsset::GetEmbeddedTextureID(const std::string& path) const
	{
		if (path.empty() || path.front() != '*')
		{
			return 0;
		}
		try
		{
			size_t index = std::stoul(path.substr(1));
			if (index < m_EmbeddedTextures.size() && m_EmbeddedTextures[index])
			{
				return m_EmbeddedTextures[index]->GetNativeHandle();
			}
		} catch (const std::exception&)
		{
			CH_CORE_ERROR("ModelAsset: Embedded texture ID not found for path '{0}'", path);
			return 0;
		}
		return 0;
	}

	void ModelAsset::MergeStaticMeshesByMaterial()
	{
		// Build a map: materialIndex → list of (meshIndex, localTransform) from m_Instances
		// Skip any instance whose source mesh has skinning data.
		std::unordered_map<int, std::vector<size_t>> byMaterial; // materialIndex → instance indices
		for (size_t i = 0; i < m_Instances.size(); ++i)
		{
			int mi = m_Instances[i].meshIndex;
			if (mi < 0 || mi >= (int)m_Meshes.size())
			{
				continue;
			}
			// Leave skinned meshes untouched
			if (!m_Meshes[mi].joints.empty())
			{
				continue;
			}
			int matIdx = m_Meshes[mi].materialIndex;
			byMaterial[matIdx].push_back(i);
		}

		if (byMaterial.empty())
		{
			return;
		}

		// Collect instances we could NOT merge (skinned)
		std::vector<MeshInstance> survivingInstances;
		std::unordered_map<size_t, bool> mergedSet;
		for (const auto& [matIdx, instIndices] : byMaterial)
		{
			for (size_t idx : instIndices)
			{
				mergedSet[idx] = true;
			}
		}
		for (size_t i = 0; i < m_Instances.size(); ++i)
		{
			if (mergedSet.find(i) == mergedSet.end())
			{
				survivingInstances.push_back(m_Instances[i]);
			}
		}

		// For each material group, merge all source meshes into one
		for (const auto& [matIdx, instIndices] : byMaterial)
		{
			if (instIndices.size() == 1)
			{
				// Only one instance for this material — keep as-is, no merge needed
				survivingInstances.push_back(m_Instances[instIndices[0]]);
				continue;
			}

			// Accumulate merged vertex/index buffers (positions already in local space)
			std::vector<float> mergedVerts, mergedTexcoords, mergedNormals, mergedTangents;
			std::vector<uint32_t> mergedIndices;
			uint32_t vertexBase = 0;

			bool hasTexcoords = false, hasNormals = false, hasTangents = false;
			// Pre-check what attributes exist across source meshes
			for (size_t idx : instIndices)
			{
				int mi = m_Instances[idx].meshIndex;
				if (!m_Meshes[mi].texcoords.empty())
				{
					hasTexcoords = true;
				}
				if (!m_Meshes[mi].normals.empty())
				{
					hasNormals = true;
				}
				if (!m_Meshes[mi].tangents.empty())
				{
					hasTangents = true;
				}
			}

			for (size_t idx : instIndices)
			{
				int mi = m_Instances[idx].meshIndex;
				const MeshData& src = m_Meshes[mi];
				const glm::mat4& T = m_Instances[idx].localTransform;
				glm::mat4 normalMat = glm::transpose(glm::inverse(T));

				uint32_t srcVertCount = (uint32_t)src.vertices.size() / 3;

				// Transform positions
				for (uint32_t v = 0; v < srcVertCount; ++v)
				{
					glm::vec4 pos(src.vertices[v * 3], src.vertices[v * 3 + 1], src.vertices[v * 3 + 2], 1.0f);
					glm::vec4 tpos = T * pos;
					mergedVerts.push_back(tpos.x);
					mergedVerts.push_back(tpos.y);
					mergedVerts.push_back(tpos.z);
				}

				// Transform normals
				if (hasNormals)
				{
					for (uint32_t v = 0; v < srcVertCount; ++v)
					{
						if (v * 3 + 2 < src.normals.size())
						{
							glm::vec4 n(src.normals[v * 3], src.normals[v * 3 + 1], src.normals[v * 3 + 2], 0.0f);
							glm::vec4 tn = normalMat * n;
							mergedNormals.push_back(tn.x);
							mergedNormals.push_back(tn.y);
							mergedNormals.push_back(tn.z);
						}
						else
						{
							mergedNormals.push_back(0.f);
							mergedNormals.push_back(1.f);
							mergedNormals.push_back(0.f);
						}
					}
				}

				// Transform tangents
				if (hasTangents)
				{
					for (uint32_t v = 0; v < srcVertCount; ++v)
					{
						if (v * 3 + 2 < src.tangents.size())
						{
							glm::vec4 t(src.tangents[v * 3], src.tangents[v * 3 + 1], src.tangents[v * 3 + 2], 0.0f);
							glm::vec4 tt = T * t;
							mergedTangents.push_back(tt.x);
							mergedTangents.push_back(tt.y);
							mergedTangents.push_back(tt.z);
						}
						else
						{
							mergedTangents.push_back(1.f);
							mergedTangents.push_back(0.f);
							mergedTangents.push_back(0.f);
						}
					}
				}

				// Copy UVs as-is (not affected by world transform)
				if (hasTexcoords)
				{
					for (uint32_t v = 0; v < srcVertCount; ++v)
					{
						if (v * 2 + 1 < src.texcoords.size())
						{
							mergedTexcoords.push_back(src.texcoords[v * 2]);
							mergedTexcoords.push_back(src.texcoords[v * 2 + 1]);
						}
						else
						{
							mergedTexcoords.push_back(0.f);
							mergedTexcoords.push_back(0.f);
						}
					}
				}

				// Remap indices
				for (uint32_t index : src.indices)
				{
					mergedIndices.push_back(vertexBase + index);
				}
				vertexBase += srcVertCount;
			}

			if (mergedVerts.empty() || mergedIndices.empty())
			{
				continue;
			}

			// Build a new GPU Mesh from merged data
			Mesh mergedMesh;
			mergedMesh.VertexCount = vertexBase;
			mergedMesh.TriangleCount = (uint32_t)mergedIndices.size() / 3;
			mergedMesh.MaterialIndex = matIdx;
			mergedMesh.HasSkinning = false;

			mergedMesh.VAO = VertexArray::Create();
			if (mergedMesh.VAO)
			{
				auto vboPos = VertexBuffer::Create(mergedVerts.data(), (uint32_t)mergedVerts.size() * sizeof(float));
				vboPos->SetLayout({{VertexAttributeType::Float3, "a_Position"}});
				mergedMesh.VAO->AddVertexBuffer(vboPos);

				if (hasTexcoords && !mergedTexcoords.empty())
				{
					auto vboTex =
						VertexBuffer::Create(mergedTexcoords.data(), (uint32_t)mergedTexcoords.size() * sizeof(float));
					vboTex->SetLayout({{VertexAttributeType::Float2, "a_TexCoord"}});
					mergedMesh.VAO->AddVertexBuffer(vboTex);
				}
				if (hasNormals && !mergedNormals.empty())
				{
					auto vboNorm =
						VertexBuffer::Create(mergedNormals.data(), (uint32_t)mergedNormals.size() * sizeof(float));
					vboNorm->SetLayout({{VertexAttributeType::Float3, "a_Normal"}});
					mergedMesh.VAO->AddVertexBuffer(vboNorm);
				}
				if (hasTangents && !mergedTangents.empty())
				{
					auto vboTan =
						VertexBuffer::Create(mergedTangents.data(), (uint32_t)mergedTangents.size() * sizeof(float));
					vboTan->SetLayout({{VertexAttributeType::Float3, "a_Tangent"}});
					mergedMesh.VAO->AddVertexBuffer(vboTan);
				}

				auto ibo = IndexBuffer::Create(mergedIndices.data(), (uint32_t)mergedIndices.size());
				mergedMesh.VAO->SetIndexBuffer(ibo);
			}

			// Compute merged bounding box
			mergedMesh.MinBounds = {FLT_MAX, FLT_MAX, FLT_MAX};
			mergedMesh.MaxBounds = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
			for (size_t v = 0; v < mergedVerts.size(); v += 3)
			{
				glm::vec3 p = {mergedVerts[v], mergedVerts[v + 1], mergedVerts[v + 2]};
				mergedMesh.MinBounds = glm::min(mergedMesh.MinBounds, p);
				mergedMesh.MaxBounds = glm::max(mergedMesh.MaxBounds, p);
			}

			// Append merged GPU mesh to m_Model.Meshes.
			// Also append a merged CPU MeshData to m_Meshes so the physics system
			// (which indexes rawMeshes[inst.meshIndex]) finds the correct triangles.
			int newMeshIndex = (int)m_Model.Meshes.size(); // same index in both arrays
			m_Model.Meshes.push_back(std::move(mergedMesh));

			{
				MeshData mergedCpu;
				mergedCpu.materialIndex = matIdx;
				mergedCpu.vertices = mergedVerts;
				mergedCpu.texcoords = mergedTexcoords;
				mergedCpu.normals = mergedNormals;
				mergedCpu.tangents = mergedTangents;
				mergedCpu.indices = mergedIndices;
				m_Meshes.push_back(std::move(mergedCpu));
			}

			MeshInstance inst;
			inst.meshIndex = newMeshIndex;
			inst.localTransform = glm::mat4(1.0f); // transforms already baked in
			survivingInstances.push_back(inst);
		}

		CH_CORE_TRACE("[ModelAsset] MergeStaticMeshesByMaterial '{}': {} instances → {} merged instances", GetPath(),
					  m_Instances.size(), survivingInstances.size());

		m_Instances = std::move(survivingInstances);
	}

	void ModelAsset::Unload()
	{
		m_Model = Model();
		m_Meshes.clear();
		m_Materials.clear();
		m_Instances.clear();
		m_Animations.clear();
		m_EmbeddedTextures.clear();
		m_PendingData = PendingModelData();
		m_BoundingBox = {{0, 0, 0}, {0, 0, 0}};
		SetState(AssetState::None);
	}

} // namespace Chained