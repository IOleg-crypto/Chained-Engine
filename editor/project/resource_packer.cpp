#include "resource_packer.h"

#include "engine/core/log.h"

#include <pack/reader.hpp>
#include <pack/writer.hpp>
#include <pack/common.h>

// ZSTD C API — used directly to control LRM and ZSTDMT without modifying thirdparty/.
// zstd.h wraps itself in extern "C" when compiled as C++, so no wrapper needed.
#include "zstd.h"
// lz4hc.h — fallback compressor for preferSpeed chunks (already compiled into lz4_static)
#include "lz4hc.h"

// XXH64 for O(n) duplicate detection (header-only, inlined)
#define XXH_INLINE_ALL
#include "xxhash.h"

#include <algorithm>
#include <cctype>
#include <future>
#include <mutex>
#include <system_error>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace fs = std::filesystem;

namespace Chained
{
	namespace
	{
		bool IsCancelled(const std::atomic<bool>* flag)
		{
			return flag && flag->load(std::memory_order_relaxed);
		}

		struct CancelException : public std::exception
		{
			const char* what() const noexcept override
			{
				return "Packing cancelled by user.";
			}
		};

		bool CopySingleFile(const fs::path& src, const fs::path& dst, std::string& outError)
		{
			std::error_code ec;
			fs::create_directories(dst.parent_path(), ec);
			if (ec)
			{
				outError = "Failed to create directory '" + dst.parent_path().string() + "': " + ec.message();
				return false;
			}
			fs::copy_file(src, dst, fs::copy_options::update_existing, ec);
			if (ec)
			{
				outError = "Failed to copy '" + src.string() + "': " + ec.message();
				return false;
			}
			return true;
		}

		// ---------------------------------------------------------------------------
		// ParallelPackChunk — parallel-compress + sequential-write replacement for
		// pack::Writer::pack().  Only used for ZSTD (non-preferSpeed) chunks.
		// The pack binary format is identical to thirdparty/pack/source/writer.c:
		//   [PackHeader]
		//   for each item:  [PackItemHeader][path_bytes][data_bytes?]
		// ---------------------------------------------------------------------------

		/// @brief Result from a single file's compression worker.
		struct CompressedEntry
		{
			std::string itemPath;		  // pack key (used as path in header)
			std::vector<uint8_t> rawData; // original file bytes (kept for dedup compare)
			std::vector<uint8_t> zipData; // compressed bytes (empty → store raw)
			uint32_t dataSize = 0;		  // uncompressed size
			uint32_t zipSize = 0;		  // compressed size (0 → stored uncompressed)
			uint64_t xxh3 = 0;			  // XX_H3 hash of the data written to disk
			bool ok = true;
			std::string error;
		};

