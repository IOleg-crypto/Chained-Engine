#include "texture_compressor.h"

#include "engine/core/log.h"

#include <basisu_comp.h>
#include <basisu_enc.h>
#include <basisu_frontend.h>
#include <stb_image.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#include <unordered_set>

namespace fs = std::filesystem;

namespace Chained
{
	namespace
	{
		void EnsureBasisuEncoderInit()
		{
			static std::once_flag s_EncInitOnce;
			std::call_once(s_EncInitOnce, []() { basisu::basisu_encoder_init(); });
		}

		std::string StringToLower(std::string str)
		{
			std::transform(str.begin(), str.end(), str.begin(),
						   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return str;
		}

		bool IsCancelled(const std::atomic<bool>* flag)
		{
			return flag && flag->load(std::memory_order_relaxed);
		}
	} // namespace

	bool TextureCompressor::IsSupportedTexture(const fs::path& path)
	{
		static const std::unordered_set<std::string> kExtensions = {".png", ".jpg", ".jpeg", ".tga", ".bmp"};
		const std::string ext = StringToLower(path.extension().string());
		return kExtensions.count(ext) > 0;
	}

	bool TextureCompressor::IsUiTexture(const fs::path& packKey)
	{
		const std::string lower = StringToLower(packKey.generic_string());
		return lower.find("/ui/") != std::string::npos || lower.find("/icons/") != std::string::npos ||
			   lower.find("/font/") != std::string::npos || lower.find("/fonts/") != std::string::npos ||
			   lower.find("logo") != std::string::npos || lower.find("menu") != std::string::npos;
	}

	bool TextureCompressor::IsNormalMap(const fs::path& packKey)
	{
		const std::string lower = StringToLower(packKey.filename().string());
		return lower.find("normal") != std::string::npos || lower.find("_norm") != std::string::npos ||
			   lower.find("_n.") != std::string::npos || lower.find("bump") != std::string::npos;
	}

	bool TextureCompressor::CompressToKTX2(const fs::path& srcPath, const fs::path& dstPath, bool flipY,
										   bool isNormalMap, PackMode mode, unsigned int basisThreads)
	{
		try
		{
			EnsureBasisuEncoderInit();

			// 1. Load pixels directly via stb_image for 100% format support and crash immunity
			int width = 0, height = 0, channels = 0;
			stbi_uc* pixels = stbi_load(srcPath.string().c_str(), &width, &height, &channels, 4);
			if (!pixels || width <= 0 || height <= 0)
			{
				CH_CORE_WARN("TextureCompressor: stb_image failed to load '{}'", srcPath.string());
				return false;
			}

			// Use specified threads for BasisU internal encoding (defaults to 2)
			unsigned int numThreads = std::max(1u, basisThreads);
			basisu::job_pool jpool(numThreads);

			basisu::basis_compressor_params params;
			params.m_create_ktx2_file = true;
			params.m_mip_gen = false;
			params.m_y_flip = flipY;
			params.m_multithreading = true; // Use jpool's threads internally
			params.m_pJob_pool = &jpool;

			if (isNormalMap)
			{
				params.m_uastc = true;
				if (mode == PackMode::Max)
				{
					// High-precision UASTC + Zstd lvl22 for normal/bump maps with RDO
					params.m_rdo_uastc_ldr_4x4 = true;
					params.m_rdo_uastc_ldr_4x4_dict_size = 4096;					   // Good balance of size/speed
					params.m_pack_uastc_ldr_4x4_flags = basisu::cPackUASTCLevelSlower; // Not VerySlow to avoid hanging
					params.m_ktx2_uastc_supercompression = basist::KTX2_SS_ZSTANDARD;
					params.m_ktx2_zstd_supercompression_level = 22;
				}
				else
				{
					// Fast high-precision UASTC + Zstd level 9
					params.m_rdo_uastc_ldr_4x4 = false;
					params.m_pack_uastc_ldr_4x4_flags = basisu::cPackUASTCLevelFastest;
					params.m_ktx2_uastc_supercompression = basist::KTX2_SS_ZSTANDARD;
					params.m_ktx2_zstd_supercompression_level = 9;
				}
				params.m_perceptual = false;
			}
			else
			{
				params.m_uastc = false;
				if (mode == PackMode::Max)
				{
					// Max-compressed ETC1S: level 4 gives 98% of the compression of 6, but is 10x faster
					params.m_etc1s_compression_level = 4;
					params.m_quality_level = 192; // 192 is visually lossless, 255 is overkill and slow

					// OOM Prevention: 4K+ textures with Alpha eat massive RAM during RDO at level 4.
					// Cap to level 2 (which is still very good) to prevent BasisU memory allocation failures.
					if (width >= 4096 || height >= 4096)
					{
						params.m_etc1s_compression_level = 2;
						params.m_quality_level = 160;
					}

					params.m_no_endpoint_rdo = false; // Enable endpoint RDO
					params.m_no_selector_rdo = false; // Enable selector RDO
				}
				else
				{
					// Ultra-compact ETC1S / BasisLZ for Albedo, Diffuse, UI, and color textures
					params.m_etc1s_compression_level = 2; // Fast encoding
					params.m_quality_level = 128;
				}
				params.m_perceptual = true;
			}

			params.m_read_source_images = false;
			basisu::image img;
			img.init(pixels, width, height, 4);
			stbi_image_free(pixels);
			params.m_source_images.push_back(img);

			basisu::basis_compressor comp;
			if (!comp.init(params) || comp.process() != basisu::basis_compressor::cECSuccess)
			{
				CH_CORE_WARN("TextureCompressor: BasisU compression failed for '{}'", srcPath.string());
				return false;
			}

			const auto& ktx2Data = comp.get_output_ktx2_file();
			if (ktx2Data.empty())
			{
				CH_CORE_WARN("TextureCompressor: empty KTX2 output for '{}'", srcPath.string());
				return false;
			}

			std::error_code ec;
			fs::create_directories(dstPath.parent_path(), ec);

			std::ofstream out(dstPath, std::ios::binary | std::ios::trunc);
			if (!out.is_open())
			{
				CH_CORE_WARN("TextureCompressor: cannot write to '{}'", dstPath.string());
				return false;
			}

			out.write(reinterpret_cast<const char*>(ktx2Data.data()), ktx2Data.size());
			return out.good();
		} catch (const std::exception& e)
		{
			CH_CORE_WARN("TextureCompressor: exception for '{}': {}", srcPath.string(), e.what());
			return false;
		} catch (...)
		{
			CH_CORE_WARN("TextureCompressor: unknown exception for '{}'", srcPath.string());
			return false;
		}
	}

