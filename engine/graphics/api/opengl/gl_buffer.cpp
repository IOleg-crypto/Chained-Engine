#include "gl_buffer.h"
#include "engine/graphics/api/graphics_device.h"
#include <glad/gl.h>

namespace Chained
{

	// VertexBuffer ///////////////////////////////////////////////////////////

	GLVertexBuffer::GLVertexBuffer(uint32_t size)
	{
		glGenBuffers(1, &m_RendererID);
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ARRAY_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
	}

	GLVertexBuffer::GLVertexBuffer(const float* vertices, uint32_t size)
	{
		glGenBuffers(1, &m_RendererID);
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
	}

	GLVertexBuffer::~GLVertexBuffer()
	{
		if (m_RendererID)
		{
			uint32_t id = m_RendererID;
			GraphicsDevice::EnqueueResourceDeletion([id]() { glDeleteBuffers(1, &id); });
		}
	}

	void GLVertexBuffer::Bind() const
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
	}

	void GLVertexBuffer::Unbind() const
	{
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void GLVertexBuffer::SetData(const void* data, uint32_t size)
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_RendererID);
		glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
	}

	// IndexBuffer ////////////////////////////////////////////////////////////

	GLIndexBuffer::GLIndexBuffer(const uint32_t* indices, uint32_t count)
		: m_Count(count)
	{
		glGenBuffers(1, &m_RendererID);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, count * sizeof(uint32_t), indices, GL_STATIC_DRAW);
	}

	GLIndexBuffer::~GLIndexBuffer()
	{
		if (m_RendererID)
		{
			uint32_t id = m_RendererID;
			GraphicsDevice::EnqueueResourceDeletion([id]() { glDeleteBuffers(1, &id); });
		}
	}

	void GLIndexBuffer::Bind() const
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_RendererID);
	}

	void GLIndexBuffer::Unbind() const
	{
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	}

} // namespace Chained
