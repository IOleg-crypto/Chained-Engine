#ifndef CH_ASSET_METADATA_H
#define CH_ASSET_METADATA_H

#include "engine/assets/asset.h"
#include "engine/common/uuid.h"
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace Chained
{
	struct AssetMetadata
	{
		uint32_t version = 1;
		UUID uuid;
		AssetType assetType = AssetType::None;
		uint64_t contentHash = 0;
		std::string importerSettingsYaml;
		std::vector<std::string> tags;
		bool isGenerated = false;
	};

	std::filesystem::path GetMetaPath(const std::filesystem::path& assetPath);
	bool HasMeta(const std::filesystem::path& assetPath);
	uint64_t ComputeFileHash(const std::filesystem::path& path);
	uint64_t ComputeHashFromBuffer(const std::vector<uint8_t>& data);
	AssetMetadata ReadMeta(const std::filesystem::path& metaPath);
	AssetMetadata ReadMetaFromString(std::string_view yamlContent);
	bool WriteMeta(const std::filesystem::path& metaPath, const AssetMetadata& meta);
	AssetMetadata LoadOrCreateMeta(const std::filesystem::path& assetPath, AssetType type);
} // namespace Chained

#endif