		/// @brief Compress a single file with ZSTD (level max, optional LRM/MT).
		static CompressedEntry CompressOneFile(const PackItem& item, float zipThreshold, const ParallelPackConfig& cfg)
		{
			CompressedEntry entry;
			entry.itemPath = item.PackKey.generic_string();

			// Read source file
			std::error_code ec;
			const uint64_t fileSize = fs::file_size(item.Source, ec);
			if (ec || fileSize == 0)
			{
				entry.dataSize = 0;
				entry.zipSize = 0;
				entry.xxh3 = XXH64(nullptr, 0, 0);
				return entry;
			}

			entry.rawData.resize(static_cast<size_t>(fileSize));
			{
				FILE* f =
#if defined(_WIN32)
					_wfopen(item.Source.wstring().c_str(), L"rb");
#else
					fopen(item.Source.string().c_str(), "rb");
#endif
				if (!f)
				{
					entry.ok = false;
					entry.error = "Cannot open: " + item.Source.string();
					return entry;
				}
				const size_t rd = fread(entry.rawData.data(), 1, entry.rawData.size(), f);
				fclose(f);
				if (rd != entry.rawData.size())
				{
					entry.ok = false;
					entry.error = "Short read: " + item.Source.string();
					return entry;
				}
			}

			entry.dataSize = static_cast<uint32_t>(fileSize);

			// Compression threshold: max compressed bytes allowed before we store raw
			const uint32_t maxZipSize =
				entry.dataSize - static_cast<uint32_t>(static_cast<double>(entry.dataSize) * zipThreshold);

			// Create per-thread ZSTD context
			ZSTD_CCtx* cctx = ZSTD_createCCtx();
			if (!cctx)
			{
				entry.ok = false;
				entry.error = "ZSTD_createCCtx failed";
				return entry;
			}

			// Configure: max level, optional LRM, optional ZSTDMT
			ZSTD_CCtx_setParameter(cctx, ZSTD_c_compressionLevel, ZSTD_maxCLevel());

			if (cfg.ZstdWindowLog > 0)
			{
				ZSTD_CCtx_setParameter(cctx, ZSTD_c_windowLog, static_cast<int>(cfg.ZstdWindowLog));
			}

			if (cfg.EnableLongRangeMatching)
			{
				ZSTD_CCtx_setParameter(cctx, ZSTD_c_enableLongDistanceMatching, 1);
			}

			if (cfg.ZstdWorkers > 0)
			{
				ZSTD_CCtx_setParameter(cctx, ZSTD_c_nbWorkers, static_cast<int>(cfg.ZstdWorkers));
			}

			// Allocate output buffer (ZSTD_compressBound guarantees it is enough)
			const size_t bound = ZSTD_compressBound(entry.dataSize);
			entry.zipData.resize(bound);

			// Execute single-shot compression.
			// ZSTD_compressCCtx automatically honors all parameters set on cctx,
			// including ZSTD_c_nbWorkers and ZSTD_c_enableLongDistanceMatching.
			size_t result = ZSTD_compressCCtx(cctx, entry.zipData.data(), bound, entry.rawData.data(), entry.dataSize,
											  ZSTD_maxCLevel());

			ZSTD_freeCCtx(cctx);

			if (!ZSTD_isError(result) && static_cast<uint32_t>(result) <= maxZipSize)
			{
				// Compression worthwhile
				entry.zipSize = static_cast<uint32_t>(result);
				entry.zipData.resize(entry.zipSize);
				entry.xxh3 = XXH64(entry.zipData.data(), entry.zipSize, 0);

				// Immediately free rawData to prevent peak RAM explosion
				entry.rawData.clear();
				entry.rawData.shrink_to_fit();
			}
			else
			{
				// Store raw: zipSize == 0 signals "uncompressed" to the writer below
				entry.zipSize = 0;
				entry.zipData.clear();
				entry.zipData.shrink_to_fit();
				entry.xxh3 = XXH64(entry.rawData.data(), entry.dataSize, 0);
			}

			return entry;
		}

		/// @brief Sort predicate matching thirdparty writer.c comparePackPathPairs:
		/// shorter path first, then lexicographic.
		static bool PackPathLess(const CompressedEntry& a, const CompressedEntry& b)
		{
			if (a.itemPath.size() != b.itemPath.size())
			{
				return a.itemPath.size() < b.itemPath.size();
			}
			return a.itemPath < b.itemPath;
		}

