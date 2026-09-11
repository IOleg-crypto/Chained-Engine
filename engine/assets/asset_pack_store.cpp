#include "engine/assets/asset_pack_store.h"
#include "engine/assets/asset_path_resolver.h"
#include "engine/core/log.h"

#include <pack/reader.hpp>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <unordered_set>

namespace Chained
{

	AssetPackStore::AssetPackStore(AssetPathResolver& resolver)
		: m_Resolver(resolver)
	{
	}

	AssetPackStore::~AssetPackStore()
	{
		CloseAllPacks();
	}

	bool AssetPackStore::OpenPack(const std::filesystem::path& packPath)
	{
		std::error_code ec;
		if (!std::filesystem::exists(packPath, ec))
		{
			CH_CORE_WARN("AssetPackStore: Pack file not found: {}", packPath.string());
			return false;
		}

		try
		{
			auto reader = std::make_unique<pack::Reader>(packPath);
			CH_CORE_INFO("AssetPackStore: Opened pack '{}' ({} items)", packPath.string(), reader->getItemCount());
			m_OpenedPackPaths.push_back(packPath);
			m_PackReaders.push_back(std::move(reader));
			m_PackOpen = true;
			return true;
		} catch (const pack::Error& err)
		{
			CH_CORE_ERROR("AssetPackStore: Failed to open pack '{}': {}", packPath.string(), err.what());
			return false;
		}
	}

	size_t AssetPackStore::OpenAllPacksInDirectory(const std::filesystem::path& dir)
	{
		std::error_code ec;
		if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec))
		{
			return 0;
		}

		std::vector<std::filesystem::path> packFiles;
		for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
		{
			if (entry.is_regular_file() && entry.path().extension() == ".pack")
			{
				packFiles.push_back(entry.path());
			}
		}

		std::sort(packFiles.begin(), packFiles.end());

		size_t openedCount = 0;
		for (const auto& p : packFiles)
		{
			if (OpenPack(p))
			{
				++openedCount;
			}
		}

		return openedCount;
	}

	void AssetPackStore::CloseAllPacks()
	{
		m_PackReaders.clear();
		m_OpenedPackPaths.clear();
		m_PackOpen = false;
	}

	std::vector<uint8_t> AssetPackStore::FindInPacks(const std::string& packKey, std::string* outFoundKey) const
	{
		if (!m_PackOpen || m_PackReaders.empty())
		{
			return {};
		}

		// Try direct key first
		for (auto it = m_PackReaders.rbegin(); it != m_PackReaders.rend(); ++it)
		{
			auto& reader = *it;
			uint64_t idx = 0;
			if (reader->getItemIndex(packKey.c_str(), idx))
			{
				std::vector<uint8_t> data;
				reader->readItemData(idx, data);
				if (outFoundKey)
				{
					*outFoundKey = packKey;
				}
				return data;
			}
		}

		// Fallback: try "assets/" and "resources/" prefixes
		if (packKey.rfind("assets/", 0) != 0 && packKey.rfind("resources/", 0) != 0)
		{
			std::string altAssets = "assets/" + packKey;
			std::string altResources = "resources/" + packKey;

			for (auto it = m_PackReaders.rbegin(); it != m_PackReaders.rend(); ++it)
			{
				auto& reader = *it;
				uint64_t idx = 0;
				if (reader->getItemIndex(altAssets.c_str(), idx))
				{
					std::vector<uint8_t> data;
					reader->readItemData(idx, data);
					if (outFoundKey)
					{
						*outFoundKey = altAssets;
					}
					return data;
				}
				if (reader->getItemIndex(altResources.c_str(), idx))
				{
					std::vector<uint8_t> data;
					reader->readItemData(idx, data);
					if (outFoundKey)
					{
						*outFoundKey = altResources;
					}
					return data;
				}
			}
		}

		return {};
	}

	bool AssetPackStore::ExistsInPacks(const std::string& packKey) const
	{
		return !FindInPacks(packKey).empty();
	}

	std::vector<uint8_t> AssetPackStore::ReadAssetData(const std::string& assetPath) const
	{
		std::string packKey = m_Resolver.ResolvePackKey(assetPath);

		auto data = FindInPacks(packKey);
		if (!data.empty())
		{
			return data;
		}

		// Filesystem fallback (non-packed mode or missing from pack)
		std::string resolved = m_Resolver.ResolvePath(assetPath);
		std::ifstream file;
		if (!resolved.empty())
		{
			file.open(resolved, std::ios::binary | std::ios::ate);
		}
		if (!file.is_open())
		{
			file.open(assetPath, std::ios::binary | std::ios::ate);
		}
		if (!file.is_open() && assetPath.rfind("assets/", 0) != 0)
		{
			file.open("assets/" + assetPath, std::ios::binary | std::ios::ate);
		}
		if (file.is_open())
		{
			auto size = file.tellg();
			file.seekg(0);
			std::vector<uint8_t> fileData(static_cast<size_t>(size));
			file.read(reinterpret_cast<char*>(fileData.data()), size);
			return fileData;
		}

		return {};
	}

	bool AssetPackStore::FileExists(const std::string& path) const
	{
		std::string packKey = m_Resolver.ResolvePackKey(path);
		if (ExistsInPacks(packKey))
		{
			return true;
		}

		std::error_code ec;
		std::string resolved = m_Resolver.ResolvePath(path);
		if (!resolved.empty() && std::filesystem::exists(resolved, ec))
		{
			return true;
		}
		if (std::filesystem::exists(path, ec))
		{
			return true;
		}
		if (path.rfind("assets/", 0) != 0 && std::filesystem::exists("assets/" + path, ec))
		{
			return true;
		}
		return false;
	}

	std::vector<uint8_t> AssetPackStore::ReadProjectAsset(const std::filesystem::path& absolutePath) const
	{
		if (!m_PackOpen || m_PackReaders.empty() || m_Resolver.GetProjectDirectory().empty())
		{
			return {};
		}
		std::error_code ec;
		auto rel = std::filesystem::relative(absolutePath, m_Resolver.GetProjectDirectory(), ec);
		if (ec || rel.empty())
		{
			return {};
		}
		return ReadAssetData(rel.generic_string());
	}

	void AssetPackStore::EnumeratePackedPaths(const std::function<void(std::string_view)>& callback) const
	{
		if (!m_PackOpen || m_PackReaders.empty())
		{
			return;
		}
		std::unordered_set<std::string_view> seen;

		for (const auto& reader : m_PackReaders)
		{
			const uint64_t count = reader->getItemCount();
			for (uint64_t i = 0; i < count; ++i)
			{
				std::string_view itemPath = reader->getItemPath(i);
				if (seen.insert(itemPath).second)
				{
					callback(itemPath);
				}
			}
		}
	}

} // namespace Chained
