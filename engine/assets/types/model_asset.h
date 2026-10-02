#ifndef CH_MODEL_ASSET_H
#define CH_MODEL_ASSET_H

#include "engine/assets/asset.h"
#include "engine/common/base.h"
#include "engine/assets/model_data.h"
#include "engine/graphics/api/renderer_types.h"
#include <future>
// #include <mutex>
#include <string>
#include <vector>

namespace Chained
{
	class Texture;

	class ModelAsset : public Asset
	{
	public:
		ModelAsset()
			: Asset(GetStaticType())
		{
		}
		virtual ~ModelAsset() = default;

		static AssetType GetStaticType()
		{
			return AssetType::Model;
		}

		void OnLoaded() override;
		void Unload() override;

		const Model& GetModel() const
		{
			return m_Model;
		}
		const std::vector<AnimationData>& GetAnimations() const
		{
			return m_Animations;
		}
		const std::vector<MeshInstance>& GetInstances() const
		{
			return m_Instances;
		}
		const BoundingBox& GetBoundingBox() const
		{
			return m_BoundingBox;
		}

		const std::vector<MeshData>& GetMeshes() const
		{
			return m_Meshes;
		}

		const std::vector<Material>& GetMaterials() const
		{
			return m_Materials;
		}
		std::vector<Material>& GetMaterials()
		{
			return m_Materials;
		}

		// Helpers
		int GetAnimationCount() const
		{
			return (int)m_Animations.size();
		}
		void SetPendingData(PendingModelData&& data)
		{
			m_PendingData = std::move(data);
		}
		PendingModelData& GetPendingData()
		{
			return m_PendingData;
		}
		bool HasPendingData() const
		{
			return m_PendingData.isValid;
		}

		void SetModel(const Model& model)
		{
			m_Model = model;
		}
		void SetAnimations(const std::vector<AnimationData>& animations)
		{
			m_Animations = animations;
		}
		void SetInstances(const std::vector<MeshInstance>& instances)
		{
			m_Instances = instances;
		}
		void SetBoundingBox(const BoundingBox& bbox)
		{
			m_BoundingBox = bbox;
		}
		void SetMeshes(const std::vector<MeshData>& meshes)
		{
			m_Meshes = meshes;
		}
		void SetOffsetMatrices(const std::vector<glm::mat4>& matrices)
		{
			m_OffsetMatrices = matrices;
		}
		void SetNodeNames(const std::vector<std::string>& names)
		{
			m_NodeNames = names;
		}
		void SetNodeParents(const std::vector<int>& parents)
		{
			m_NodeParents = parents;
		}
		std::string GetAnimationName(int index) const;
		std::vector<glm::mat4> GetBoneMatrices(int animationIndex, int frame) const;
		// Get renderer ID for embedded texture (e.g., "*0", "*1").
		// Returns 0 if the texture is not found or path is not an embedded texture marker.
		uint32_t GetEmbeddedTextureID(const std::string& path) const;

	private:
		Model m_Model;
		std::vector<MeshData> m_Meshes;
		std::vector<AnimationData> m_Animations;
		std::vector<MeshInstance> m_Instances;
		std::vector<Material> m_Materials;
		BoundingBox m_BoundingBox = {{0, 0, 0}, {0, 0, 0}};

		// Skeleton data
		std::vector<glm::mat4> m_OffsetMatrices;
		std::vector<std::string> m_NodeNames;
		std::vector<int> m_NodeParents;

		// Loading data
		PendingModelData m_PendingData;
		std::vector<std::shared_ptr<Texture>> m_EmbeddedTextures;
	};
} // namespace Chained

#endif // CH_MODEL_ASSET_H