		/// @brief Full parallel packer: compress all files concurrently, dedup, write.
		/// @return true on success; outError set on failure.
		static bool ParallelPackChunk(const fs::path& chunkPath, const std::vector<PackItem>& chunkItems,
									  uint32_t dataVersion, float zipThreshold, const ParallelPackConfig& cfg,
									  std::atomic<uint64_t>& globalPacked, uint64_t totalCount,
									  std::mutex& progressMutex, ExportProgressCallback onProgress,
									  const std::atomic<bool>* cancelFlag, std::atomic<bool>& aborted,
									  std::string& outError)
		{
			if (chunkItems.empty())
			{
				return true;
			}

			// ---------------------------------------------------------------
			// Phase 1: Parallel compression
			// ---------------------------------------------------------------
			const unsigned int hwThreads = std::thread::hardware_concurrency();
			const unsigned int fileWorkers =
				(cfg.FileWorkers > 0) ? cfg.FileWorkers : std::min(4u, std::max(1u, hwThreads));

			std::vector<CompressedEntry> entries(chunkItems.size());
			std::atomic<size_t> nextJob{0};

			// Worker lambda: each thread picks the next unprocessed file
			auto worker = [&]() {
				while (true)
				{
					const size_t idx = nextJob.fetch_add(1, std::memory_order_relaxed);
					if (idx >= chunkItems.size())
					{
						break;
					}
					if (aborted.load(std::memory_order_relaxed) || IsCancelled(cancelFlag))
					{
						aborted.store(true);
						return;
					}

					try
					{
						entries[idx] = CompressOneFile(chunkItems[idx], zipThreshold, cfg);
					} catch (const std::bad_alloc&)
					{
						aborted.store(true);
						std::lock_guard<std::mutex> lk(progressMutex);
						outError = "Out of memory compressing: " + chunkItems[idx].PackKey.generic_string();
						return;
					} catch (const std::exception& e)
					{
						aborted.store(true);
						std::lock_guard<std::mutex> lk(progressMutex);
						outError =
							"Exception compressing " + chunkItems[idx].PackKey.generic_string() + ": " + e.what();
						return;
					}

					if (!entries[idx].ok)
					{
						aborted.store(true);
						std::lock_guard<std::mutex> lk(progressMutex);
						outError = entries[idx].error;
						return;
					}

					// Progress callback
					uint64_t done = ++globalPacked;
					if (onProgress)
					{
						std::lock_guard<std::mutex> lk(progressMutex);
						onProgress(done, totalCount, entries[idx].itemPath);
					}
				}
			};

			// Launch fileWorkers - 1 extra threads; current thread is the Nth worker
			std::vector<std::future<void>> futures;
			futures.reserve(fileWorkers > 1 ? fileWorkers - 1 : 0);
			for (unsigned int t = 1; t < fileWorkers; ++t)
			{
				futures.push_back(std::async(std::launch::async, worker));
			}
			worker(); // main thread also works
			for (auto& f : futures)
			{
				f.get();
			}

			if (aborted.load(std::memory_order_relaxed) || IsCancelled(cancelFlag))
			{
				return false;
			}

			// ---------------------------------------------------------------
			// Phase 2: Sort (path-length order, matches original writer.c qsort)
			//          then O(n) XXH3 dedup
			// ---------------------------------------------------------------
			std::sort(entries.begin(), entries.end(), PackPathLess);

			// Map: xxh3 hash → first entry index that produced this data blob
			std::unordered_map<uint64_t, size_t> dedupMap;
			dedupMap.reserve(entries.size());

			// Per-entry dedup flag and reference offset (filled in write phase)
			std::vector<uint64_t> refOffset(entries.size(), UINT64_MAX);

			// First pass: build dedup candidates (same xxh3 + same size)
			// We store the *index* of the first occurrence; the write phase
			// will fill in the actual byte offset once known.
			struct DedupKey
			{
				uint64_t xxh3;
				uint32_t zipSize;
				uint32_t dataSize;
				bool operator==(const DedupKey& o) const
				{
					return xxh3 == o.xxh3 && zipSize == o.zipSize && dataSize == o.dataSize;
				}
			};
			struct DedupKeyHash
			{
				size_t operator()(const DedupKey& k) const
				{
					// Combine the three fields into one hash
					uint64_t h = k.xxh3;
					h ^= static_cast<uint64_t>(k.zipSize) * 0x9e3779b97f4a7c15ULL;
					h ^= static_cast<uint64_t>(k.dataSize) * 0x6c62272e07bb0142ULL;
					return static_cast<size_t>(h);
				}
			};
			std::unordered_map<DedupKey, size_t, DedupKeyHash> firstOccurrence;
			firstOccurrence.reserve(entries.size());

			for (size_t i = 0; i < entries.size(); ++i)
			{
				if (entries[i].dataSize == 0)
				{
					continue;
				}
				DedupKey key{entries[i].xxh3, entries[i].zipSize, entries[i].dataSize};
				auto it = firstOccurrence.find(key);
				if (it != firstOccurrence.end())
				{
					// Candidate duplicate — will resolve actual byte offset in write phase
					refOffset[i] = it->second; // store index of first occurrence for now
				}
				else
				{
					firstOccurrence.emplace(key, i);
				}
			}

			// ---------------------------------------------------------------
			// Phase 3: Sequential write — pack binary format
			// ---------------------------------------------------------------
			// Format (identical to thirdparty/pack/source/writer.c):
			//   PackHeader (fixed, written first)
			//   for each item:
			//     PackItemHeader  (zipSize, dataSize, pathSize:8|isReference:1|dataOffset:55)
			//     path bytes      (pathSize chars, no null terminator)
			//     data bytes      (zipSize if compressed, dataSize if raw, 0 if reference)

			FILE* pf =
#if defined(_WIN32)
				_wfopen(chunkPath.wstring().c_str(), L"w+b");
#else
				fopen(chunkPath.string().c_str(), "w+b");
#endif
			if (!pf)
			{
				outError = "Cannot create: " + chunkPath.string();
				return false;
			}

			// Write placeholder header (will seek back to overwrite itemCount later if needed)
			PackHeader ph{};
			ph.magic = PACK_HEADER_MAGIC;
			ph.versionMajor = PACK_VERSION_MAJOR;
			ph.versionMinor = PACK_VERSION_MINOR;
			ph.versionPatch = PACK_VERSION_PATCH;
			ph.isBigEndian = 0; // little-endian
			ph.itemCount = static_cast<uint64_t>(entries.size());
			ph.dataVersion = dataVersion;
			ph.preferSpeed = 0; // ZSTD path
			ph._reserved = 0;

			if (fwrite(&ph, sizeof(ph), 1, pf) != 1)
			{
				fclose(pf);
				outError = "Header write failed: " + chunkPath.string();
				return false;
			}

			// Collect actual byte offsets for reference items (index → file byte offset)
			std::vector<uint64_t> dataByteOffset(entries.size(), UINT64_MAX);

			uint64_t fileOffset = sizeof(PackHeader);

			for (size_t i = 0; i < entries.size(); ++i)
			{
				const CompressedEntry& e = entries[i];

				const size_t pathLen = e.itemPath.size();
				if (pathLen > UINT8_MAX)
				{
					fclose(pf);
					outError = "Path too long (>255): " + e.itemPath;
					return false;
				}

				// Determine if this entry references a previously written blob
				bool isRef = false;
				uint64_t referenceDataOffset = UINT64_MAX;

				if (e.dataSize > 0 && refOffset[i] != UINT64_MAX)
				{
					const size_t firstIdx = refOffset[i];
					// dataByteOffset[firstIdx] must be set (firstIdx < i by construction)
					if (dataByteOffset[firstIdx] != UINT64_MAX)
					{
						isRef = true;
						referenceDataOffset = dataByteOffset[firstIdx];
					}
				}

				// Calculate where data bytes will live
				const uint64_t thisDataOffset = fileOffset + sizeof(PackItemHeader) + pathLen;

				PackItemHeader ih{};
				ih.zipSize = e.zipSize;
				ih.dataSize = e.dataSize;
				ih.pathSize = static_cast<uint8_t>(pathLen);
				ih.isReference = isRef ? 1 : 0;
				ih.dataOffset = isRef ? referenceDataOffset : thisDataOffset;

				if (fwrite(&ih, sizeof(ih), 1, pf) != 1)
				{
					fclose(pf);
					outError = "Item header write failed";
					return false;
				}
				if (fwrite(e.itemPath.data(), 1, pathLen, pf) != pathLen)
				{
					fclose(pf);
					outError = "Path write failed";
					return false;
				}

				fileOffset += sizeof(PackItemHeader) + pathLen;

				if (e.dataSize > 0 && !isRef)
				{
					dataByteOffset[i] = thisDataOffset;
					const uint8_t* writePtr = e.zipSize > 0 ? e.zipData.data() : e.rawData.data();
					const size_t writeSize = e.zipSize > 0 ? e.zipSize : e.dataSize;

					if (writeSize > 5 * 1024 * 1024)
					{
						CH_CORE_TRACE("Packed {0}: {1} MB (compressed: {2})", e.itemPath,
									  writeSize / (1024.0f * 1024.0f), e.zipSize > 0);
					}

					if (fwrite(writePtr, 1, writeSize, pf) != writeSize)
					{
						fclose(pf);
						outError = "Data write failed";
						return false;
					}
					fileOffset += writeSize;
				}
			}

			fclose(pf);
			return true;
		}

	} // namespace

