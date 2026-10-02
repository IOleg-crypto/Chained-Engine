#include "gl_uniform_buffer.h"
#include "engine/graphics/api/graphics_device.h"
#include <glad/gl.h>

namespace Chained
{
	GLUniformBuffer::GLUniformBuffer(uint32_t size, uint32_t binding)
	{
		glGenBuffers(1, &m_RendererID);
		glBindBuffer(GL_UNIFORM_BUFFER, m_RendererID);
		glBufferData(GL_UNIFORM_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, binding, m_RendererID);
		glBindBuffer(GL_UNIFORM_BUFFER, 0);
	}

	GLUniformBuffer::~GLUniformBuffer()
	{
		if (m_RendererID)
		{
			uint32_t id = m_RendererID;
			GraphicsDevice::EnqueueResourceDeletion([id]() { glDeleteBuffers(1, &id); });
		}
	}

	void GLUniformBuffer::SetData(const void* data, uint32_t size, uint32_t offset)
	{
		glBindBuffer(GL_UNIFORM_BUFFER, m_RendererID);
		glBufferSubData(GL_UNIFORM_BUFFER, offset, size, data);
	}

	void GLUniformBuffer::BindBase(uint32_t binding)
	{
		glBindBufferBase(GL_UNIFORM_BUFFER, binding, m_RendererID);
	}
} // namespace Chained
