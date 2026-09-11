#ifndef CH_MATERIAL_EXTRACTOR_H
#define CH_MATERIAL_EXTRACTOR_H

#include "engine/assets/model_data.h"
#include <assimp/scene.h>
#include <filesystem>
#include <vector>

namespace Chained
{

	namespace MaterialExtractor
	{
		void Process(const aiScene* scene, const std::filesystem::path& modelDir, std::vector<MaterialData>& materials,
					 std::vector<MeshData>& meshes);

		void ExtractMaterial(aiMaterial* am, const std::filesystem::path& modelDir, MaterialData& out);
		void RemoveUnreferenced(std::vector<MaterialData>& materials, std::vector<MeshData>& meshes);
	} // namespace MaterialExtractor

} // namespace Chained

#endif // CH_MATERIAL_EXTRACTOR_H
