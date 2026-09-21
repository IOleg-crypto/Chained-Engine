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
		/// @details 0 → hw cores (one file per CPU core). Set manually to limit RAM usage on
		/// machines with many cores and large files. Each file needs ~windowSize bytes of RAM.
		unsigned int FileWorkers = 0;

		/// @brief Number of ZSTDMT worker threads per ZSTD_CCtx context.
		/// @details Keep 0 when FileWorkers already saturates all cores (Strategy A).
		/// Set to hw to compress a single large file with all threads (Strategy B).
		unsigned int ZstdWorkers = 0;

		/// @brief Enable ZSTD Long-Range Matching (LRM).
		bool EnableLongRangeMatching = true;

		/// @brief ZSTD window log (power of 2 bytes).
		/// 23 = 8MB window — excellent ratio for typical game assets (<20MB each),
		/// uses only ~16MB RAM per worker thread (vs 256MB at windowLog=27).
		/// Raise to 25–26 only if you have very large homogeneous assets AND excess RAM.
		unsigned int ZstdWindowLog = 23;

		/// @brief Size of the ZSTD dictionary trained from asset samples before parallel compression.
		/// @details The dictionary substitutes for the cross-file history that single-threaded streaming
		/// provided, recovering the compression ratio lost when each file gets its own isolated CCtx.
		/// 0 = disable dictionary training (fallback to per-file ZSTD without shared context).
		size_t DictionarySize = 0; // Disabled because thirdparty pack reader lacks dict support

		/// @brief Maximum bytes sampled from each file to build the training corpus.
		/// @details Capped to avoid spending too much RAM / time on huge files.
		size_t DictSampleSizePerFile = 64 * 1024; // 64 KB per file
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
