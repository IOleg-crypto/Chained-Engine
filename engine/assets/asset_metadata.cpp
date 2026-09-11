#include "engine/assets/asset_metadata.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/service_locator.h"

namespace Chained
{

	std::filesystem::path GetMetaPath(const std::filesystem::path& assetPath)
	{
		return assetPath.string() + ".meta";
	}

	bool HasMeta(const std::filesystem::path& assetPath)
	{
		auto* am = ServiceLocator::TryGet<AssetManager>();
		if (am && am->IsPacked())
		{
			std::filesystem::path metaPath = GetMetaPath(assetPath);
			if (am->FileExists(metaPath.string()))
			{
				return true;
			}
		}
		return std::filesystem::exists(GetMetaPath(assetPath));
	}

	uint64_t ComputeFileHash(const std::filesystem::path& path)
	{
		constexpr size_t kChunkSize = 65536;
		std::ifstream file(path, std::ios::binary);
		if (!file.is_open())
		{
			return 0;
		}
		uint64_t hash = UINT64_C(14695981039346656037);
		char buf[kChunkSize];
		while (file.read(buf, kChunkSize) || file.gcount() > 0)
		{
			auto count = static_cast<size_t>(file.gcount());
			for (size_t i = 0; i < count; ++i)
			{
				hash = (hash ^ static_cast<uint64_t>(static_cast<unsigned char>(buf[i]))) * 1099511628211ULL;
			}
			if (count < kChunkSize)
			{
				break;
			}
		}
		return hash;
	}

	uint64_t ComputeHashFromBuffer(const std::vector<uint8_t>& data)
	{
		uint64_t hash = UINT64_C(14695981039346656037);
		for (size_t i = 0; i < data.size(); ++i)
		{
			hash = (hash ^ static_cast<uint64_t>(data[i])) * 1099511628211ULL;
		}
		return hash;
	}

	namespace
	{
		constexpr const char* kAssetTypeNames[] = {
			"None",		"Model", "Texture", "Audio",  "Shader",			"Environment",
			"Material", "Font",	 "Scene",	"Script", "AnimationGraph",
		};

		static_assert(sizeof(kAssetTypeNames) / sizeof(kAssetTypeNames[0]) ==
						  static_cast<size_t>(AssetType::AnimationGraph) + 1,
					  "kAssetTypeNames size does not match AssetType enum");

		const char* AssetTypeToString(AssetType type)
		{
			auto idx = static_cast<size_t>(type);
			return idx < sizeof(kAssetTypeNames) / sizeof(kAssetTypeNames[0]) ? kAssetTypeNames[idx] : "Unknown";
		}

		AssetType StringToAssetType(const std::string& str)
		{
			for (size_t i = 0; i < sizeof(kAssetTypeNames) / sizeof(kAssetTypeNames[0]); ++i)
			{
				if (str == kAssetTypeNames[i])
				{
					return static_cast<AssetType>(i);
				}
			}
			return AssetType::None;
		}
	} // namespace

	AssetMetadata ReadMetaFromString(std::string_view yamlContent)
	{
		AssetMetadata meta;
		try
		{
			YAML::Node root = YAML::Load(std::string(yamlContent));

			meta.version = root["Version"] ? root["Version"].as<uint32_t>() : 1;
			meta.contentHash = root["ContentHash"] ? root["ContentHash"].as<uint64_t>() : 0;
			meta.isGenerated = root["IsGenerated"] ? root["IsGenerated"].as<bool>() : false;

			if (root["UUID"])
			{
				meta.uuid = UUID(root["UUID"].as<uint64_t>());
			}

			if (root["Type"])
			{
				meta.assetType = StringToAssetType(root["Type"].as<std::string>());
			}

			if (root["ImporterSettings"])
			{
				meta.importerSettingsYaml = YAML::Dump(root["ImporterSettings"]);
			}

			if (root["Tags"] && root["Tags"].IsSequence())
			{
				for (auto tag : root["Tags"])
				{
					meta.tags.push_back(tag.as<std::string>());
				}
			}
		} catch (const YAML::Exception& e)
		{
			CH_CORE_ERROR("Failed to parse meta YAML: {}", e.what());
			return {};
		}
		return meta;
	}

