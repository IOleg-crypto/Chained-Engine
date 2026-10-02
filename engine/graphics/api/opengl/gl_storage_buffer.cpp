#include "gl_storage_buffer.h"
#include "engine/graphics/api/graphics_device.h"
#include <glad/gl.h>

namespace Chained
{
	GLStorageBuffer::GLStorageBuffer(uint32_t size)
	{
		glGenBuffers(1, &m_RendererID);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_RendererID);
		glBufferData(GL_SHADER_STORAGE_BUFFER, size, nullptr, GL_DYNAMIC_DRAW);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}

	GLStorageBuffer::~GLStorageBuffer()
	{
		if (m_RendererID)
		{
			uint32_t id = m_RendererID;
			GraphicsDevice::EnqueueResourceDeletion([id]() { glDeleteBuffers(1, &id); });
		}
	}

	void GLStorageBuffer::BindBase(uint32_t slot) const
	{
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, slot, m_RendererID);
	}

	void GLStorageBuffer::SetData(const void* data, uint32_t size, uint32_t offset)
	{
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_RendererID);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, offset, size, data);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	}
} // namespace Chained
