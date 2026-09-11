#ifndef CH_GEOMETRY_GENERATOR_H
#define CH_GEOMETRY_GENERATOR_H

#include "engine/graphics/api/renderer_types.h"
#include "engine/assets/model_data.h"
#include "engine/assets/loaders/model_loader.h"
#include <string>

namespace Chained
{
	namespace GeometryGenerator
	{
		// Generates a sphere mesh
		Mesh GenerateSphere(float radius, int slices, int stacks);

		// Generates a unit cube mesh for skyboxes/cubemaps (triangles)
		Mesh GenerateUnitCube();

		// Generates a wireframe cube with 12 edges for debug drawing (GL_LINES)
		Mesh GenerateWireCube();

		// Generates a grid mesh
		Mesh GenerateGrid(int slices, float spacing);

		// Generates a fullscreen quad or a simple plane quad
		Mesh GenerateQuad(float size);

		// Generates a capsule mesh (cylinder + two hemispheres)
		Mesh GenerateCapsule(float radius, float height, int slices, int stacks);

		// Generate a cube mesh with given dimensions (for procedural cube generation)
		Mesh GenerateCube(const glm::vec3& dimensions);

	} // namespace GeometryGenerator
} // namespace Chained

#endif // CH_GEOMETRY_GENERATOR_H
