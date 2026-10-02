#include "gl_framebuffer.h"
#include "engine/graphics/api/graphics_device.h"

#include <glad/gl.h>

namespace Chained
{
	GLFramebuffer::GLFramebuffer(const FramebufferSpecification& spec)
		: m_Specification(spec)
	{
		Invalidate();
	}

	GLFramebuffer::~GLFramebuffer()
	{
		uint32_t fbo = m_RendererID;
		uint32_t color = m_ColorAttachment;
		uint32_t depth = m_DepthAttachment;
		uint32_t resolveFbo = m_ResolveFBO;
		uint32_t resolveColor = m_ResolveColorAttachment;
		uint32_t resolveDepth = m_ResolveDepthAttachment;

		GraphicsDevice::EnqueueResourceDeletion([fbo, color, depth, resolveFbo, resolveColor, resolveDepth]() {
			if (fbo)
			{
				glDeleteFramebuffers(1, &fbo);
			}
			if (color)
			{
				glDeleteTextures(1, &color);
			}
			if (depth)
			{
				glDeleteTextures(1, &depth);
			}
			if (resolveFbo)
			{
				glDeleteFramebuffers(1, &resolveFbo);
			}
			if (resolveColor)
			{
				glDeleteTextures(1, &resolveColor);
			}
			if (resolveDepth)
			{
				glDeleteTextures(1, &resolveDepth);
			}
		});
	}

	void GLFramebuffer::Invalidate()
	{
		GLint previousFBO = 0;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFBO);

		if (m_RendererID)
		{
			glDeleteFramebuffers(1, &m_RendererID);
			glDeleteTextures(1, &m_ColorAttachment);
			glDeleteTextures(1, &m_DepthAttachment);
			m_RendererID = 0;
			m_ColorAttachment = 0;
			m_DepthAttachment = 0;
		}
		if (m_ResolveFBO)
		{
			glDeleteFramebuffers(1, &m_ResolveFBO);
			glDeleteTextures(1, &m_ResolveColorAttachment);
			glDeleteTextures(1, &m_ResolveDepthAttachment);
			m_ResolveFBO = 0;
			m_ResolveColorAttachment = 0;
			m_ResolveDepthAttachment = 0;
		}

		m_IsComplete = false;

