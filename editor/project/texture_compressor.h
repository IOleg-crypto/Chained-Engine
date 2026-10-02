#pragma once

#include "project_exporter.h"

#include <atomic>
#include <filesystem>
#include <vector>

namespace Chained
{
	namespace TextureCompressor
	{
		/// @brief Check if file is a supported image format for KTX2 conversion.
		bool IsSupportedTexture(const std::filesystem::path& path);

		/// @brief Compress a single image file to KTX2.
		/// @param srcPath Input image path (.png, .jpg, .tga, .bmp).
		/// @param dstPath Target .ktx2 path.
		/// @param flipY Whether to flip texture vertically (true for Scene, false for UI).
		/// @param isNormalMap Whether texture is a normal/bump map (uses UASTC + Zstd9).
		bool CompressToKTX2(const std::filesystem::path& srcPath, const std::filesystem::path& dstPath, bool flipY,
							bool isNormalMap);

		/// @brief Convert eligible texture PackItems to KTX2 in parallel using disk cache.
		/// @return true if successful or completed; false if cancelled.
		bool ProcessTextures(const std::filesystem::path& projectDir, std::vector<PackItem>& items,
							 ExportProgressCallback onProgress, const std::atomic<bool>* cancelFlag);

		/// @brief Check if pack key corresponds to a UI or icon texture (not flipped).
		bool IsUiTexture(const std::filesystem::path& packKey);

		/// @brief Check if pack key indicates a normal or bump map.
		bool IsNormalMap(const std::filesystem::path& packKey);
	} // namespace TextureCompressor
} // namespace Chained
