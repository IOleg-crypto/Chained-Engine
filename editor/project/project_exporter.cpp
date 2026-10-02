#include "project_exporter.h"

#include "asset_dependency_collector.h"
#include "binary_deployer.h"
#include "resource_packer.h"
#include "texture_compressor.h"

#include "engine/core/log.h"
#include "engine/core/platform.h"
#include "engine/project/project.h"

#include <future>
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

		fs::path ResolveResourcesDirectory(const fs::path& exeDir)
		{
			fs::path resDir = exeDir / "resources";
#ifdef CH_SOURCE_RESOURCES_DIR
			if (!fs::exists(resDir))
			{
				fs::path srcRes(CH_SOURCE_RESOURCES_DIR);
				if (fs::exists(srcRes / "resources"))
				{
					resDir = srcRes / "resources";
				}
				else if (fs::exists(srcRes))
				{
					resDir = srcRes;
				}
			}
#elif defined(PROJECT_ROOT_DIR)
			if (!fs::exists(resDir))
			{
				fs::path rootRes = fs::path(PROJECT_ROOT_DIR) / "resources";
				if (fs::exists(rootRes))
				{
					resDir = rootRes;
				}
			}
#endif
			return resDir;
		}
	} // namespace

	ExportResult ProjectExporter::ExportTo(const fs::path& outputDir, ExportProgressCallback onProgress,
										   const std::atomic<bool>* cancelFlag, bool forceRepack, bool skipKtx2)
	{
		ExportResult result;
		result.OutDir = outputDir;

		const bool outputExisted = fs::exists(outputDir);

		auto CleanupAndCancel = [&](const std::string& phase) -> ExportResult {
			if (!outputExisted)
			{
				std::error_code ec;
				fs::remove_all(outputDir, ec);
			}
			else
			{
				// Remove potentially incomplete .pack files from aborted run
				std::error_code ec;
				for (const auto& entry : fs::directory_iterator(outputDir, ec))
				{
					if (entry.is_regular_file(ec) && entry.path().extension() == ".pack")
					{
						fs::remove(entry.path(), ec);
					}
				}
			}
			result.Cancelled = true;
			CH_CORE_INFO("ProjectExporter: Cancelled during {}.", phase);
			return result;
		};

		// 1. Verify active project
		auto project = Project::GetActive();
		if (!project)
		{
			result.Error = "No active project. Please open a project before exporting.";
			CH_CORE_ERROR("ProjectExporter: {}", result.Error);
			return result;
		}

		const auto& cfg = project->GetConfig();
		const fs::path projectDir = cfg.ProjectDirectory;
		const fs::path assetDir = projectDir / cfg.AssetDirectory;
		const fs::path exeDir = Platform::GetExecutableDirectory();
		const fs::path resourcesDir = ResolveResourcesDirectory(exeDir);

#ifdef CH_BUILD_CONFIG
		const bool isDebugExport = (std::string(CH_BUILD_CONFIG) == "Debug");
#else
		const bool isDebugExport = (exeDir.string().find("Debug") != std::string::npos);
#endif

		CH_CORE_INFO("ProjectExporter: Starting export for '{}' [Config: {}]", cfg.Name,
					 isDebugExport ? "Debug" : "Release");

		// 2. Prepare output directory
		std::error_code ec;
		fs::create_directories(outputDir, ec);
		if (ec)
		{
			result.Error = "Failed to create output directory: " + ec.message();
			CH_CORE_ERROR("ProjectExporter: {}", result.Error);
			return result;
		}

		if (IsCancelled(cancelFlag))
		{
			return CleanupAndCancel("initialization");
		}

		// 3. Collect only referenced game assets and required resources
		std::vector<PackItem> items;
		if (!AssetDependencyCollector::Collect(projectDir, assetDir, resourcesDir, items))
		{
			result.Error = "Failed to collect required project assets.";
			CH_CORE_ERROR("ProjectExporter: {}", result.Error);
			return result;
		}

		result.PackedFileCount = items.size();
		result.TotalUncompressedSize = 0;
		for (const auto& item : items)
		{
			std::error_code szEc;
			result.TotalUncompressedSize += fs::file_size(item.Source, szEc);
		}

		const auto& exp = cfg.Export;
		const bool isRawMode = (exp.Mode == PackMode::Raw);
		const std::string packBaseName = exp.PackName.empty() ? "resources" : exp.PackName;
		const fs::path packPath = outputDir / (packBaseName + ".pack");

		// 4. Texture optimization (KTX2 conversion with smart caching)
		if (!isRawMode && !skipKtx2)
		{
			if (!TextureCompressor::ProcessTextures(projectDir, items, onProgress, cancelFlag))
			{
				return CleanupAndCancel("texture compression");
			}
		}

		if (IsCancelled(cancelFlag))
		{
			return CleanupAndCancel("pre-packing");
		}

		// 5. Parallel Execution: Deploy Binaries concurrently with Packing
		auto binaryDeployTask = std::async(std::launch::async, [&]() -> bool {
			return BinaryDeployer::Deploy(exeDir, outputDir, cfg.Name, isDebugExport, cancelFlag);
		});

		bool packSuccess = false;
		if (isRawMode)
		{
			CH_CORE_INFO("ProjectExporter: Raw mode -- copying {} files directly.", items.size());
			packSuccess = ResourcePacker::CopyRaw(outputDir, items, onProgress, cancelFlag);
			result.PackSkipped = true;
		}
		else if (!forceRepack && !ResourcePacker::IsPackStale(packPath, items, items.size()))
		{
			CH_CORE_INFO("ProjectExporter: {}.pack is up to date ({} items) -- skipping repack.", packBaseName,
						 items.size());
			result.PackSkipped = true;
			packSuccess = true;
		}
		else
		{
			const float threshold = (exp.Mode == PackMode::Max) ? 0.0f : exp.ZipThreshold;
			const bool preferSpeed = (exp.Mode == PackMode::Fast);
			std::string packError;

			packSuccess = ResourcePacker::Pack(packPath, packBaseName, items, exp.DataVersion, threshold, preferSpeed,
											   exp.SplitSizeMB, onProgress, cancelFlag, packError);
			if (!packSuccess && !packError.empty())
			{
				result.Error = packError;
			}
		}

		const bool deploySuccess = binaryDeployTask.get();

		if (IsCancelled(cancelFlag))
		{
			return CleanupAndCancel("deployment or packing");
		}

		if (!deploySuccess || !packSuccess)
		{
			if (result.Error.empty())
			{
				result.Error = !deploySuccess ? "Binary deployment failed." : "Resource packing failed.";
			}
			return CleanupAndCancel("task execution");
		}

		// 6. Calculate total pack archive size
		if (!isRawMode)
		{
			result.PackFileSize = 0;
			for (const auto& entry : fs::directory_iterator(outputDir, ec))
			{
				if (entry.is_regular_file(ec) && entry.path().extension() == ".pack")
				{
					result.PackFileSize += static_cast<uint64_t>(fs::file_size(entry.path(), ec));
				}
			}
		}

		result.Success = true;
		CH_CORE_INFO("ProjectExporter: Export finished successfully -> '{}' ({} items, pack size: {:.2f} MB)",
					 outputDir.string(), result.PackedFileCount,
					 static_cast<double>(result.PackFileSize) / (1024.0 * 1024.0));

		return result;
	}
} // namespace Chained