	bool TextureCompressor::ProcessTextures(const fs::path& projectDir, std::vector<PackItem>& items, PackMode mode,
											ExportProgressCallback onProgress, const std::atomic<bool>* cancelFlag)
	{
		fs::path cacheDir = projectDir / ".texture_cache";
		std::error_code ec;
		fs::create_directories(cacheDir, ec);

		struct Job
		{
			size_t itemIndex;
			fs::path src;
			fs::path dst;
			std::string name;
			bool flipY;
			bool isNormal;
		};

		std::vector<Job> jobs;
		jobs.reserve(items.size());

		for (size_t i = 0; i < items.size(); ++i)
		{
			if (IsCancelled(cancelFlag))
			{
				return false;
			}

			const fs::path& src = items[i].Source;
			if (!IsSupportedTexture(src))
			{
				continue;
			}

			const bool isUi = IsUiTexture(items[i].PackKey);
			const bool isNormal = IsNormalMap(items[i].PackKey);
			const bool flipY = !isUi; // Scene textures flip Y for OpenGL; UI textures remain unflipped

			// Cache key includes: pack key, flipY, normal map flag, version tag, and pack mode
			std::string modeTag = (mode == PackMode::Max) ? "|max" : "|fast";
			std::string cacheKey = items[i].PackKey.generic_string() + (flipY ? "|flip" : "|noflip") +
								   (isNormal ? "|norm" : "|color") + "|v4_nomip" + modeTag;
			std::string ktxName = std::to_string(std::hash<std::string>{}(cacheKey)) + ".ktx2";
			fs::path ktxPath = cacheDir / ktxName;

			// Check disk cache validity against source modification time and size
			std::error_code timeEc;
			if (fs::exists(ktxPath, timeEc))
			{
				auto srcTime = fs::last_write_time(src, timeEc);
				auto dstTime = fs::last_write_time(ktxPath, timeEc);
				if (!timeEc && dstTime >= srcTime && fs::file_size(ktxPath, timeEc) > 0)
				{
					items[i].Source = ktxPath;
					continue; // Cache hit: instant reuse
				}
			}

			jobs.push_back({i, src, ktxPath, items[i].PackKey.filename().string(), flipY, isNormal});
		}

		if (jobs.empty())
		{
			return true;
		}

		const unsigned int hw = std::max(1u, std::thread::hardware_concurrency());

		// OOM Prevention: BasisU UASTC encoding uses ~1GB+ RAM per 4K texture.
		// Uncapped parallel workers (hw/2) on a 32-thread CPU will crash on 16GB-32GB systems.
		// Solution: Cap parallel files strictly to 2 to bound RAM usage, and scale BasisU internal threads to fill CPU
		// cores.
		unsigned int fileWorkers = std::min(2u, std::max(1u, hw / 4));
		unsigned int basisThreads = std::max(2u, hw / std::max(1u, fileWorkers));

		CH_CORE_INFO("TextureCompressor: Compressing {} textures to KTX2 ({} files parallel, {} Basis threads/file)...",
					 jobs.size(), fileWorkers, basisThreads);

		std::atomic<size_t> completed{0};
		std::atomic<bool> abortJobs{false};
		std::mutex resultMutex;

		auto worker = [&](size_t start, size_t end) {
			for (size_t j = start; j < end; ++j)
			{
				if (abortJobs.load(std::memory_order_relaxed) || IsCancelled(cancelFlag))
				{
					abortJobs.store(true);
					return;
				}

				const auto& job = jobs[j];
				if (CompressToKTX2(job.src, job.dst, job.flipY, job.isNormal, mode, basisThreads))
				{
					std::lock_guard<std::mutex> lock(resultMutex);
					items[job.itemIndex].Source = job.dst;
				}
				else
				{
					CH_CORE_WARN("TextureCompressor: Conversion failed for '{}' -- packing original", job.name);
				}

				size_t current = ++completed;
				if (onProgress)
				{
					onProgress(current, jobs.size(), "Compressing texture: " + job.name);
				}
			}
		};

		std::vector<std::thread> workers;
		const size_t totalJobs = jobs.size();
		const size_t chunkSize = (totalJobs + fileWorkers - 1) / fileWorkers;

		for (unsigned int t = 0; t < fileWorkers; ++t)
		{
			size_t start = t * chunkSize;
			size_t end = std::min(start + chunkSize, totalJobs);
			if (start < end)
			{
				workers.emplace_back(worker, start, end);
			}
		}

		for (auto& w : workers)
		{
			if (w.joinable())
			{
				w.join();
			}
		}

		return !IsCancelled(cancelFlag);
	}
} // namespace Chained
