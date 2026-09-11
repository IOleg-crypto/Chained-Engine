#include "geometry_generator.h"
#include <vector>
#include <cmath>
#include <algorithm>
#include <limits>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace Chained
{
	Mesh GeometryGenerator::GenerateUnitCube()
	{
		float vertices[] = {-0.5f, 0.5f,  -0.5f, -0.5f, -0.5f, -0.5f, 0.5f,	 -0.5f, -0.5f,
							0.5f,  -0.5f, -0.5f, 0.5f,	0.5f,  -0.5f, -0.5f, 0.5f,	-0.5f,

							-0.5f, -0.5f, 0.5f,	 -0.5f, -0.5f, -0.5f, -0.5f, 0.5f,	-0.5f,
							-0.5f, 0.5f,  0.5f,	 -0.5f, 0.5f,  0.5f,  -0.5f, -0.5f, 0.5f,

							0.5f,  -0.5f, -0.5f, 0.5f,	-0.5f, 0.5f,  0.5f,	 0.5f,	0.5f,
							0.5f,  0.5f,  0.5f,	 0.5f,	0.5f,  -0.5f, 0.5f,	 -0.5f, -0.5f,

							-0.5f, -0.5f, 0.5f,	 -0.5f, 0.5f,  0.5f,  0.5f,	 0.5f,	0.5f,
							0.5f,  0.5f,  0.5f,	 0.5f,	-0.5f, 0.5f,  -0.5f, -0.5f, 0.5f,

							-0.5f, 0.5f,  -0.5f, 0.5f,	0.5f,  -0.5f, 0.5f,	 0.5f,	0.5f,
							0.5f,  0.5f,  0.5f,	 -0.5f, 0.5f,  0.5f,  -0.5f, 0.5f,	-0.5f,

							-0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f,  0.5f,	 -0.5f, -0.5f,
							0.5f,  -0.5f, -0.5f, -0.5f, -0.5f, 0.5f,  0.5f,	 -0.5f, 0.5f};

		auto vbo = VertexBuffer::Create(vertices, sizeof(vertices));
		vbo->SetLayout({{VertexAttributeType::Float3, "a_Position"}});
		auto vao = VertexArray::Create();
		vao->AddVertexBuffer(vbo);

		uint32_t indices[36];
		for (uint32_t index = 0; index < 36; ++index)
		{
			indices[index] = index;
		}
		auto ebo = IndexBuffer::Create(indices, 36);
		vao->SetIndexBuffer(ebo);

		Mesh mesh;
		mesh.VAO = vao;
		mesh.VertexCount = 36;
		mesh.TriangleCount = 12;
		return mesh;
	}

	Mesh GeometryGenerator::GenerateWireCube()
	{
		// 8 unique corner vertices of a unit cube [-0.5, 0.5]
		float vertices[] = {
			-0.5f, -0.5f, -0.5f, // 0: left  bottom back
			0.5f,  -0.5f, -0.5f, // 1: right bottom back
			0.5f,  0.5f,  -0.5f, // 2: right top    back
			-0.5f, 0.5f,  -0.5f, // 3: left  top    back
			-0.5f, -0.5f, 0.5f,	 // 4: left  bottom front
			0.5f,  -0.5f, 0.5f,	 // 5: right bottom front
			0.5f,  0.5f,  0.5f,	 // 6: right top    front
			-0.5f, 0.5f,  0.5f	 // 7: left  top    front
		};

		// 12 edges × 2 indices = 24 indices for GL_LINES
		uint32_t indices[] = {// Back face edges
							  0, 1, 1, 2, 2, 3, 3, 0,
							  // Front face edges
							  4, 5, 5, 6, 6, 7, 7, 4,
							  // Connecting edges
							  0, 4, 1, 5, 2, 6, 3, 7};

		auto vbo = VertexBuffer::Create(vertices, sizeof(vertices));
		vbo->SetLayout({{VertexAttributeType::Float3, "a_Position"}});
		auto vao = VertexArray::Create();
		vao->AddVertexBuffer(vbo);
		auto ebo = IndexBuffer::Create(indices, 24);
		vao->SetIndexBuffer(ebo);

		Mesh mesh;
		mesh.VAO = vao;
		mesh.VertexCount = 8;
		mesh.TriangleCount = 0; // Lines, not triangles
		return mesh;
	}

	Mesh GeometryGenerator::GenerateSphere(float radius, int slices, int stacks)
	{
		std::vector<float> vertices;
		std::vector<uint32_t> indices;

		for (int stackIndex = 0; stackIndex <= stacks; ++stackIndex)
		{
			float stackFraction = (float)stackIndex / (float)stacks;
			float polarAngle = stackFraction * glm::pi<float>();

			for (int sliceIndex = 0; sliceIndex <= slices; ++sliceIndex)
			{
				float sliceFraction = (float)sliceIndex / (float)slices;
				float azimuthAngle = sliceFraction * 2.0f * glm::pi<float>();

				float nx = std::cos(azimuthAngle) * std::sin(polarAngle);
				float ny = std::cos(polarAngle);
				float nz = std::sin(azimuthAngle) * std::sin(polarAngle);

				// Position
				vertices.push_back(nx * radius);
				vertices.push_back(ny * radius);
				vertices.push_back(nz * radius);
				// TexCoord
				vertices.push_back(sliceFraction);
				vertices.push_back(1.0f - stackFraction);
				// Normal
				vertices.push_back(nx);
				vertices.push_back(ny);
				vertices.push_back(nz);
			}
		}

		for (int stackIndex = 0; stackIndex < stacks; ++stackIndex)
		{
			for (int sliceIndex = 0; sliceIndex < slices; ++sliceIndex)
			{
				indices.push_back((stackIndex + 1) * (slices + 1) + sliceIndex);
				indices.push_back(stackIndex * (slices + 1) + sliceIndex);
				indices.push_back(stackIndex * (slices + 1) + sliceIndex + 1);
				indices.push_back((stackIndex + 1) * (slices + 1) + sliceIndex);
				indices.push_back(stackIndex * (slices + 1) + sliceIndex + 1);
				indices.push_back((stackIndex + 1) * (slices + 1) + (sliceIndex + 1));
			}
		}

		auto vbo = VertexBuffer::Create(vertices.data(), (uint32_t)vertices.size() * sizeof(float));
		// Pos(3) + Tex(2) + Normal(3) = 8 floats per vertex
		vbo->SetLayout({{VertexAttributeType::Float3, "a_Position"},
						{VertexAttributeType::Float2, "a_TexCoord"},
						{VertexAttributeType::Float3, "a_Normal"}});
		auto vao = VertexArray::Create();
		vao->AddVertexBuffer(vbo);
		auto ebo = IndexBuffer::Create(indices.data(), (uint32_t)indices.size());
		vao->SetIndexBuffer(ebo);

		Mesh mesh;
		mesh.VAO = vao;
		mesh.VertexCount = (uint32_t)vertices.size() / 8;
		mesh.TriangleCount = (uint32_t)indices.size() / 3;
		return mesh;
	}

	Mesh GeometryGenerator::GenerateGrid(int slices, float spacing)
	{
		std::vector<float> vertices;
		for (int lineIndex = -slices; lineIndex <= slices; ++lineIndex)
		{
			vertices.push_back((float)lineIndex * spacing);
			vertices.push_back(0);
			vertices.push_back((float)-slices * spacing);
			vertices.push_back((float)lineIndex * spacing);
			vertices.push_back(0);
			vertices.push_back((float)slices * spacing);

			vertices.push_back((float)-slices * spacing);
			vertices.push_back(0);
			vertices.push_back((float)lineIndex * spacing);
			vertices.push_back((float)slices * spacing);
			vertices.push_back(0);
			vertices.push_back((float)lineIndex * spacing);
		}

		auto vbo = VertexBuffer::Create(vertices.data(), (uint32_t)vertices.size() * sizeof(float));
		vbo->SetLayout({{VertexAttributeType::Float3, "a_Position"}});
		auto vao = VertexArray::Create();
		vao->AddVertexBuffer(vbo);

		Mesh mesh;
		mesh.VAO = vao;
		mesh.VertexCount = (uint32_t)vertices.size() / 3;
		mesh.TriangleCount = 0; // Grid is lines
		return mesh;
	}

	Mesh GeometryGenerator::GenerateQuad(float size)
	{
		// Pos(3), Tex(2), Normal(3)
		float vertices[] = {-size, 0.0f, -size, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, size, 0.0f, -size,
							1.0f,  0.0f, 0.0f,	1.0f, 0.0f, size, 0.0f, size, 1.0f, 1.0f, 0.0f,
							1.0f,  0.0f, -size, 0.0f, size, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f};

		auto vbo = VertexBuffer::Create(vertices, sizeof(vertices));
		vbo->SetLayout({{VertexAttributeType::Float3, "a_Position"},
						{VertexAttributeType::Float2, "a_TexCoord"},
						{VertexAttributeType::Float3, "a_Normal"}});
		auto vao = VertexArray::Create();
		vao->AddVertexBuffer(vbo);
		// TriangleCount = 2 below makes DrawMesh take the DrawIndexed path, so the VAO MUST
		// carry an index buffer — without it glDrawElements reads from a null EBO and crashes.
		uint32_t indices[] = {0, 1, 2, 2, 3, 0};
		auto ebo = IndexBuffer::Create(indices, 6);
		vao->SetIndexBuffer(ebo);

		Mesh mesh;
		mesh.VAO = vao;
		mesh.VertexCount = 4;
		mesh.TriangleCount = 2;
		return mesh;
	}

	Mesh GeometryGenerator::GenerateCube(const glm::vec3& dimensions)
	{
		float w = dimensions.x * 0.5f;
		float h = dimensions.y * 0.5f;
		float d = dimensions.z * 0.5f;

		// Front, Back, Top, Bottom, Right, Left faces
		// 8 floats per vertex: Pos(3), Tex(2), Normal(3)
		float vertices[] = {// Front
							-w, -h, d, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, w, -h, d, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, w, h, d,
							1.0f, 1.0f, 0.0f, 0.0f, 1.0f, -w, h, d, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
							// Back
							w, -h, -d, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, -w, -h, -d, 1.0f, 0.0f, 0.0f, 0.0f, -1.0f, -w, h,
							-d, 1.0f, 1.0f, 0.0f, 0.0f, -1.0f, w, h, -d, 0.0f, 1.0f, 0.0f, 0.0f, -1.0f,
							// Top
							-w, h, d, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, w, h, d, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, w, h, -d,
							1.0f, 1.0f, 0.0f, 1.0f, 0.0f, -w, h, -d, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
							// Bottom
							-w, -h, -d, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, w, -h, -d, 1.0f, 0.0f, 0.0f, -1.0f, 0.0f, w, -h,
							d, 1.0f, 1.0f, 0.0f, -1.0f, 0.0f, -w, -h, d, 0.0f, 1.0f, 0.0f, -1.0f, 0.0f,
							// Right
							w, -h, d, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, w, -h, -d, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, w, h, -d,
							1.0f, 1.0f, 1.0f, 0.0f, 0.0f, w, h, d, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f,
							// Left
							-w, -h, -d, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f, -w, -h, d, 1.0f, 0.0f, -1.0f, 0.0f, 0.0f, -w, h,
							d, 1.0f, 1.0f, -1.0f, 0.0f, 0.0f, -w, h, -d, 0.0f, 1.0f, -1.0f, 0.0f, 0.0f};

		uint32_t indices[] = {0,  1,  2,  2,  3,  0,  4,  5,  6,  6,  7,  4,  8,  9,  10, 10, 11, 8,
							  12, 13, 14, 14, 15, 12, 16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20};

		auto vbo = VertexBuffer::Create(vertices, sizeof(vertices));
		vbo->SetLayout({{VertexAttributeType::Float3, "a_Position"},
						{VertexAttributeType::Float2, "a_TexCoord"},
						{VertexAttributeType::Float3, "a_Normal"}});
		auto vao = VertexArray::Create();
		vao->AddVertexBuffer(vbo);
		auto ebo = IndexBuffer::Create(indices, 36);
		vao->SetIndexBuffer(ebo);

		Mesh mesh;
		mesh.VAO = vao;
		mesh.VertexCount = 24;
		mesh.TriangleCount = 12;
		return mesh;
	}

	Mesh GeometryGenerator::GenerateCapsule(float radius, float height, int slices, int stacks)
	{
		std::vector<float> vertices;
		std::vector<uint32_t> indices;

		// A capsule is a cylinder with two hemispheres.
		// Height provided by user is total height.
		// Cylinder height = height - 2 * radius.
		float cylinderHeight = std::max(0.0f, height - 2.0f * radius);
		float halfCylinderHeight = cylinderHeight * 0.5f;

		// Generate vertices
		for (int stackIndex = 0; stackIndex <= stacks; ++stackIndex)
		{
			float stackFraction = (float)stackIndex / (float)stacks;
			float polarAngle = stackFraction * glm::pi<float>();

			// Offset Y based on which hemisphere or cylinder part we are in
			float yOffset = 0.0f;
			if (stackFraction < 0.5f)
			{
				yOffset = halfCylinderHeight;
			}
			else
			{
				yOffset = -halfCylinderHeight;
			}

			for (int sliceIndex = 0; sliceIndex <= slices; ++sliceIndex)
			{
				float sliceFraction = (float)sliceIndex / (float)slices;
				float azimuthAngle = sliceFraction * 2.0f * glm::pi<float>();

				float x = std::cos(azimuthAngle) * std::sin(polarAngle);
				float y = std::cos(polarAngle);
				float z = std::sin(azimuthAngle) * std::sin(polarAngle);

				vertices.push_back(x * radius);
				vertices.push_back(y * radius + yOffset);
				vertices.push_back(z * radius);
			}
		}

		// Generate indices
		for (int stackIndex = 0; stackIndex < stacks; ++stackIndex)
		{
			for (int sliceIndex = 0; sliceIndex < slices; ++sliceIndex)
			{
				indices.push_back((stackIndex + 1) * (slices + 1) + sliceIndex);
				indices.push_back(stackIndex * (slices + 1) + sliceIndex);
				indices.push_back(stackIndex * (slices + 1) + sliceIndex + 1);
				indices.push_back((stackIndex + 1) * (slices + 1) + sliceIndex);
				indices.push_back(stackIndex * (slices + 1) + sliceIndex + 1);
				indices.push_back((stackIndex + 1) * (slices + 1) + (sliceIndex + 1));
			}
		}

		auto vbo = VertexBuffer::Create(vertices.data(), (uint32_t)vertices.size() * sizeof(float));
		vbo->SetLayout({{VertexAttributeType::Float3, "a_Position"}});
		auto vao = VertexArray::Create();
		vao->AddVertexBuffer(vbo);
		auto ebo = IndexBuffer::Create(indices.data(), (uint32_t)indices.size());
		vao->SetIndexBuffer(ebo);

		Mesh mesh;
		mesh.VAO = vao;
		mesh.VertexCount = (uint32_t)vertices.size() / 3;
		mesh.TriangleCount = (uint32_t)indices.size() / 3;
		return mesh;
	}
} // namespace Chained
