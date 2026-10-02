#include "gl_device.h"
#include "engine/graphics/api/vertex_array.h"
#include <glad/gl.h>

namespace Chained
{

	void GLDevice::Init()
	{
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LEQUAL);
#ifdef GLM_FORCE_DEPTH_ZERO_TO_ONE
		if (glClipControl)
		{
			glClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);
		}
#endif
		SetCullMode(CullMode::Back);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

		// Sync the state cache with what we just set above via raw GL calls,
		// so that IsBlendEnabled(), IsDepthTestEnabled(), etc. return correct
		// values immediately after Init() without requiring a round-trip to GL.
		m_StateCache.Blend = true;
		m_StateCache.SrcBlend = BlendFactor::SrcAlpha;
		m_StateCache.DstBlend = BlendFactor::OneMinusSrcAlpha;
		m_StateCache.DepthTest = true;
		m_StateCache.DepthMask = true;
		m_StateCache.DepthFunction = DepthFunc::LEqual;
		m_StateCache.PolyMode = PolygonMode::Fill;
	}

	void GLDevice::Shutdown()
	{
	}

	void GLDevice::SetViewport(int x, int y, int width, int height)
	{
		glViewport(x, y, width, height);
		m_StateCache.Viewport[0] = x;
		m_StateCache.Viewport[1] = y;
		m_StateCache.Viewport[2] = width;
		m_StateCache.Viewport[3] = height;
	}

	void GLDevice::GetViewport(int* x, int* y, int* width, int* height) const
	{
		// Query GL directly rather than the state cache: Framebuffer::Bind() calls glViewport()
		// without going through SetViewport(), so the cache can be stale. Mirrors how
		// GetFramebufferBinding() reads the live GL state.
		GLint vp[4] = {0, 0, 0, 0};
		glGetIntegerv(GL_VIEWPORT, vp);
		if (x)
		{
			*x = vp[0];
		}
		if (y)
		{
			*y = vp[1];
		}
		if (width)
		{
			*width = vp[2];
		}
		if (height)
		{
			*height = vp[3];
		}
	}

	void GLDevice::SetClearColor(const Color& color)
	{
		m_ClearColor[0] = color.r / 255.0f;
		m_ClearColor[1] = color.g / 255.0f;
		m_ClearColor[2] = color.b / 255.0f;
		m_ClearColor[3] = color.a / 255.0f;
	}

	void GLDevice::Clear()
	{
		glClearColor(m_ClearColor[0], m_ClearColor[1], m_ClearColor[2], m_ClearColor[3]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	}

	void GLDevice::SetDepthFunc(DepthFunc func)
	{
		if (m_StateCache.DepthFunction == func)
		{
			return;
		}

		GLenum glFunc = GL_LESS;
		switch (func)
		{
		case DepthFunc::Never:
			glFunc = GL_NEVER;
			break;
		case DepthFunc::Less:
			glFunc = GL_LESS;
			break;
		case DepthFunc::Equal:
			glFunc = GL_EQUAL;
			break;
		case DepthFunc::LEqual:
			glFunc = GL_LEQUAL;
			break;
		case DepthFunc::Greater:
			glFunc = GL_GREATER;
			break;
		case DepthFunc::NotEqual:
			glFunc = GL_NOTEQUAL;
			break;
		case DepthFunc::GEqual:
			glFunc = GL_GEQUAL;
			break;
		case DepthFunc::Always:
			glFunc = GL_ALWAYS;
			break;
		}
		glDepthFunc(glFunc);
		m_StateCache.DepthFunction = func;
	}

	void GLDevice::SetDepthTest(bool enabled)
	{
		if (m_StateCache.DepthTest == enabled)
		{
			return;
		}

		if (enabled)
		{
			glEnable(GL_DEPTH_TEST);
		}
		else
		{
			glDisable(GL_DEPTH_TEST);
		}

		m_StateCache.DepthTest = enabled;
	}

	void GLDevice::SetDepthMask(bool enabled)
	{
		if (m_StateCache.DepthMask == enabled)
		{
			return;
		}

		glDepthMask(enabled ? GL_TRUE : GL_FALSE);
		m_StateCache.DepthMask = enabled;
	}

	void GLDevice::SetCullMode(CullMode mode)
	{
		if (m_StateCache.Cull == mode)
		{
			return;
		}

		if (mode == CullMode::None)
		{
			glDisable(GL_CULL_FACE);
		}
		else
		{
			glEnable(GL_CULL_FACE);
			switch (mode)
			{
			case CullMode::Front:
				glCullFace(GL_FRONT);
				break;
			case CullMode::Back:
				glCullFace(GL_BACK);
				break;
			case CullMode::FrontAndBack:
				glCullFace(GL_FRONT_AND_BACK);
				break;
			}
		}
		m_StateCache.Cull = mode;
	}

	void GLDevice::SetBlendEnabled(bool enabled)
	{
		if (m_StateCache.Blend == enabled)
		{
			return;
		}

		if (enabled)
		{
			glEnable(GL_BLEND);
		}
		else
		{
			glDisable(GL_BLEND);
		}

		m_StateCache.Blend = enabled;
	}

	void GLDevice::SetBlendFunc(BlendFactor src, BlendFactor dst)
	{
		if (m_StateCache.SrcBlend == src && m_StateCache.DstBlend == dst)
		{
			return;
		}

		auto toGL = [](BlendFactor factor) -> GLenum {
			switch (factor)
			{
			case BlendFactor::Zero:
				return GL_ZERO;
			case BlendFactor::One:
				return GL_ONE;
			case BlendFactor::SrcColor:
				return GL_SRC_COLOR;
			case BlendFactor::OneMinusSrcColor:
				return GL_ONE_MINUS_SRC_COLOR;
			case BlendFactor::DstColor:
				return GL_DST_COLOR;
			case BlendFactor::OneMinusDstColor:
				return GL_ONE_MINUS_DST_COLOR;
			case BlendFactor::SrcAlpha:
				return GL_SRC_ALPHA;
			case BlendFactor::OneMinusSrcAlpha:
				return GL_ONE_MINUS_SRC_ALPHA;
			case BlendFactor::DstAlpha:
				return GL_DST_ALPHA;
			case BlendFactor::OneMinusDstAlpha:
				return GL_ONE_MINUS_DST_ALPHA;
			}
			return GL_ONE;
		};
		glBlendFunc(toGL(src), toGL(dst));
		m_StateCache.SrcBlend = src;
		m_StateCache.DstBlend = dst;
	}

	void GLDevice::SetLineWidth(float width)
	{
		if (width < 1.0f)
		{
			width = 1.0f;
		}
		glLineWidth(width);
	}

	void GLDevice::DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray, uint32_t indexCount)
	{
		vertexArray->Bind();
		auto ib = vertexArray->GetIndexBuffer();
		if (!ib)
		{
			return;
		}
		uint32_t count = indexCount ? indexCount : ib->GetCount();
		glDrawElements(GL_TRIANGLES, (GLsizei)count, GL_UNSIGNED_INT, nullptr);
	}

	void GLDevice::DrawIndexedLines(const std::shared_ptr<VertexArray>& vertexArray, uint32_t indexCount)
	{
		vertexArray->Bind();
		auto ib = vertexArray->GetIndexBuffer();
		if (!ib)
		{
			return;
		}
		uint32_t count = indexCount ? indexCount : ib->GetCount();
		glDrawElements(GL_LINES, (GLsizei)count, GL_UNSIGNED_INT, nullptr);
	}

	void GLDevice::DrawLines(const std::shared_ptr<VertexArray>& vertexArray, uint32_t vertexCount)
	{
		vertexArray->Bind();
		glDrawArrays(GL_LINES, 0, (GLsizei)vertexCount);
	}

	void GLDevice::SetPolygonMode(PolygonMode mode)
	{
		if (m_StateCache.PolyMode == mode)
		{
			return;
		}

		GLenum glMode = GL_FILL;
		switch (mode)
		{
		case PolygonMode::Fill:
			glMode = GL_FILL;
			break;
		case PolygonMode::Line:
			glMode = GL_LINE;
			break;
		case PolygonMode::Point:
			glMode = GL_POINT;
			break;
		}
		glPolygonMode(GL_FRONT_AND_BACK, glMode);
		m_StateCache.PolyMode = mode;
	}

	void GLDevice::SetPolygonOffset(bool enabled, float factor, float units)
	{
		if (enabled)
		{
			glEnable(GL_POLYGON_OFFSET_FILL);
			glEnable(GL_POLYGON_OFFSET_LINE);
			glEnable(GL_POLYGON_OFFSET_POINT);
			glPolygonOffset(factor, units);
		}
		else
		{
			glDisable(GL_POLYGON_OFFSET_FILL);
			glDisable(GL_POLYGON_OFFSET_LINE);
			glDisable(GL_POLYGON_OFFSET_POINT);
		}
	}

	void GLDevice::DrawArrays(uint32_t vertexCount)
	{
		glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertexCount);
	}

	void GLDevice::DrawIndexedInstanced(const std::shared_ptr<VertexArray>& vertexArray, uint32_t instanceCount,
										uint32_t indexCount)
	{
		vertexArray->Bind();
		auto ib = vertexArray->GetIndexBuffer();
		if (!ib)
		{
			return;
		}
		uint32_t count = indexCount ? indexCount : ib->GetCount();
		glDrawElementsInstanced(GL_TRIANGLES, (GLsizei)count, GL_UNSIGNED_INT, nullptr, (GLsizei)instanceCount);
	}

	void GLDevice::DrawArraysInstanced(uint32_t vertexCount, uint32_t instanceCount)
	{
		glDrawArraysInstanced(GL_TRIANGLES, 0, (GLsizei)vertexCount, (GLsizei)instanceCount);
	}

	void GLDevice::SetTexture(uint32_t slot, uint32_t textureId, bool isCubemap)
	{
		auto it = m_StateCache.BoundTextures.find(slot);
		if (it != m_StateCache.BoundTextures.end() && it->second == textureId)
		{
			return;
		}

		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(isCubemap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D, textureId);
		m_StateCache.BoundTextures[slot] = textureId;
	}

	bool GLDevice::IsDepthTestEnabled() const
	{
		return m_StateCache.DepthTest;
	}

	bool GLDevice::IsDepthMaskEnabled() const
	{
		return m_StateCache.DepthMask;
	}

	bool GLDevice::IsBlendEnabled() const
	{
		return m_StateCache.Blend;
	}

	bool GLDevice::IsCullFaceEnabled() const
	{
		return m_StateCache.Cull != CullMode::None;
	}

	GraphicsDevice::PolygonMode GLDevice::GetPolygonMode() const
	{
		return m_StateCache.PolyMode;
	}

	uint32_t GLDevice::GetFramebufferBinding() const
	{
		GLint fbo = 0;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
		return static_cast<uint32_t>(fbo);
	}

	void GLDevice::BindFramebuffer(uint32_t fbo)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	}

	void GLDevice::ClearDepth()
	{
		glClear(GL_DEPTH_BUFFER_BIT);
	}

} // namespace Chained
