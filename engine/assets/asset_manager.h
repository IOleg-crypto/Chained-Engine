#ifndef CH_ASSET_MANAGER_H
#define CH_ASSET_MANAGER_H

#include "engine/assets/loaders/iasset_loader.h"
#include "engine/assets/asset_pack_store.h"
#include "engine/assets/asset_path_resolver.h"
#include "engine/core/service.h"
#include "engine/common/timestep.h"
#include <filesystem>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <atomic>
#include <unordered_map>
#include <vector>

namespace Chained
{

	class AssetManager : public Service
	{
	public:
		AssetManager();
		~AssetManager();
		virtual void Initialize();
		virtual void Shutdown();
		void Update(Timestep ts);

		void RegisterLoader(AssetType type, std::unique_ptr<IAssetLoader> loader);

		void Unload(AssetHandle handle);
		void Unload(const std::string& path);
		void UnloadUnused();

		void SetAssetDirectory(const std::filesystem::path& path)
		{
			m_PathResolver.SetAssetDirectory(path);
		}
		void SetProjectDirectory(const std::filesystem::path& path)
		{
			m_PathResolver.SetProjectDirectory(path);
		}
		void SetEngineRoot(const std::filesystem::path& path)
		{
			m_PathResolver.SetEngineRoot(path);
		}
		void SetSourceResourcesDir(const std::filesystem::path& path)
		{
			m_PathResolver.SetSourceResourcesDir(path);
		}
		void SetSourceAssetsDir(const std::filesystem::path& path)
		{
			m_PathResolver.SetSourceAssetsDir(path);
		}

		// Pack I/O — delegated to AssetPackStore
		bool OpenPack(const std::filesystem::path& packPath);
		size_t OpenAllPacksInDirectory(const std::filesystem::path& dir);
		void CloseAllPacks();
		size_t GetOpenPackCount() const;
		bool IsPacked() const;
		std::vector<uint8_t> ReadAssetData(const std::string& assetPath);
		std::string ReadText(const std::string& path);
		bool HasAsset(const std::string& path) const;
		bool FileExists(const std::string& path) const;
		void EnumeratePackedPaths(const std::function<void(std::string_view)>& callback) const;
		std::vector<uint8_t> ReadProjectAsset(const std::filesystem::path& absolutePath);

		AssetPackStore& GetPackStore()
		{
			return *m_PackStore;
		}

		const std::filesystem::path& GetAssetDirectory() const
		{
			return m_PathResolver.GetAssetDirectory();
		}
		const std::filesystem::path& GetProjectDirectory() const
		{
			return m_PathResolver.GetProjectDirectory();
		}
		const std::filesystem::path& GetEngineRoot() const
		{
			return m_PathResolver.GetEngineRoot();
		}

		std::string ResolvePath(const std::string& path) const
		{
			return m_PathResolver.ResolvePath(path);
		}
		AssetHandle ResolveToHandle(const std::string& path) const
		{
			return m_PathResolver.ResolveToHandle(path);
		}

		template <typename T> std::shared_ptr<T> Get(const std::string& path)
		{
			return Load<T>(path);
		}

		template <typename T> std::shared_ptr<T> Get(AssetHandle handle)
		{
			return std::static_pointer_cast<T>(GetAsset(handle));
		}

		template <typename T> std::shared_ptr<T> GetByUUID(uint64_t uuid)
		{
			return std::static_pointer_cast<T>(GetAsset(AssetHandle(uuid)));
		}

		template <typename T> std::shared_ptr<T> Load(const std::string& path)
		{
			return std::static_pointer_cast<T>(LoadAsset(path, T::GetStaticType()));
		}

		size_t GetPendingFinalizeCount() const;
		size_t GetLoadingAssetCount() const;
		bool HasBackgroundWork() const;

		void FinalizePendingLoads();

		void SetHotReloadInterval(float seconds)
		{
			m_HotReloadInterval = seconds;
		}
		float GetHotReloadInterval() const
		{
			return m_HotReloadInterval;
		}

		template <typename T> void Reload(const std::string& path)
		{
			AssetHandle handle = ResolveToHandle(path);
			ReloadAsset(handle, T::GetStaticType());
		}

		void Invalidate(const std::string& path, bool deleteFromDisk = false);
		bool DeleteChasset(const std::string& path);
		size_t DeleteAllChassets();

		std::vector<std::string> GetStaleAssets() const;
		size_t ReloadAllStale();

		std::shared_ptr<Asset> GetAsset(AssetHandle handle);
		std::shared_ptr<Asset> LoadAsset(const std::string& path, AssetType type);
		void RetryFailedAsset(AssetHandle handle, AssetType type);

	private:
		struct StaleAsset
		{
			AssetHandle handle;
			AssetType type;
			std::string path;
		};

		void ReloadAsset(AssetHandle handle, AssetType type);
		void CheckAssetHotReload();
		std::vector<StaleAsset> CollectStaleAssets(int thresholdSeconds) const;
		bool ExecuteLoad(const std::shared_ptr<Asset>& asset, IAssetLoader* loader, const std::string& resolved);
		void StartAsyncLoad(const std::shared_ptr<Asset>& asset, IAssetLoader* loader, const std::string& resolved);

		std::unordered_map<AssetHandle, std::shared_ptr<Asset>> m_AssetCache;
		std::unordered_map<AssetType, std::unique_ptr<IAssetLoader>> m_Loaders;

		// LOCK HIERARCHY (must always be acquired in this order):
		//   1. m_AssetLock (recursive_mutex)
		//   2. m_PathMutex inside AssetPathResolver (mutex, never acquire from outside)
		// Never hold m_PathMutex while acquiring m_AssetLock.
		mutable std::deque<std::shared_ptr<Asset>> m_PendingAssets;
		mutable std::mutex m_PendingMutex;
		mutable std::recursive_mutex m_AssetLock;
		std::atomic<size_t> m_LoadingCount{0};

		AssetPathResolver m_PathResolver;
		std::unique_ptr<AssetPackStore> m_PackStore;

		float m_HotReloadInterval = 3.0f;
		float m_HotReloadAccumulator = 0.0f;
		mutable std::unordered_map<AssetHandle, std::filesystem::file_time_type> m_AssetLastWriteTimes;
	};
} // namespace Chained

#endif // CH_ASSET_MANAGER_H
