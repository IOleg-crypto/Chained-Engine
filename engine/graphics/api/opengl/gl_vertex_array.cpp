#include "gl_vertex_array.h"
#include "engine/graphics/api/graphics_device.h"
#include <glad/gl.h>

namespace Chained
{

	static uint32_t VertexAttributeTypeToOpenGLBaseType(VertexAttributeType type)
	{
		switch (type)
		{
		case VertexAttributeType::Float:
			return GL_FLOAT;
		case VertexAttributeType::Float2:
			return GL_FLOAT;
		case VertexAttributeType::Float3:
			return GL_FLOAT;
		case VertexAttributeType::Float4:
			return GL_FLOAT;
		case VertexAttributeType::Mat3:
			return GL_FLOAT;
		case VertexAttributeType::Mat4:
			return GL_FLOAT;
		case VertexAttributeType::Int:
			return GL_INT;
		case VertexAttributeType::Int2:
			return GL_INT;
		case VertexAttributeType::Int3:
			return GL_INT;
		case VertexAttributeType::Int4:
			return GL_INT;
		case VertexAttributeType::Bool:
			return GL_UNSIGNED_BYTE;
		}

		CH_CORE_ASSERT(false, "Unknown VertexAttributeType!");
		return 0;
	}

	GLVertexArray::GLVertexArray()
	{
		glGenVertexArrays(1, &m_RendererID);
	}

	GLVertexArray::~GLVertexArray()
	{
		if (m_RendererID)
		{
			uint32_t id = m_RendererID;
			GraphicsDevice::EnqueueResourceDeletion([id]() { glDeleteVertexArrays(1, &id); });
		}
	}

	void GLVertexArray::Bind() const
	{
		glBindVertexArray(m_RendererID);
	}

	void GLVertexArray::Unbind() const
	{
		glBindVertexArray(0);
	}

	void GLVertexArray::AddVertexBuffer(const std::shared_ptr<VertexBuffer>& vertexBuffer)
	{
		CH_CORE_ASSERT(vertexBuffer->GetLayout().GetElements().size(), "Vertex Buffer has no layout!");

		glBindVertexArray(m_RendererID);
		vertexBuffer->Bind();

		const auto& layout = vertexBuffer->GetLayout();
		for (const auto& element : layout)
		{
			uint32_t count = element.GetComponentCount();
			VertexAttributeType type = element.Type;

			if (type == VertexAttributeType::Mat4)
			{
				for (uint32_t i = 0; i < 4; i++)
				{
					uint32_t index = m_AttributeIndex++;
					glEnableVertexAttribArray(index);
					glVertexAttribPointer(index, 4, VertexAttributeTypeToOpenGLBaseType(type),
										  element.Normalized ? GL_TRUE : GL_FALSE, layout.GetStride(),
										  (const void*)(element.Offset + sizeof(glm::vec4) * i));

					if (element.Instanced)
					{
						glVertexAttribDivisor(index, 1);
					}
				}
			}
			else if (type == VertexAttributeType::Mat3)
			{
				for (uint32_t i = 0; i < 3; i++)
				{
					uint32_t index = m_AttributeIndex++;
					glEnableVertexAttribArray(index);
					glVertexAttribPointer(index, 3, VertexAttributeTypeToOpenGLBaseType(type),
										  element.Normalized ? GL_TRUE : GL_FALSE, layout.GetStride(),
										  (const void*)(element.Offset + sizeof(glm::vec3) * i));

					if (element.Instanced)
					{
						glVertexAttribDivisor(index, 1);
					}
				}
			}
			else if (type == VertexAttributeType::Int || type == VertexAttributeType::Int2 ||
					 type == VertexAttributeType::Int3 || type == VertexAttributeType::Int4 ||
					 type == VertexAttributeType::Bool)
			{
				uint32_t index = m_AttributeIndex++;
				glEnableVertexAttribArray(index);
				glVertexAttribIPointer(index, count, VertexAttributeTypeToOpenGLBaseType(type), layout.GetStride(),
									   (const void*)element.Offset);

				if (element.Instanced)
				{
					glVertexAttribDivisor(index, 1);
				}
			}
			else
			{
				uint32_t index = m_AttributeIndex++;
				glEnableVertexAttribArray(index);
				glVertexAttribPointer(index, count, VertexAttributeTypeToOpenGLBaseType(type),
									  element.Normalized ? GL_TRUE : GL_FALSE, layout.GetStride(),
									  (const void*)element.Offset);

				if (element.Instanced)
				{
					glVertexAttribDivisor(index, 1);
				}
			}
		}

		m_VertexBuffers.push_back(vertexBuffer);
	}

	void GLVertexArray::SetIndexBuffer(const std::shared_ptr<IndexBuffer>& indexBuffer)
	{
		glBindVertexArray(m_RendererID);
		indexBuffer->Bind();

		m_IndexBuffer = indexBuffer;
	}

} // namespace Chained
