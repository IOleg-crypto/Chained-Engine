#ifndef CH_PROJECT_BINARY_DEPLOYER_H
#define CH_PROJECT_BINARY_DEPLOYER_H

#include <atomic>
#include <filesystem>
#include <string>

namespace Chained
{
	namespace BinaryDeployer
	{
		/// @brief Deploy game executable, runtime DLLs, and required support directories.
		/// @param exeDir Directory containing engine binaries and build outputs.
		/// @param outputDir Target export folder.
		/// @param appName Application name used for the renamed executable.
		/// @param isDebugExport True if exporting a Debug build, false for Release.
		/// @param cancelFlag Optional atomic cancellation flag.
		/// @return true if all deployment steps succeeded.
		bool Deploy(const std::filesystem::path& exeDir, const std::filesystem::path& outputDir,
					const std::string& appName, bool isDebugExport, const std::atomic<bool>* cancelFlag);

		bool CopyFile(const std::filesystem::path& src, const std::filesystem::path& dst, std::string& outError);
	} // namespace BinaryDeployer
} // namespace Chained
#endif