		if (m_Specification.Width == 0 || m_Specification.Height == 0)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, previousFBO);
			return;
		}

		// DepthOnly (shadow map) framebuffers are never multisampled - sample-shadow lookups
		// need a single-sample sampler2D/sampler2DShadow.
		GLint maxSamples = 1;
		glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
		uint32_t clampedMsaaSampleCount =
			m_Specification.DepthOnly ? 1 : std::min<uint32_t>(m_Specification.Samples, (uint32_t)maxSamples);
		bool multisampled = clampedMsaaSampleCount > 1;

		glGenFramebuffers(1, &m_RendererID);
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);

		GLenum internalFormat = GL_RGBA8;
		GLenum dataType = GL_UNSIGNED_BYTE;
		if (m_Specification.ColorFormat == FramebufferColorFormat::RGBA16F)
		{
			internalFormat = GL_RGBA16F;
			dataType = GL_FLOAT;
		}

		if (m_Specification.DepthOnly)
		{
			// ── Depth-only path (shadow map) ────────────────────────────────
			// No color attachment; depth is written to a sampable 2D texture.
			glGenTextures(1, &m_DepthAttachment);
			glBindTexture(GL_TEXTURE_2D, m_DepthAttachment);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, m_Specification.Width, m_Specification.Height, 0,
						 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			// Clamp-to-border with depth=1 so fragments outside the light frustum are NOT in shadow.
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
			float border[] = {1.0f, 1.0f, 1.0f, 1.0f};
			glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);

			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_DepthAttachment, 0);

			// No color buffer
			glDrawBuffer(GL_NONE);
			glReadBuffer(GL_NONE);
		}
		else
		{
			if (multisampled)
			{
				// ── Multisample color + depth path ──────────────────────────
				GLenum target = GL_TEXTURE_2D_MULTISAMPLE;

				glGenTextures(1, &m_ColorAttachment);
				glBindTexture(target, m_ColorAttachment);
				glTexImage2DMultisample(target, clampedMsaaSampleCount, internalFormat, m_Specification.Width,
										m_Specification.Height, GL_TRUE);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, target, m_ColorAttachment, 0);

				glGenTextures(1, &m_DepthAttachment);
				glBindTexture(target, m_DepthAttachment);
				glTexImage2DMultisample(target, clampedMsaaSampleCount, GL_DEPTH24_STENCIL8, m_Specification.Width,
										m_Specification.Height, GL_TRUE);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, target, m_DepthAttachment, 0);
			}
			else
			{
				// ── Standard color + depth path ─────────────────────────────────
				// Color Attachment
				glGenTextures(1, &m_ColorAttachment);
				glBindTexture(GL_TEXTURE_2D, m_ColorAttachment);
				glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_Specification.Width, m_Specification.Height, 0,
							 GL_RGBA, dataType, nullptr);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorAttachment, 0);

				// Depth Attachment
				glGenTextures(1, &m_DepthAttachment);
				glBindTexture(GL_TEXTURE_2D, m_DepthAttachment);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, m_Specification.Width, m_Specification.Height, 0,
							 GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
				glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_DepthAttachment,
									   0);
			}
		}

		GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
		if (status != GL_FRAMEBUFFER_COMPLETE)
		{
			CH_CORE_ERROR("Framebuffer is incomplete! Status: 0x{0:X}, Width: {1}, Height: {2}", status,
						  m_Specification.Width, m_Specification.Height);
			// Do NOT assert here — a non-complete FBO should degrade gracefully, not crash the editor.
		}
		else
		{
			m_IsComplete = true;
		}

		// Multisample attachments can't be sampled with sampler2D - build a same-size single-sample
		// resolve target that Resolve() blits into, so callers keep reading a plain 2D texture.
		if (multisampled && m_IsComplete)
		{
			glGenFramebuffers(1, &m_ResolveFBO);
			glBindFramebuffer(GL_FRAMEBUFFER, m_ResolveFBO);

			glGenTextures(1, &m_ResolveColorAttachment);
			glBindTexture(GL_TEXTURE_2D, m_ResolveColorAttachment);
			glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_Specification.Width, m_Specification.Height, 0, GL_RGBA,
						 dataType, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ResolveColorAttachment, 0);

			glGenTextures(1, &m_ResolveDepthAttachment);
			glBindTexture(GL_TEXTURE_2D, m_ResolveDepthAttachment);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, m_Specification.Width, m_Specification.Height, 0,
						 GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_ResolveDepthAttachment,
								   0);

			GLenum resolveStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
			if (resolveStatus != GL_FRAMEBUFFER_COMPLETE)
			{
				CH_CORE_ERROR("MSAA resolve framebuffer is incomplete! Status: 0x{0:X}", resolveStatus);
			}
		}

		if (m_IsComplete)
		{
			m_ColorTexture =
				Texture::WrapNative(m_Specification.Samples > 1 ? m_ResolveColorAttachment : m_ColorAttachment,
									m_Specification.Width, m_Specification.Height);
			m_DepthTexture =
				Texture::WrapNative(m_Specification.Samples > 1 ? m_ResolveDepthAttachment : m_DepthAttachment,
									m_Specification.Width, m_Specification.Height);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, previousFBO);
	}

	void GLFramebuffer::Bind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
		glViewport(0, 0, m_Specification.Width, m_Specification.Height);
	}

	void GLFramebuffer::Unbind()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}

	void GLFramebuffer::Resolve()
	{
		if (m_Specification.Samples <= 1 || !m_ResolveFBO || !m_IsComplete)
		{
			return;
		}

		GLint previousFBO = 0;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFBO);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_RendererID);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_ResolveFBO);
		glBlitFramebuffer(0, 0, m_Specification.Width, m_Specification.Height, 0, 0, m_Specification.Width,
						  m_Specification.Height, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT,
						  GL_NEAREST);

		glBindFramebuffer(GL_FRAMEBUFFER, previousFBO);
	}

	void GLFramebuffer::Resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0 || width > 8192 || height > 8192)
		{
			return;
		}
		m_Specification.Width = width;
		m_Specification.Height = height;
		Invalidate();
	}

} // namespace Chained
