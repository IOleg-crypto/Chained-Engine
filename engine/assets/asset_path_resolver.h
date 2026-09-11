#ifndef CH_ASSET_PATH_RESOLVER_H
#define CH_ASSET_PATH_RESOLVER_H

#include "engine/assets/asset.h"
#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

namespace Chained
{

	class AssetPathResolver
	{
	public:
		void SetAssetDirectory(const std::filesystem::path& path)
		{
			m_AssetDirectory = path;
		}
		void SetProjectDirectory(const std::filesystem::path& path)
		{
			m_ProjectDirectory = path;
		}
		void SetEngineRoot(const std::filesystem::path& path)
		{
			m_EngineRoot = path;
		}
		void SetSourceResourcesDir(const std::filesystem::path& path)
		{
			m_SourceResourcesDir = path;
		}
		void SetSourceAssetsDir(const std::filesystem::path& path)
		{
			m_SourceAssetsDir = path;
		}

	public:
		std::string ResolvePath(const std::string& path) const;
		AssetHandle ResolveToHandle(const std::string& path) const;
		void RegisterHandle(const std::string& path, AssetHandle handle);
		void UnregisterHandle(AssetHandle handle);

		void ResetPath(const std::string& path);
		void ClearCache();

		std::string ResolvePackKey(const std::string& assetPath) const;

	public:
		const std::filesystem::path& GetAssetDirectory() const
		{
			return m_AssetDirectory;
		}
		const std::filesystem::path& GetProjectDirectory() const
		{
			return m_ProjectDirectory;
		}
		const std::filesystem::path& GetEngineRoot() const
		{
			return m_EngineRoot;
		}
		const std::filesystem::path& GetSourceResourcesDir() const
		{
			return m_SourceResourcesDir;
		}
		const std::filesystem::path& GetSourceAssetsDir() const
		{
			return m_SourceAssetsDir;
		}

	private:
		// Internal mutex — never acquire from outside AssetPathResolver.
		// If you need to call AssetPathResolver while holding m_AssetLock in AssetManager,
		// ensure m_AssetLock is acquired FIRST (see lock hierarchy in asset_manager.h).
		mutable std::mutex m_PathMutex;
		mutable std::unordered_map<std::string, std::string> m_PathCache;
		mutable std::unordered_map<std::string, AssetHandle> m_PathToHandle;

		std::filesystem::path m_AssetDirectory;
		std::filesystem::path m_ProjectDirectory;
		std::filesystem::path m_EngineRoot;
		std::filesystem::path m_SourceResourcesDir;
		std::filesystem::path m_SourceAssetsDir;
	};

} // namespace Chained

#endif // CH_ASSET_PATH_RESOLVER_H