	AssetMetadata ReadMeta(const std::filesystem::path& metaPath)
	{
		auto* am = ServiceLocator::TryGet<AssetManager>();
		if (am && am->IsPacked())
		{
			auto metaData = am->ReadAssetData(metaPath.string());
			if (!metaData.empty())
			{
				std::string content(metaData.begin(), metaData.end());
				return ReadMetaFromString(content);
			}
		}

		AssetMetadata meta;
		try
		{
			YAML::Node root = YAML::LoadFile(metaPath.string());

			meta.version = root["Version"] ? root["Version"].as<uint32_t>() : 1;
			meta.contentHash = root["ContentHash"] ? root["ContentHash"].as<uint64_t>() : 0;
			meta.isGenerated = root["IsGenerated"] ? root["IsGenerated"].as<bool>() : false;

			if (root["UUID"])
			{
				meta.uuid = UUID(root["UUID"].as<uint64_t>());
			}

			if (root["Type"])
			{
				meta.assetType = StringToAssetType(root["Type"].as<std::string>());
			}

			if (root["ImporterSettings"])
			{
				meta.importerSettingsYaml = YAML::Dump(root["ImporterSettings"]);
			}

			if (root["Tags"] && root["Tags"].IsSequence())
			{
				for (auto tag : root["Tags"])
				{
					meta.tags.push_back(tag.as<std::string>());
				}
			}
		} catch (const YAML::Exception& e)
		{
			CH_CORE_ERROR("Failed to parse meta file {}: {}", metaPath.string(), e.what());
			return {};
		}
		return meta;
	}

	bool WriteMeta(const std::filesystem::path& metaPath, const AssetMetadata& meta)
	{
		try
		{
			YAML::Emitter out;
			out << YAML::BeginMap;
			out << YAML::Key << "Version" << YAML::Value << meta.version;
			out << YAML::Key << "UUID" << YAML::Value << static_cast<uint64_t>(meta.uuid);
			out << YAML::Key << "Type" << YAML::Value << AssetTypeToString(meta.assetType);
			out << YAML::Key << "ContentHash" << YAML::Value << meta.contentHash;
			out << YAML::Key << "IsGenerated" << YAML::Value << meta.isGenerated;

			if (!meta.tags.empty())
			{
				out << YAML::Key << "Tags" << YAML::Value << YAML::BeginSeq;
				for (const auto& tag : meta.tags)
				{
					out << tag;
				}
				out << YAML::EndSeq;
			}

			if (!meta.importerSettingsYaml.empty())
			{
				out << YAML::Key << "ImporterSettings" << YAML::Value << YAML::Load(meta.importerSettingsYaml);
			}

			out << YAML::EndMap;

			std::ofstream file(metaPath);
			if (!file.is_open())
			{
				CH_CORE_ERROR("Failed to open meta file for writing: {}", metaPath.string());
				return false;
			}
			file << out.c_str();
			return true;
		} catch (const YAML::Exception& e)
		{
			CH_CORE_ERROR("Failed to write meta file {}: {}", metaPath.string(), e.what());
			return false;
		}
	}

	AssetMetadata LoadOrCreateMeta(const std::filesystem::path& assetPath, AssetType type)
	{
		auto* am = ServiceLocator::TryGet<AssetManager>();
		if (am && am->IsPacked())
		{
			std::filesystem::path metaPath = GetMetaPath(assetPath);
			if (am->FileExists(metaPath.string()))
			{
				return ReadMeta(metaPath);
			}
		}

		std::filesystem::path metaPath = GetMetaPath(assetPath);

		if (std::filesystem::exists(metaPath))
		{
			AssetMetadata meta = ReadMeta(metaPath);

			uint64_t currentHash = ComputeFileHash(assetPath);
			if (currentHash != 0 && meta.contentHash != currentHash)
			{
				meta.contentHash = currentHash;
				WriteMeta(metaPath, meta);
			}
			return meta;
		}

		AssetMetadata meta;
		meta.uuid = UUID();
		meta.assetType = type;
		meta.contentHash = ComputeFileHash(assetPath);
		meta.isGenerated = true;

		WriteMeta(metaPath, meta);
		return meta;
	}

} // namespace Chained
