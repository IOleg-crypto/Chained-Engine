#include "engine/assets/loaders/texture_loader.h"

#include "engine/core/service_locator.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/types/texture_asset.h"

#include <basisu_transcoder.h>
#include "stb_image_impl.h"
#include <fstream>
#include <mutex>

namespace Chained
{
	void TextureLoader::EnsureBasisuInit()
	{
		basist::basisu_transcoder_init();
	}

	void TextureLoader::FlipImageVertically(void* pixels, int width, int height, int channels, size_t bytesPerChannel)
	{
		if (pixels == nullptr || width <= 0 || height <= 0 || channels <= 0)
		{
			return;
		}

		const size_t rowBytes = static_cast<size_t>(width) * static_cast<size_t>(channels) * bytesPerChannel;
		std::vector<unsigned char> temp(rowBytes);
		auto* bytes = static_cast<unsigned char*>(pixels);

		for (int row = 0; row < height / 2; ++row)
		{
			const size_t topOffset = static_cast<size_t>(row) * rowBytes;
			const size_t bottomOffset = static_cast<size_t>(height - row - 1) * rowBytes;

			std::memcpy(temp.data(), bytes + topOffset, rowBytes);
			std::memcpy(bytes + topOffset, bytes + bottomOffset, rowBytes);
			std::memcpy(bytes + bottomOffset, temp.data(), rowBytes);
		}
	}

	bool TextureLoader::TryTranscodeKTX2(const void* data, size_t dataSize, std::shared_ptr<TextureAsset> texAsset,
										 bool flipY)
	{
		if (!data || dataSize < 12)
		{
			return false;
		}

		// KTX2 magic identifier: "\xABKTX 20\xBB\r\n\x1A\n"
		static const uint8_t ktx2Magic[12] = {0xAB, 0x4B, 0x54, 0x58, 0x20, 0x32, 0x30, 0xBB, 0x0D, 0x0A, 0x1A, 0x0A};
		if (std::memcmp(data, ktx2Magic, 12) != 0)
		{
			return false;
		}

		EnsureBasisuInit();

		basist::ktx2_transcoder transcoder;
		if (!transcoder.init(data, static_cast<uint32_t>(dataSize)))
		{
			return false;
		}

		if (!transcoder.start_transcoding())
		{
			return false;
		}

		uint32_t width = transcoder.get_width();
		uint32_t height = transcoder.get_height();

		// Try BC7 GPU-compressed transcode first (1 byte/pixel, direct GPU upload)
		basist::transcoder_texture_format targetFormat = basist::transcoder_texture_format::cTFBC7_RGBA;
		uint32_t blockCount = ((width + 3) / 4) * ((height + 3) / 4);
		uint32_t compressedBytes = blockCount * 16;

		void* gpuBuffer = std::malloc(compressedBytes);
		if (!gpuBuffer)
		{
			return false;
		}

		if (!transcoder.transcode_image_level(0, 0, 0, gpuBuffer, blockCount, targetFormat, 0))
		{
			// BC7 failed — fall back to uncompressed RGBA32
			std::free(gpuBuffer);
			targetFormat = basist::transcoder_texture_format::cTFRGBA32;
			compressedBytes = width * height * 4;
			gpuBuffer = std::malloc(compressedBytes);
			if (!gpuBuffer)
			{
				return false;
			}

			if (!transcoder.transcode_image_level(0, 0, 0, gpuBuffer, width * height, targetFormat, 0))
			{
				std::free(gpuBuffer);
				return false;
			}
		}

		DecodedImage rawImage;
		rawImage.data = gpuBuffer;
		rawImage.width = static_cast<int>(width);
		rawImage.height = static_cast<int>(height);
		rawImage.channels = 4;
		rawImage.isHDR = false;
		rawImage.format = 0;
		rawImage.mipmaps = 1;

		if (targetFormat == basist::transcoder_texture_format::cTFBC7_RGBA)
		{
			// BC7 compressed — upload directly to GPU
			rawImage.isCompressedGPU = true;
			rawImage.compressedFormat = TextureFormat::BC7;
			rawImage.compressedDataSize = compressedBytes;
		}
		else
		{
			// RGBA8 uncompressed — apply Y-flip if needed
			if (flipY)
			{
				FlipImageVertically(gpuBuffer, static_cast<int>(width), static_cast<int>(height), 4,
									sizeof(unsigned char));
			}
			rawImage.isCompressedGPU = false;
			rawImage.compressedFormat = TextureFormat::RGBA8;
			rawImage.compressedDataSize = compressedBytes;
		}

		texAsset->SetIsHDR(false);
		texAsset->SetPendingImage(rawImage);
		return true;
	}

	std::shared_ptr<Asset> TextureLoader::Create()
	{
		return std::make_shared<TextureAsset>();
	}

