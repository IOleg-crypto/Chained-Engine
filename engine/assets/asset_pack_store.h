#ifndef CH_ASSET_PACK_STORE_H
#define CH_ASSET_PACK_STORE_H

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace pack
{
	class Reader;
}

namespace Chained
{
	class AssetPathResolver;

	// Owns pack file readers and provides I/O access to packed assets.
	// Thread-safe: pack::Reader is documented as MT-Safe.
	class AssetPackStore
	{
	public:
		explicit AssetPackStore(AssetPathResolver& resolver);
		~AssetPackStore();

		AssetPackStore(const AssetPackStore&) = delete;
		AssetPackStore& operator=(const AssetPackStore&) = delete;

		bool OpenPack(const std::filesystem::path& packPath);
		size_t OpenAllPacksInDirectory(const std::filesystem::path& dir);
		void CloseAllPacks();

		bool IsPacked() const
		{
			return m_PackOpen;
		}

		size_t GetOpenPackCount() const
		{
			return m_PackReaders.size();
		}

		// Reads raw bytes for the given asset path (resolved via AssetPathResolver).
		std::vector<uint8_t> ReadAssetData(const std::string& assetPath) const;

		// Returns true if the asset exists in pack or on disk.
		bool FileExists(const std::string& path) const;

		// Reads a project asset by computing its relative path from projectDir.
		std::vector<uint8_t> ReadProjectAsset(const std::filesystem::path& absolutePath) const;

		// Enumerates all packed item paths.
		void EnumeratePackedPaths(const std::function<void(std::string_view)>& callback) const;

	private:
		// Checks if packKey exists in any open pack reader.
		bool ExistsInPacks(const std::string& packKey) const;

		// Tries to find packKey in packs, with fallback to "assets/" and "resources/" prefixes.
		// Returns non-empty data if found. outFoundKey optionally receives the matched key.
		std::vector<uint8_t> FindInPacks(const std::string& packKey, std::string* outFoundKey = nullptr) const;

		AssetPathResolver& m_Resolver;

		std::vector<std::unique_ptr<pack::Reader>> m_PackReaders;
		std::vector<std::filesystem::path> m_OpenedPackPaths;
		bool m_PackOpen = false;
	};

} // namespace Chained

#endif // CH_ASSET_PACK_STORE_H