	bool ResourcePacker::IsPackStale(const fs::path& packPath, const std::vector<PackItem>& items,
									 uint64_t expectedCount)
	{
		std::error_code ec;
		if (!fs::exists(packPath, ec))
		{
			return true;
		}

		const fs::path exportDir = packPath.parent_path();
		const std::string packBaseName = packPath.stem().string();
		const std::string chunkPrefix = packBaseName + "_";

		uint64_t totalPackedCount = 0;
		auto oldestPackTime = fs::last_write_time(packPath, ec);
		if (ec)
		{
			return true;
		}

		try
		{
			pack::Reader mainReader(packPath);
			totalPackedCount += mainReader.getItemCount();
		} catch (...)
		{
			return true;
		}

		for (const auto& entry : fs::directory_iterator(exportDir, ec))
		{
			if (entry.is_regular_file(ec) && entry.path().extension() == ".pack" && entry.path() != packPath)
			{
				if (entry.path().stem().string().starts_with(chunkPrefix))
				{
					try
					{
						pack::Reader chunkReader(entry.path());
						totalPackedCount += chunkReader.getItemCount();
						auto chunkTime = fs::last_write_time(entry.path(), ec);
						if (!ec && chunkTime < oldestPackTime)
						{
							oldestPackTime = chunkTime;
						}
					} catch (...)
					{
						return true;
					}
				}
			}
		}

		if (totalPackedCount != expectedCount)
		{
			return true;
		}

		for (const auto& item : items)
		{
			std::error_code srcEc;
			const auto srcTime = fs::last_write_time(item.Source, srcEc);
			if (srcEc || srcTime > oldestPackTime)
			{
				return true;
			}
		}

		return false;
	}

