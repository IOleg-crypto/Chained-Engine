#include "resource_packer.h"

#include "engine/core/log.h"

#include <pack/reader.hpp>
#include <pack/writer.hpp>

#include <algorithm>
#include <mutex>
#include <system_error>

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
	} // namespace

	bool ResourcePacker::IsPackStale(const fs::path& packPath, const std::vector<PackItem>& items,
									 uint64_t expectedCount)
	{
		std::error_code ec;
		if (!fs::exists(packPath, ec))
		{
			return true;
		}

		try
		{
			pack::Reader reader(packPath);
			if (reader.getItemCount() != expectedCount)
			{
				return true;
			}
		} catch (...)
		{
			return true;
		}

		const auto packTime = fs::last_write_time(packPath, ec);
		if (ec)
		{
			return true;
		}

		for (const auto& item : items)
		{
			std::error_code srcEc;
			const auto srcTime = fs::last_write_time(item.Source, srcEc);
			if (srcEc || srcTime > packTime)
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

	void ResourcePacker::CleanupStaleChunks(const fs::path& outputDir, const std::string& packBaseName)
	{
		std::error_code ec;
		const std::string chunkPrefix = packBaseName + "_";
		for (const auto& entry : fs::directory_iterator(outputDir, ec))
		{
			if (entry.is_regular_file(ec) && entry.path().extension() == ".pack")
			{
				if (entry.path().stem().string().starts_with(chunkPrefix))
				{
					fs::remove(entry.path(), ec);
				}
			}
		}
	}

	bool ResourcePacker::Pack(const fs::path& packPath, const std::string& packBaseName,
							  const std::vector<PackItem>& items, uint32_t dataVersion, float zipThreshold,
							  bool preferSpeed, uint32_t splitSizeMB, ExportProgressCallback onProgress,
							  const std::atomic<bool>* cancelFlag, std::string& outError)
	{
		const fs::path exportDir = packPath.parent_path();

		// Partition into chunks if splitSizeMB > 0
		struct Chunk
		{
			std::vector<PackItem> items;
		};
		std::vector<Chunk> chunks;

		if (splitSizeMB > 0)
		{
			uint64_t limitBytes = static_cast<uint64_t>(splitSizeMB) * 1024 * 1024;
			chunks.emplace_back();
			uint64_t currentBytes = 0;

			for (const auto& item : items)
			{
				std::error_code szEc;
				uint64_t fileBytes = static_cast<uint64_t>(fs::file_size(item.Source, szEc));
				if (szEc)
				{
					fileBytes = 0;
				}

				if (currentBytes > 0 && currentBytes + fileBytes > limitBytes)
				{
					chunks.emplace_back();
					currentBytes = 0;
				}

				chunks.back().items.push_back(item);
				currentBytes += fileBytes;
			}
		}
		else
		{
			chunks.emplace_back();
			chunks.back().items = items;
		}

		std::atomic<uint64_t> globalPacked{0};
		std::atomic<bool> aborted{false};
		std::mutex progressMutex;

		for (size_t chunkIdx = 0; chunkIdx < chunks.size(); ++chunkIdx)
		{
			if (aborted.load(std::memory_order_relaxed) || IsCancelled(cancelFlag))
			{
				return false;
			}

			const auto& chunk = chunks[chunkIdx];
			if (chunk.items.empty())
			{
				continue;
			}

			fs::path chunkPath =
				(chunkIdx == 0) ? packPath : exportDir / (packBaseName + "_" + std::to_string(chunkIdx) + ".pack");

			std::vector<std::string> sourceStrings;
			std::vector<std::string> keyStrings;
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

			struct Context
			{
				const std::vector<PackItem>& chunkItems;
				uint64_t totalCount;
				std::atomic<uint64_t>& packedCount;
				std::mutex& mtx;
				ExportProgressCallback cb;
				const std::atomic<bool>* cancel;
				std::atomic<bool>& abortedRef;
			};

			Context ctx{chunk.items, items.size(), globalPacked, progressMutex, onProgress, cancelFlag, aborted};

			OnPackFile callback = [](uint64_t itemIndex, void* arg) {
				auto* c = static_cast<Context*>(arg);
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
				CH_CORE_INFO("ResourcePacker: Packing '{}' ({} items)", chunkPath.filename().string(),
							 chunk.items.size());

				pack::Writer::pack(chunkPath, chunk.items.size(), rawPaths.data(), dataVersion, zipThreshold,
								   preferSpeed, false, callback, &ctx);
			} catch (const CancelException&)
			{
				aborted.store(true);
				return false;
			} catch (const std::exception& err)
			{
				aborted.store(true);
				outError = "Pack error: " + std::string(err.what());
				CH_CORE_ERROR("ResourcePacker: {}", outError);
				return false;
			}
		}

		if (splitSizeMB == 0)
		{
			CleanupStaleChunks(exportDir, packBaseName);
		}

		return true;
	}
} // namespace Chained
