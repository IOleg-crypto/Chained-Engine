#pragma once

#include "project_exporter.h"

#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

namespace Chained
{
	namespace ResourcePacker
	{
		/// @brief Pack all items into one or more .pack files using maximum ZSTD compression.
		bool Pack(const std::filesystem::path& packPath, const std::string& packBaseName,
				  const std::vector<PackItem>& items, uint32_t dataVersion, float zipThreshold, bool preferSpeed,
				  uint32_t splitSizeMB, ExportProgressCallback onProgress, const std::atomic<bool>* cancelFlag,
				  std::string& outError);

		/// @brief Check if existing pack file is still fresh compared to source item files.
		bool IsPackStale(const std::filesystem::path& packPath, const std::vector<PackItem>& items,
						 uint64_t expectedCount);

		/// @brief Copy files uncompressed for Raw export mode.
		bool CopyRaw(const std::filesystem::path& outputDir, const std::vector<PackItem>& items,
					 ExportProgressCallback onProgress, const std::atomic<bool>* cancelFlag);

		/// @brief Clean up old chunks from previous split exports.
		void CleanupStaleChunks(const std::filesystem::path& outputDir, const std::string& packBaseName);
	} // namespace ResourcePacker
} // namespace Chained
