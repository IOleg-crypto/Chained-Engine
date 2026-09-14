#ifndef CH_RENDERER_TYPES_H
#define CH_RENDERER_TYPES_H

#include "engine/common/color.h"
#include "engine/graphics/api/vertex_array.h"
#include "engine/reflection/reflection_rfl.h"
#include "engine/graphics/api/texture.h"
#include <cstring>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>
#include <variant>

namespace Chained
{

	enum class ShaderUniformType : int
	{
		Float = 0,
		Vec2,
		Vec3,
		Vec4,
		Color
	};

	struct ShaderUniform
	{
		std::string Name;
		std::variant<float, glm::vec2, glm::vec3, glm::vec4, Color> Value = 0.0f;

		static const char* GetStaticName()
		{
			return "ShaderUniform";
		}

		struct UI
		{
			UIMeta Name = {.Tooltip = "The uniform variable name defined inside the GLSL shader"};
			UIMeta Value = {.Tooltip = "The modern variant value matching the uniform's data type"};
		};
	};
	CH_MARK_RFL(ShaderUniform);

	struct Material
	{
		glm::vec4 AlbedoColor = {1.0f, 1.0f, 1.0f, 1.0f};
		glm::vec4 EmissiveColor = {0.0f, 0.0f, 0.0f, 1.0f};
		float EmissiveIntensity = 0.0f;
		float Metalness = 0.0f;
		float Roughness = 0.5f;

		bool FlipUV_Y = false;
		bool FlipUV_X = false;
		glm::vec2 UVScale = {1.0f, 1.0f};
		glm::vec2 UVOffset = {0.0f, 0.0f};

		std::shared_ptr<Texture> AlbedoMap;
		std::shared_ptr<Texture> NormalMap;
		std::shared_ptr<Texture> MetallicRoughnessMap;
		std::shared_ptr<Texture> EmissiveMap;
		std::shared_ptr<Texture> OcclusionMap;

		std::string AlbedoPath;
		std::string NormalPath;
		std::string MetallicRoughnessPath;
		std::string EmissivePath;
		std::string OcclusionPath;

		uint32_t ShaderID = 0;
		bool Transparent = false;
		float Alpha = 1.0f;
		std::string Name;

		static const char* GetStaticName()
		{
			return "Material";
		}

		/// Path-based equality: two Materials are "same batch" when they reference the same
		/// textures and share the same key scalar/state values. shared_ptr identity is NOT
		/// compared — only the resolved path strings matter for instancing grouping.
		bool operator==(const Material& o) const
		{
			return AlbedoPath == o.AlbedoPath && NormalPath == o.NormalPath &&
				   MetallicRoughnessPath == o.MetallicRoughnessPath && EmissivePath == o.EmissivePath &&
				   OcclusionPath == o.OcclusionPath && ShaderID == o.ShaderID && Transparent == o.Transparent &&
				   Alpha == o.Alpha && Metalness == o.Metalness && Roughness == o.Roughness &&
				   AlbedoColor == o.AlbedoColor && EmissiveColor == o.EmissiveColor &&
				   EmissiveIntensity == o.EmissiveIntensity && FlipUV_Y == o.FlipUV_Y && FlipUV_X == o.FlipUV_X &&
				   UVScale == o.UVScale && UVOffset == o.UVOffset;
		}
		bool operator!=(const Material& o) const
		{
			return !(*this == o);
		}

		struct UI
		{
			UIMeta AlbedoColor = {.Tooltip = "Base diffuse surface color"};
			UIMeta EmissiveColor = {.Tooltip = "Color emitted by the material surface"};
			UIMeta EmissiveIntensity = {.Tooltip = "Brightness multiplier for the emissive color"};
			UIMeta Metalness = {.Tooltip = "How close the surface reflects like a metal (0.0 to 1.0)"};
			UIMeta Roughness = {.Tooltip = "Microfacet roughness from smooth/glossy to diffuse (0.0 to 1.0)"};
			UIMeta Transparent = {.Tooltip = "Enables alpha blending layers for this material"};
			UIMeta Alpha = {.Tooltip = "Global opacity multiplier"};
			UIMeta FlipUV_Y = {.Tooltip = "Flip texture V/Y coordinates vertically"};
			UIMeta FlipUV_X = {.Tooltip = "Flip texture U/X coordinates horizontally"};
			UIMeta UVScale = {.Tooltip = "Texture coordinate tiling/scale factor"};
			UIMeta UVOffset = {.Tooltip = "Texture coordinate offset"};
		};
	};
	CH_MARK_RFL(Material);

	struct Mesh
	{
		std::shared_ptr<VertexArray> VAO;
		uint32_t VertexCount = 0;
		uint32_t TriangleCount = 0;
		int MaterialIndex = 0;
		bool HasSkinning = false;

		glm::vec3 MinBounds = {0.0f, 0.0f, 0.0f};
		glm::vec3 MaxBounds = {0.0f, 0.0f, 0.0f};
	};

	struct Model
	{
		std::vector<Mesh> Meshes;
		std::vector<Material> Materials;
		glm::mat4 Transform = glm::mat4(1.0f);
	};

} // namespace Chained

#endif // CH_RENDERER_TYPES_H
