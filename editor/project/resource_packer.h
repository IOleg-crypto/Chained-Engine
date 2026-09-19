#pragma once

#include "project_exporter.h"

#include <atomic>
#include <filesystem>
#include <string>
#include <vector>

namespace Chained
{
	/// @brief Tuning parameters for the parallel packer.
	/// @details Passed to ResourcePacker::Pack to control the thread pool
	/// and ZSTD compression parameters used in non-precompressed chunks.
	struct ParallelPackConfig
	{
		/// @brief Number of files compressed concurrently.
		/// @details Capped to 4 by default to avoid peak RAM exhaustion when compressing
		/// large 200MB+ models with Long-Range Matching. 0 → automatic safe worker count (min(4, CPU cores)).
		unsigned int FileWorkers = 4;

		/// @brief Number of ZSTDMT worker threads per ZSTD_CCtx context.
		/// @details 0 disabled since we parallelize across files.
		unsigned int ZstdWorkers = 0;

		/// @brief Enable ZSTD Long-Range Matching (LRM).
		bool EnableLongRangeMatching = true;

		/// @brief ZSTD window log (power of 2 bytes). 26 = 64MB window.
		unsigned int ZstdWindowLog = 26;
	};

	namespace ResourcePacker
	{
		/// @brief Pack all items into one or more .pack files using maximum ZSTD compression.
		/// @param cfg Optional parallel compression tuning. Pass nullptr for defaults.
		bool Pack(const std::filesystem::path& packPath, const std::string& packBaseName,
				  const std::vector<PackItem>& items, uint32_t dataVersion, float zipThreshold, bool preferSpeed,
				  uint32_t splitSizeMB, ExportProgressCallback onProgress, const std::atomic<bool>* cancelFlag,
				  std::string& outError, const ParallelPackConfig* cfg = nullptr);

		/// @brief Check if existing pack file is still fresh compared to source item files.
		bool IsPackStale(const std::filesystem::path& packPath, const std::vector<PackItem>& items,
						 uint64_t expectedCount);

		/// @brief Copy files uncompressed for Raw export mode.
		bool CopyRaw(const std::filesystem::path& outputDir, const std::vector<PackItem>& items,
					 ExportProgressCallback onProgress, const std::atomic<bool>* cancelFlag);

		/// @brief Clean up old chunks from previous split exports.
		void CleanupStaleChunks(const std::filesystem::path& outputDir, const std::string& packBaseName,
								size_t validChunkCount = 1);
	} // namespace ResourcePacker
} // namespace Chained
