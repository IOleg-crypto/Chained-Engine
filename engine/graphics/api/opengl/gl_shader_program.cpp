#include "gl_shader_program.h"
#include "engine/graphics/api/graphics_device.h"

#include <glad/gl.h>

namespace Chained
{

	GLShaderProgram::GLShaderProgram(const std::string& vsSource, const std::string& fsSource)
	{
		auto compileShader = [](GLenum type, const char* source) -> uint32_t {
			uint32_t shader = glCreateShader(type);
			glShaderSource(shader, 1, &source, nullptr);
			glCompileShader(shader);

			int success;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
			if (!success)
			{
				int maxLength = 0;
				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
				std::vector<char> infoLog(maxLength);
				glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);

				CH_CORE_ERROR("[Shader] {} Compilation Error:\n{}", (type == GL_VERTEX_SHADER ? "Vertex" : "Fragment"),
							  infoLog.data());

				glDeleteShader(shader);
				return 0;
			}
			return shader;
		};

		uint32_t vs = compileShader(GL_VERTEX_SHADER, vsSource.c_str());
		uint32_t fs = compileShader(GL_FRAGMENT_SHADER, fsSource.c_str());

		if (vs == 0 || fs == 0)
		{
			if (vs)
			{
				glDeleteShader(vs);
			}
			if (fs)
			{
				glDeleteShader(fs);
			}
			return;
		}

		m_RendererID = glCreateProgram();
		glAttachShader(m_RendererID, vs);
		glAttachShader(m_RendererID, fs);
		glLinkProgram(m_RendererID);

		int success;
		glGetProgramiv(m_RendererID, GL_LINK_STATUS, &success);
		if (!success)
		{
			int maxLength = 0;
			glGetProgramiv(m_RendererID, GL_INFO_LOG_LENGTH, &maxLength);
			std::vector<char> infoLog(maxLength);
			glGetProgramInfoLog(m_RendererID, maxLength, &maxLength, &infoLog[0]);

			CH_CORE_ERROR("[Shader] Linking Error:\n{}", infoLog.data());

			glDeleteProgram(m_RendererID);
			m_RendererID = 0;
			return;
		}

		glDeleteShader(vs);
		glDeleteShader(fs);
	}

	GLShaderProgram::~GLShaderProgram()
	{
		if (m_RendererID > 0)
		{
			uint32_t id = m_RendererID;
			GraphicsDevice::EnqueueResourceDeletion([id]() { glDeleteProgram(id); });
		}
	}

	void GLShaderProgram::Bind() const
	{
		glUseProgram(m_RendererID);
	}

	void GLShaderProgram::Unbind() const
	{
		glUseProgram(0);
	}

	int GLShaderProgram::GetLocation(const std::string& name)
	{
		auto it = m_UniformCache.find(name);
		if (it != m_UniformCache.end())
		{
			return it->second;
		}

		int location = glGetUniformLocation(m_RendererID, name.c_str());
		m_UniformCache[name] = location;
		return location;
	}

	void GLShaderProgram::SetFloat(const std::string& name, float value)
	{
		int location = GetLocation(name);
		if (location >= 0)
		{
			glProgramUniform1f(m_RendererID, location, value);
		}
	}

	void GLShaderProgram::SetInt(const std::string& name, int value)
	{
		int location = GetLocation(name);
		if (location >= 0)
		{
			glProgramUniform1i(m_RendererID, location, value);
		}
	}

	void GLShaderProgram::SetVec2(const std::string& name, const glm::vec2& value)
	{
		int location = GetLocation(name);
		if (location >= 0)
		{
			glProgramUniform2fv(m_RendererID, location, 1, glm::value_ptr(value));
		}
	}

	void GLShaderProgram::SetVec3(const std::string& name, const glm::vec3& value)
	{
		int location = GetLocation(name);
		if (location >= 0)
		{
			glProgramUniform3fv(m_RendererID, location, 1, glm::value_ptr(value));
		}
	}

	void GLShaderProgram::SetVec4(const std::string& name, const glm::vec4& value)
	{
		int location = GetLocation(name);
		if (location >= 0)
		{
			glProgramUniform4fv(m_RendererID, location, 1, glm::value_ptr(value));
		}
	}

	void GLShaderProgram::SetColor(const std::string& name, const Color& value)
	{
		glm::vec4 c(value.r / 255.0f, value.g / 255.0f, value.b / 255.0f, value.a / 255.0f);
		SetVec4(name, c);
	}

	void GLShaderProgram::SetMatrix(const std::string& name, const glm::mat4& value)
	{
		int location = GetLocation(name);
		if (location >= 0)
		{
			glProgramUniformMatrix4fv(m_RendererID, location, 1, GL_FALSE, glm::value_ptr(value));
		}
	}

	void GLShaderProgram::SetMatrices(const std::string& name, const glm::mat4* values, int count)
	{
		int location = GetLocation(name);
		if (location >= 0)
		{
			glProgramUniformMatrix4fv(m_RendererID, location, count, GL_FALSE, glm::value_ptr(values[0]));
		}
	}

} // namespace Chained
