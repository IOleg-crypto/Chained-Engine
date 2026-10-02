#ifndef CH_MATERIAL_MANAGER_H
#define CH_MATERIAL_MANAGER_H

#include "engine/common/base.h"
#include "engine/graphics/api/renderer_types.h"
#include <vector>

namespace Chained
{

	class Shader;
	class ModelAsset;
	struct Model;

	/// @brief Centralizes material resolution and GPU binding.
	/// Extracted from SceneRenderer to isolate texture loading, PBR uniform
	/// binding, and the 3-tier material fallback chain.
	class CH_API MaterialManager
	{
	public:
		MaterialManager() = default;
		~MaterialManager() = default;

		/// @brief Resolve the material for a specific mesh using a 3-tier fallback:
		///   1. Caller-supplied @p materials vector
		///   2. ModelAsset embedded materials
		///   3. Model struct embedded materials
		///   Returns a default Material if nothing matches.
		Material Resolve(int meshIndex, const Model& model, const std::vector<Material>& materials,
						 ModelAsset* modelAsset = nullptr) const;

		/// @brief Bind all PBR textures and uniforms for the given material to the shader.
		/// Resolves lazy-loaded textures via AssetManager when shared_ptr is null.
		void Bind(Shader* shader, const Material& material, int meshIndex, const Model& model) const;
	};

} // namespace Chained

#endif // CH_MATERIAL_MANAGER_H