	bool TextureLoader::Load(std::shared_ptr<Asset> asset, const std::string& resolvedPath, std::string* outError)
	{
		auto texAsset = std::static_pointer_cast<TextureAsset>(asset);

		if (resolvedPath.empty())
		{
			if (outError)
			{
				*outError = "TextureLoader: empty path";
			}
			return false;
		}

		std::string ext = std::filesystem::path(resolvedPath).extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		int width, height, channels;

		// Check if we should read from pack or filesystem
		auto* assetManager = ServiceLocator::TryGet<AssetManager>();
		bool usePack = assetManager && assetManager->IsPacked();

		// Determine if HDR by checking file header or extension.
		// This is intentionally per-asset and not a single global rule: UI and editor
		// textures often need their source pixels kept in the original orientation,
		// while scene/game textures are typically loaded to OpenGL's bottom-left UV convention.
		bool isHDR = (ext == ".hdr" || ext == ".exr");

		const auto isUiLikePath = [](const std::string& path) {
			const std::string lower = [&]() {
				std::string result = path;
				std::transform(result.begin(), result.end(), result.begin(), ::tolower);
				return result;
			}();

			return lower.find("/ui/") != std::string::npos || lower.find("\\ui\\") != std::string::npos ||
				   lower.find("/icons/") != std::string::npos || lower.find("\\icons\\") != std::string::npos ||
				   lower.find("/font/") != std::string::npos || lower.find("\\font\\") != std::string::npos ||
				   lower.find("logo") != std::string::npos || lower.find("menu") != std::string::npos;
		};

		const TextureUsage usage = isUiLikePath(resolvedPath) ? TextureUsage::UI : TextureUsage::Scene;
		texAsset->SetUsage(usage);

		const bool shouldFlipVertically = !isHDR && usage == TextureUsage::Scene &&
										  (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
										   ext == ".tga" || ext == ".gif" || ext == ".webp");
		texAsset->SetFlipYOnLoad(shouldFlipVertically);

		void* data = nullptr;

		if (usePack)
		{
			auto fileData = assetManager->ReadAssetData(resolvedPath);
			if (!fileData.empty())
			{
				// Check for Khronos KTX2 container
				if (TryTranscodeKTX2(fileData.data(), fileData.size(), texAsset, shouldFlipVertically))
				{
					return true;
				}

				if (isHDR)
				{
					data = stbi_loadf_from_memory(fileData.data(), (int)fileData.size(), &width, &height, &channels, 0);
				}
				else
				{
					data = stbi_load_from_memory(fileData.data(), (int)fileData.size(), &width, &height, &channels, 4);
					channels = 4;
				}
			}
		}
		else
		{
			// Check if file on disk is KTX2
			{
				std::ifstream diskFile(resolvedPath, std::ios::binary | std::ios::ate);
				if (diskFile.is_open())
				{
					std::streamsize sz = diskFile.tellg();
					if (sz >= 12)
					{
						diskFile.seekg(0, std::ios::beg);
						std::vector<uint8_t> fileBytes(static_cast<size_t>(sz));
						if (diskFile.read(reinterpret_cast<char*>(fileBytes.data()), sz))
						{
							if (TryTranscodeKTX2(fileBytes.data(), fileBytes.size(), texAsset, shouldFlipVertically))
							{
								return true;
							}
						}
					}
				}
			}

			isHDR = stbi_is_hdr(resolvedPath.c_str());
			const TextureUsage usage = isUiLikePath(resolvedPath) ? TextureUsage::UI : TextureUsage::Scene;
			texAsset->SetUsage(usage);

			const bool shouldFlipVertically = !isHDR && usage == TextureUsage::Scene &&
											  (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" ||
											   ext == ".tga" || ext == ".gif" || ext == ".webp");
			texAsset->SetFlipYOnLoad(shouldFlipVertically);

			if (isHDR)
			{
				data = stbi_loadf(resolvedPath.c_str(), &width, &height, &channels, 0);
			}
			else
			{
				data = stbi_load(resolvedPath.c_str(), &width, &height, &channels, 4);
				channels = 4;
			}
		}

		if (shouldFlipVertically && data != nullptr)
		{
			if (isHDR)
			{
				FlipImageVertically(data, width, height, channels > 0 ? channels : 4, sizeof(float));
			}
			else
			{
				FlipImageVertically(data, width, height, channels > 0 ? channels : 4, sizeof(unsigned char));
			}
		}

		if (data == nullptr)
		{
			const char* reason = stbi_failure_reason();
			CH_CORE_ERROR("TextureLoader: Failed to load image {}. Reason: {}", resolvedPath,
						  reason ? reason : "Unknown");
			if (outError)
			{
				*outError = "TextureLoader: failed to load image '" + resolvedPath +
							"'. Reason: " + (reason ? reason : "Unknown");
			}
			return false;
		}

		texAsset->SetIsHDR(isHDR);

		DecodedImage rawImage;
		rawImage.data = data;
		rawImage.width = width;
		rawImage.height = height;
		rawImage.channels = channels;
		rawImage.isHDR = isHDR;
		rawImage.format = isHDR ? 11 : 7; // Matching previous constants
		rawImage.mipmaps = 1;

		texAsset->SetPendingImage(rawImage);
		return true;
	}
} // namespace Chained