	bool ResourcePacker::CopyRaw(const fs::path& outputDir, const std::vector<PackItem>& items,
								 ExportProgressCallback onProgress, const std::atomic<bool>* cancelFlag)
	{
		for (size_t i = 0; i < items.size(); ++i)
		{
			if (IsCancelled(cancelFlag))
			{
				return false;
			}

			const fs::path dst = outputDir / items[i].PackKey;
			std::string err;
			if (!CopySingleFile(items[i].Source, dst, err))
			{
				CH_CORE_ERROR("ResourcePacker: Raw copy failed: {}", err);
				return false;
			}

			if (onProgress)
			{
				onProgress(i + 1, items.size(), items[i].PackKey.generic_string());
			}
		}
		return true;
	}

	void ResourcePacker::CleanupStaleChunks(const fs::path& outputDir, const std::string& packBaseName,
											size_t validChunkCount)
	{
		std::error_code ec;
		const std::string chunkPrefix = packBaseName + "_";
		for (const auto& entry : fs::directory_iterator(outputDir, ec))
		{
			if (entry.is_regular_file(ec) && entry.path().extension() == ".pack")
			{
				const std::string stem = entry.path().stem().string();
				if (stem.starts_with(chunkPrefix))
				{
					try
					{
						size_t chunkIdx = std::stoull(stem.substr(chunkPrefix.length()));
						if (chunkIdx >= validChunkCount)
						{
							fs::remove(entry.path(), ec);
						}
					} catch (...)
					{
						fs::remove(entry.path(), ec);
					}
				}
			}
		}
	}

