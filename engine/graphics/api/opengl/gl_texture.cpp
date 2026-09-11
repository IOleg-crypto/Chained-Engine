#include "gl_texture.h"
#include "engine/graphics/api/graphics_device.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/service_locator.h"
#include "engine/core/log.h"
#include <stb_image.h>

namespace Chained
{

	static uint32_t CalculateMipLevels(uint32_t width, uint32_t height)
	{
		return static_cast<uint32_t>(1 + std::floor(std::log2(std::max(width, height))));
	}

	GLTexture::GLTexture(uint32_t width, uint32_t height, TextureFormat format)
		: m_Width(width),
		  m_Height(height),
		  m_Format(format),
		  m_Type(TextureType::Texture2D)
	{
		m_InternalFormat = GL_RGBA8;
		m_DataFormat = GL_RGBA;

		switch (format)
		{
		case TextureFormat::RGB8:
			m_InternalFormat = GL_RGB8;
			m_DataFormat = GL_RGB;
			break;
		case TextureFormat::RGBA8:
			m_InternalFormat = GL_RGBA8;
			m_DataFormat = GL_RGBA;
			break;
		case TextureFormat::RGB16F:
			m_InternalFormat = GL_RGB16F;
			m_DataFormat = GL_RGB;
			break;
		case TextureFormat::RGBA16F:
			m_InternalFormat = GL_RGBA16F;
			m_DataFormat = GL_RGBA;
			break;
		case TextureFormat::BC3:
			m_InternalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
			m_DataFormat = GL_RGBA;
			break;
		case TextureFormat::BC7:
			m_InternalFormat = GL_COMPRESSED_RGBA_BPTC_UNORM;
			m_DataFormat = GL_RGBA;
			break;
		default:
			break;
		}

		glGenTextures(1, &m_RendererID);
		glBindTexture(GL_TEXTURE_2D, m_RendererID);

		if (format != TextureFormat::BC3 && format != TextureFormat::BC7)
		{
			uint32_t mipLevels = CalculateMipLevels(m_Width, m_Height);
			glTexStorage2D(GL_TEXTURE_2D, mipLevels, m_InternalFormat, m_Width, m_Height);
		}

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

		if (format != TextureFormat::BC3 && format != TextureFormat::BC7)
		{
			m_IsReady = true; // BC formats: ready after SetCompressedData is called
		}
	}

	GLTexture::GLTexture(uint32_t size, TextureFormat format)
		: m_Width(size),
		  m_Height(size),
		  m_Format(format),
		  m_Type(TextureType::Cubemap)
	{
		m_InternalFormat = GL_RGB16F;
		m_DataFormat = GL_RGB;

		switch (format)
		{
		case TextureFormat::RGB16F:
			m_InternalFormat = GL_RGB16F;
			m_DataFormat = GL_RGB;
			break;
		case TextureFormat::RGBA16F:
			m_InternalFormat = GL_RGBA16F;
			m_DataFormat = GL_RGBA;
			break;
		}

		glGenTextures(1, &m_RendererID);
		glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);

