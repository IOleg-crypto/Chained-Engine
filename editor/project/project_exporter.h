#ifndef CH_PROJECT_EXPORTER_H
#define CH_PROJECT_EXPORTER_H

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace Chained
{
	struct ExportResult
	{
		bool Success = false;
		bool Cancelled = false;
		std::string Error;
		std::filesystem::path OutDir;
		std::string File;
		uint64_t PackedFileCount = 0;
		uint64_t TotalUncompressedSize = 0;
		uint64_t PackFileSize = 0;
		bool PackSkipped = false;
	};

	using ExportProgressCallback = std::function<void(uint64_t packed, uint64_t total, const std::string& file)>;

	struct PackItem
	{
		std::filesystem::path Source;
		std::filesystem::path PackKey;
	};

	class ProjectExporter
	{
	public:
		/// @brief Export the active project to @p outputDir.
		/// @param outputDir Target export directory (created if missing).
		/// @param onProgress Callback invoked as files are packed or textures converted.
		/// @param cancelFlag Atomic flag to cancel the export operation.
		/// @param forceRepack Rebuild pack archive even if up to date.
		/// @param skipKtx2 Skip KTX2 texture conversion.
		/// @return ExportResult indicating success, cancellation, or error.
		static ExportResult ExportTo(const std::filesystem::path& outputDir,
									 ExportProgressCallback onProgress = nullptr,
									 const std::atomic<bool>* cancelFlag = nullptr, bool forceRepack = false,
									 bool skipKtx2 = false);
	};
} // namespace Chained
#endif