	bool ResourcePacker::Pack(const fs::path& packPath, const std::string& packBaseName,
							  const std::vector<PackItem>& items, uint32_t dataVersion, float zipThreshold,
							  bool preferSpeed, uint32_t splitSizeMB, ExportProgressCallback onProgress,
							  const std::atomic<bool>* cancelFlag, std::string& outError, const ParallelPackConfig* cfg)
	{
		const fs::path exportDir = packPath.parent_path();

		// Build effective config (use defaults when caller passes nullptr)
		ParallelPackConfig effectiveCfg;
		if (cfg)
		{
			effectiveCfg = *cfg;
		}

		// Log what we are about to do
		{
			const unsigned int hwThreads = std::thread::hardware_concurrency();
			const unsigned int fileWorkers =
				(effectiveCfg.FileWorkers > 0) ? effectiveCfg.FileWorkers : std::min(4u, std::max(1u, hwThreads));
			CH_CORE_INFO(
				"ResourcePacker::Pack — {} items | {} file-workers | ZSTD level {} | LRM={} windowLog={} | zstdMT={}",
				items.size(), fileWorkers, ZSTD_maxCLevel(), effectiveCfg.EnableLongRangeMatching ? "on" : "off",
				effectiveCfg.ZstdWindowLog, effectiveCfg.ZstdWorkers);
		}

		struct Chunk
		{
			std::vector<PackItem> items;
			float threshold;
			bool speedFlag;
		};
		std::vector<Chunk> chunks;

		auto PartitionItems = [&](const std::vector<PackItem>& groupItems, float threshold, bool speedFlag) {
			if (groupItems.empty())
			{
				return;
			}

			if (splitSizeMB > 0)
			{
				uint64_t limitBytes = static_cast<uint64_t>(splitSizeMB) * 1024 * 1024;
				chunks.push_back({{}, threshold, speedFlag});
				uint64_t currentBytes = 0;

				for (const auto& item : groupItems)
				{
					std::error_code szEc;
					uint64_t fileBytes = static_cast<uint64_t>(fs::file_size(item.Source, szEc));
					if (szEc)
					{
						fileBytes = 0;
					}

					if (currentBytes > 0 && currentBytes + fileBytes > limitBytes)
					{
						chunks.push_back({{}, threshold, speedFlag});
						currentBytes = 0;
					}

					chunks.back().items.push_back(item);
					currentBytes += fileBytes;
				}
			}
			else
			{
				chunks.push_back({groupItems, threshold, speedFlag});
			}
		};

		// Partition all items into unified pack chunk(s) (1 single .pack if splitSizeMB == 0)
		PartitionItems(items, zipThreshold, preferSpeed);

		if (chunks.empty())
		{
			CH_CORE_WARN("ResourcePacker: No items to pack.");
			return true;
		}

		std::atomic<uint64_t> globalPacked{0};
		std::atomic<bool> aborted{false};
		std::mutex progressMutex;
		std::mutex errorMutex;

		bool allSuccess = true;

		for (size_t chunkIdx = 0; chunkIdx < chunks.size(); ++chunkIdx)
		{
			if (aborted.load(std::memory_order_relaxed) || IsCancelled(cancelFlag))
			{
				allSuccess = false;
				break;
			}

			const Chunk& chunk = chunks[chunkIdx];
			if (chunk.items.empty())
			{
				continue;
			}

			const fs::path chunkPath =
				(chunkIdx == 0) ? packPath : exportDir / (packBaseName + "_" + std::to_string(chunkIdx) + ".pack");

			CH_CORE_INFO("ResourcePacker: Packing '{}' ({} items, threshold: {:.2f}, speed: {})",
						 chunkPath.filename().string(), chunk.items.size(), chunk.threshold,
						 chunk.speedFlag ? "LZ4HC" : "ZSTD-MT");

			if (!chunk.speedFlag)
			{
				// ---------------------------------------------------------------
				// ZSTD path: fully parallel compress + write via ParallelPackChunk
				// ---------------------------------------------------------------
				std::string chunkError;
				const bool ok =
					ParallelPackChunk(chunkPath, chunk.items, dataVersion, chunk.threshold, effectiveCfg, globalPacked,
									  items.size(), progressMutex, onProgress, cancelFlag, aborted, chunkError);

				if (!ok)
				{
					allSuccess = false;
					aborted.store(true);
					if (!chunkError.empty())
					{
						std::lock_guard<std::mutex> lk(errorMutex);
						outError = "Pack error in '" + chunkPath.filename().string() + "': " + chunkError;
						CH_CORE_ERROR("ResourcePacker: {}", outError);
					}
					break;
				}
			}
			else
			{
				// ---------------------------------------------------------------
				// LZ4HC / precompressed path: delegate to original pack::Writer
				// (fast dev builds and already-compressed assets stay unchanged)
				// ---------------------------------------------------------------
				std::vector<std::string> sourceStrings, keyStrings;
				sourceStrings.reserve(chunk.items.size());
				keyStrings.reserve(chunk.items.size());
				for (const auto& item : chunk.items)
				{
					sourceStrings.push_back(item.Source.generic_string());
					keyStrings.push_back(item.PackKey.generic_string());
				}

				std::vector<const char*> rawPaths;
				rawPaths.reserve(chunk.items.size() * 2);
				for (size_t i = 0; i < chunk.items.size(); ++i)
				{
					rawPaths.push_back(sourceStrings[i].c_str());
					rawPaths.push_back(keyStrings[i].c_str());
				}

				struct LZ4Context
				{
					const std::vector<PackItem>& chunkItems;
					uint64_t totalCount;
					std::atomic<uint64_t>& packedCount;
					std::mutex& mtx;
					ExportProgressCallback cb;
					const std::atomic<bool>* cancel;
					std::atomic<bool>& abortedRef;
				};

				LZ4Context ctx{chunk.items, items.size(), globalPacked, progressMutex, onProgress, cancelFlag, aborted};

				OnPackFile callback = [](uint64_t itemIndex, void* arg) {
					auto* c = static_cast<LZ4Context*>(arg);
					if (c->abortedRef.load(std::memory_order_relaxed) || IsCancelled(c->cancel))
					{
						c->abortedRef.store(true);
						throw CancelException();
					}
					uint64_t done = ++c->packedCount;
					if (c->cb)
					{
						std::lock_guard<std::mutex> lock(c->mtx);
						c->cb(done, c->totalCount, c->chunkItems[itemIndex].PackKey.generic_string());
					}
				};

				try
				{
					pack::Writer::pack(chunkPath, chunk.items.size(), rawPaths.data(), dataVersion, chunk.threshold,
									   chunk.speedFlag, false, callback, &ctx);
				} catch (const CancelException&)
				{
					aborted.store(true);
					allSuccess = false;
					break;
				} catch (const std::exception& err)
				{
					aborted.store(true);
					std::lock_guard<std::mutex> lock(errorMutex);
					outError = "Pack error in '" + chunkPath.filename().string() + "': " + std::string(err.what());
					CH_CORE_ERROR("ResourcePacker: {}", outError);
					allSuccess = false;
					break;
				}
			}
		}

		if (!allSuccess)
		{
			return false;
		}

		CleanupStaleChunks(exportDir, packBaseName, chunks.size());
		return true;
	}
} // namespace Chained
