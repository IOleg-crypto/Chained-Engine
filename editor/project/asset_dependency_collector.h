#ifndef CH_PROJECT_ASSET_DEPENDENCY_COLLECTOR_H
#define CH_PROJECT_ASSET_DEPENDENCY_COLLECTOR_H

#include "project_exporter.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Chained
{
	namespace AssetDependencyCollector
	{
		/// @brief Collect all items to pack, including project file, referenced assets, and core resources.
		/// @param projectDir Root directory of the project containing .chproject.
		/// @param assetDir Directory containing project assets.
		/// @param resourcesDir Engine resources directory.
		/// @param outItems Output vector of PackItems (Source path and virtual PackKey).
		/// @return true if successful; false if no project file or assets found.
		bool Collect(const std::filesystem::path& projectDir, const std::filesystem::path& assetDir,
					 const std::filesystem::path& resourcesDir, std::vector<PackItem>& outItems);

		/// @brief Scan all regular files in a directory, ignoring VCS, IDE, and build artifacts.
		void ScanDirectoryFiles(const std::filesystem::path& dir, std::vector<std::filesystem::path>& outRelativeFiles);

		/// @brief Scan text file for asset path references.
		void ScanTextForReferences(const std::filesystem::path& fullPath,
								   const std::unordered_map<std::string, std::filesystem::path>& allAssetsLower,
								   std::unordered_set<std::string>& referencedLower,
								   std::vector<std::filesystem::path>& newReferences);

		/// @brief Check if file extension or name should be excluded from packing.
		bool IsIgnoredFile(const std::filesystem::path& relPath);
	} // namespace AssetDependencyCollector
} // namespace Chained

#endif // CHAINED_EDITOR_PROJECT_ASSET_DEPENDENCY_COLLECTOR_H