		for (uint32_t i = 0; i < 6; ++i)
		{
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, m_InternalFormat, m_Width, m_Height, 0, m_DataFormat,
						 GL_FLOAT, nullptr);
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		m_IsReady = true;
	}

	GLTexture::GLTexture(uint32_t handle, uint32_t width, uint32_t height, TextureType type, bool ownsResource)
		: m_RendererID(handle),
		  m_Width(width),
		  m_Height(height),
		  m_Format(TextureFormat::RGBA8),
		  m_Type(type),
		  m_IsReady(true),
		  m_OwnsResource(ownsResource)
	{
		m_InternalFormat = GL_RGBA8;
		m_DataFormat = GL_RGBA;
	}

	GLTexture::~GLTexture()
	{
		if (m_OwnsResource && m_RendererID)
		{
			uint32_t id = m_RendererID;
			GraphicsDevice::EnqueueResourceDeletion([id]() { glDeleteTextures(1, &id); });
		}
	}

	void GLTexture::SetData(void* data, uint32_t size)
	{
		GLenum dataType =
			(m_Format == TextureFormat::RGB16F || m_Format == TextureFormat::RGBA16F) ? GL_FLOAT : GL_UNSIGNED_BYTE;

		if (m_Type == TextureType::Cubemap)
		{
			// bpp = bytes per pixel: float formats use 4 bytes per component, byte formats use 1.
			uint32_t bytesPerComponent =
				(m_Format == TextureFormat::RGB16F || m_Format == TextureFormat::RGBA16F) ? sizeof(float) : 1;
			uint32_t channels = (m_DataFormat == GL_RGBA) ? 4 : 3;
			uint32_t faceSize = m_Width * m_Height * channels * bytesPerComponent;
			uint8_t* ptr = static_cast<uint8_t*>(data);
			glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
			for (uint32_t i = 0; i < 6; ++i)
			{
				glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, 0, 0, m_Width, m_Height, m_DataFormat, dataType,
								ptr + i * faceSize);
			}
			glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
		}
		else
		{
			glBindTexture(GL_TEXTURE_2D, m_RendererID);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, m_DataFormat, dataType, data);
			glGenerateMipmap(GL_TEXTURE_2D);
		}
	}

	void GLTexture::SetCompressedData(const void* data, uint32_t dataSize)
	{
		// Upload BC7 (BPTC) or BC3 (DXT5) blocks directly to GPU.
		// Note: glGenerateMipmap is invalid on block-compressed textures in OpenGL,
		// so we set GL_LINEAR filter to ensure the texture is complete without mipmaps.
		glBindTexture(GL_TEXTURE_2D, m_RendererID);
		GLenum internalFmt =
			(m_Format == TextureFormat::BC7) ? GL_COMPRESSED_RGBA_BPTC_UNORM : GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
		glCompressedTexImage2D(GL_TEXTURE_2D, 0, internalFmt, (GLsizei)m_Width, (GLsizei)m_Height, 0, (GLsizei)dataSize,
							   data);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		m_IsReady = true;
	}

	void GLTexture::Bind(uint32_t slot) const
	{
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(m_Type == TextureType::Cubemap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D, m_RendererID);
	}

	std::shared_ptr<Texture> Texture::CreateCubemapFromFiles(const std::string faces[6])
	{
		if (GraphicsDevice::GetAPI() != GraphicsDevice::API::OpenGL)
		{
			return nullptr;
		}

		uint32_t textureID = 0;
		glGenTextures(1, &textureID);
		glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

		stbi_set_flip_vertically_on_load(false);

		int width = 0, height = 0;
		auto* assetManager = ServiceLocator::TryGet<AssetManager>();

		for (uint32_t i = 0; i < 6; ++i)
		{
			std::string resolvedPath = faces[i];
			if (assetManager && !resolvedPath.empty())
			{
				resolvedPath = assetManager->ResolvePath(resolvedPath);
			}

			int w = 0, h = 0, channels = 0;
			unsigned char* data = nullptr;

			if (!resolvedPath.empty())
			{
				if (assetManager && assetManager->IsPacked())
				{
					auto fileData = assetManager->ReadAssetData(faces[i]);
					if (!fileData.empty())
					{
						data = stbi_load_from_memory(fileData.data(), (int)fileData.size(), &w, &h, &channels, 4);
						channels = 4;
					}
				}
				else
				{
					data = stbi_load(resolvedPath.c_str(), &w, &h, &channels, 4);
					channels = 4;
				}
			}

			if (data)
			{
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
				stbi_image_free(data);
				if (width == 0)
				{
					width = w;
					height = h;
				}
			}
			else
			{
				CH_CORE_WARN("GLTexture: Cubemap face {} failed to load: {}", i, faces[i]);
				uint32_t black = 0xFF000000;
				glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE,
							 &black);
				if (width == 0)
				{
					width = 1;
					height = 1;
				}
			}
		}

		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

		return std::make_shared<GLTexture>(textureID, (uint32_t)width, (uint32_t)height, TextureType::Cubemap, true);
	}

} // namespace Chained
