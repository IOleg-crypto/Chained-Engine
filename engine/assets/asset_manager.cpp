#include "engine/assets/asset_manager.h"
#include "engine/assets/asset_pack_store.h"
#include "engine/common/thread_pool.h"
#include "engine/core/profiler.h"
#include "engine/core/service_locator.h"

#include "engine/assets/asset_metadata.h"
#include "engine/assets/loaders/font_loader.h"
#include "engine/assets/loaders/model_loader.h"
#include "engine/assets/loaders/audio_loader.h"
#include "engine/assets/loaders/material_loader.h"

#include "engine/assets/loaders/texture_loader.h"
#include "engine/assets/loaders/environment_loader.h"
#include "engine/assets/loaders/shader_loader.h"
#include "engine/assets/loaders/anim_graph_loader.h"

namespace Chained
{
	constexpr size_t kMaxAssetFinalizationsPerFrame = 128;
	constexpr auto kMaxAssetFinalizeBudget = std::chrono::milliseconds(12);

	AssetManager::AssetManager()
		: m_PackStore(std::make_unique<AssetPackStore>(m_PathResolver))
	{
	}

	void AssetManager::Initialize()
	{
		if (!m_PackStore)
		{
			m_PackStore = std::make_unique<AssetPackStore>(m_PathResolver);
		}

		RegisterLoader(AssetType::Model, std::make_unique<ModelLoader>());
		RegisterLoader(AssetType::Texture, std::make_unique<TextureLoader>());
		RegisterLoader(AssetType::Shader, std::make_unique<ShaderLoader>());
		RegisterLoader(AssetType::Environment, std::make_unique<EnvironmentLoader>());
		RegisterLoader(AssetType::Font, std::make_unique<FontLoader>());
		RegisterLoader(AssetType::Audio, std::make_unique<AudioLoader>());
		RegisterLoader(AssetType::Material, std::make_unique<MaterialLoader>());
		RegisterLoader(AssetType::AnimationGraph, std::make_unique<AnimGraphLoader>());
	}

	void AssetManager::Shutdown()
	{
		constexpr int kMaxIter = 5000;
		int iter = 0;
		while (HasBackgroundWork() && iter++ < kMaxIter)
		{
			FinalizePendingLoads();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		if (iter >= kMaxIter)
		{
			CH_CORE_ERROR("AssetManager: Shutdown timed out — some assets may still be loading.");
		}
		if (auto* tp = ServiceLocator::TryGet<ThreadPool>())
		{
			tp->WaitIdle();
		}
		FinalizePendingLoads();
		if (m_PackStore)
		{
			m_PackStore->CloseAllPacks();
		}
	}

	void AssetManager::Update(Timestep ts)
	{
		if (m_HotReloadInterval > 0.0f && !IsPacked())
		{
			m_HotReloadAccumulator += ts.GetSeconds();
			if (m_HotReloadAccumulator >= m_HotReloadInterval)
			{
				m_HotReloadAccumulator = 0.0f;
				CheckAssetHotReload();
			}
		}

		FinalizePendingLoads();
	}

	std::vector<AssetManager::StaleAsset> AssetManager::CollectStaleAssets(int thresholdSeconds) const
	{
		CH_PROFILE_FUNCTION();
		std::vector<StaleAsset> stale;

		// Snapshot the cache under a short lock
		std::vector<std::pair<AssetHandle, std::shared_ptr<Asset>>> snapshot;
		{
			std::lock_guard<std::recursive_mutex> lock(m_AssetLock);
			snapshot.reserve(m_AssetCache.size());
			for (const auto& [handle, asset] : m_AssetCache)
			{
				if (asset && asset->GetState() != AssetState::Loading)
				{
					snapshot.emplace_back(handle, asset);
				}
			}
		}

		for (const auto& [handle, asset] : snapshot)
		{
			AssetType type = asset->GetType();
			if (type != AssetType::Model && type != AssetType::Texture && type != AssetType::Material)
			{
				continue;
			}

			const std::string& path = asset->GetPath();
			if (path.empty() || path.front() == '*')
			{
				continue;
			}

			std::error_code ec;
			auto fileTime = std::filesystem::last_write_time(path, ec);
			if (ec)
			{
				continue;
			}

			auto it = m_AssetLastWriteTimes.find(handle);
			if (it == m_AssetLastWriteTimes.end())
			{
				m_AssetLastWriteTimes[handle] = fileTime;
				continue;
			}

			if (fileTime > it->second)
			{
				it->second = fileTime;
				stale.push_back({handle, type, path});
			}
		}

		return stale;
	}

	void AssetManager::CheckAssetHotReload()
	{
		auto stale = CollectStaleAssets(10);
		for (const auto& [handle, type, path] : stale)
		{
			CH_CORE_INFO("AssetManager: Hot-reloading recently modified {} '{}'",
						 type == AssetType::Model	  ? "model"
						 : type == AssetType::Texture ? "texture"
													  : "material",
						 std::filesystem::path(path).filename().string());
			ReloadAsset(handle, type);
		}
	}

	AssetManager::~AssetManager()
	{
		Shutdown();
		std::lock_guard<std::recursive_mutex> lock(m_AssetLock);
		m_AssetCache.clear();
		m_PathResolver.ClearCache();
		m_Loaders.clear();
		m_PackStore.reset();
	}

	void AssetManager::RegisterLoader(AssetType type, std::unique_ptr<IAssetLoader> loader)
	{
		m_Loaders[type] = std::move(loader);
	}

	bool AssetManager::ExecuteLoad(const std::shared_ptr<Asset>& asset, IAssetLoader* loader,
								   const std::string& resolved)
	{
		try
		{
			std::string loaderError;
			if (!loader->Load(asset, resolved, &loaderError))
			{
				asset->Fail(loaderError.empty() ? ("AssetManager: Load failed for '" + resolved + "'") : loaderError);
				return false;
			}

			asset->ClearError();
			return true;
		} catch (const std::exception& e)
		{
			asset->Fail(std::string("AssetManager: Load exception for '") + resolved + "': " + e.what());
		} catch (...)
		{
			asset->Fail(std::string("AssetManager: Load exception for '") + resolved + "' with unknown exception");
		}
		return false;
	}

	void AssetManager::StartAsyncLoad(const std::shared_ptr<Asset>& asset, IAssetLoader* loader,
									  const std::string& resolved)
	{
		if (!loader->IsAsync())
		{
			if (ExecuteLoad(asset, loader, resolved))
			{
				asset->OnLoaded();
				asset->SetState(AssetState::Ready);
			}
			return;
		}

		m_LoadingCount.fetch_add(1, std::memory_order_relaxed);

		if (auto* tp = ServiceLocator::TryGet<ThreadPool>())
		{
			tp->QueueTask([this, asset, loader, resolved]() {
				if (ExecuteLoad(asset, loader, resolved))
				{
					std::lock_guard<std::mutex> lock(m_PendingMutex);
					m_PendingAssets.push_back(asset);
				}
				m_LoadingCount.fetch_sub(1, std::memory_order_relaxed);
			});
		}
	}

	std::shared_ptr<Asset> AssetManager::LoadAsset(const std::string& path, AssetType type)
	{
		if (path.empty() || path.front() == '*')
		{
			return nullptr;
		}

		std::string resolved = ResolvePath(path);

		IAssetLoader* loader = nullptr;
		std::shared_ptr<Asset> asset;

		{
			std::lock_guard<std::recursive_mutex> lock(m_AssetLock);
			if (auto it = m_PathResolver.ResolveToHandle(resolved); it != AssetHandle(0))
			{
				auto handle = it;
				if (auto currentIt = m_AssetCache.find(handle); currentIt != m_AssetCache.end())
				{
					return currentIt->second;
				}
			}

			auto loaderIt = m_Loaders.find(type);
			if (loaderIt == m_Loaders.end())
			{
				CH_CORE_ERROR("AssetManager: No loader registered for type {}", (int)type);
				return nullptr;
			}

			asset = loaderIt->second->Create();
			if (!asset)
			{
				return nullptr;
			}

			loader = loaderIt->second.get();

			// Load or create .meta sidecar to get a stable UUID
			auto meta = LoadOrCreateMeta(resolved, type);
			AssetHandle stableHandle = meta.uuid;
			asset->SetID(stableHandle);
			asset->SetPath(resolved);
			asset->SetState(AssetState::Loading);
			asset->ClearError();
			m_AssetCache[stableHandle] = asset;
			m_PathResolver.RegisterHandle(resolved, stableHandle);
		}

		StartAsyncLoad(asset, loader, resolved);

		return asset;
	}

	std::shared_ptr<Asset> AssetManager::GetAsset(AssetHandle handle)
	{
		if (handle != 0)
		{
			std::lock_guard<std::recursive_mutex> lock(m_AssetLock);
			if (auto it = m_AssetCache.find(handle); it != m_AssetCache.end())
			{
				return it->second;
			}
		}

		return nullptr;
	}

	void AssetManager::FinalizePendingLoads()
	{
		CH_PROFILE_FUNCTION();

		const auto updateStart = std::chrono::steady_clock::now();
		size_t finalizedCount = 0;

		while (finalizedCount < kMaxAssetFinalizationsPerFrame)
		{
			std::shared_ptr<Asset> asset;
			{
				std::lock_guard<std::mutex> lock(m_PendingMutex);
				if (m_PendingAssets.empty())
				{
					return;
				}

				asset = std::move(m_PendingAssets.front());
				m_PendingAssets.pop_front();
			}

			if (!asset)
			{
				continue;
			}

			try
			{
				asset->ClearError();
				asset->OnLoaded();
				if (asset->GetState() != AssetState::Failed)
				{
					asset->SetState(AssetState::Ready);
				}
			} catch (const std::exception& e)
			{
				CH_CORE_ERROR("AssetManager: Finalization failed for '{}': {}", asset->GetPath(), e.what());
				asset->Fail(std::string("AssetManager: Finalization failed for '") + asset->GetPath() +
							"': " + e.what());
			} catch (...)
			{
				CH_CORE_ERROR("AssetManager: Finalization failed for '{}' with unknown exception", asset->GetPath());
				asset->Fail(std::string("AssetManager: Finalization failed for '") + asset->GetPath() +
							"' with an unknown exception");
			}

			++finalizedCount;

			if ((std::chrono::steady_clock::now() - updateStart) >= kMaxAssetFinalizeBudget)
			{
				break;
			}
		}
	}

	size_t AssetManager::GetPendingFinalizeCount() const
	{
		std::lock_guard<std::mutex> lock(m_PendingMutex);
		return m_PendingAssets.size();
	}

	size_t AssetManager::GetLoadingAssetCount() const
	{
		return m_LoadingCount.load(std::memory_order_relaxed);
	}

	bool AssetManager::HasBackgroundWork() const
	{
		{
			std::lock_guard<std::mutex> lock(m_PendingMutex);
			if (!m_PendingAssets.empty())
			{
				return true;
			}
		}
		return m_LoadingCount.load(std::memory_order_relaxed) > 0;
	}
	void AssetManager::ReloadAsset(AssetHandle handle, AssetType type)
	{
		std::shared_ptr<Asset> asset;
		IAssetLoader* loader = nullptr;
		std::string path;

		{
			std::lock_guard<std::recursive_mutex> lock(m_AssetLock);

			auto it = m_AssetCache.find(handle);
			if (it == m_AssetCache.end())
			{
				return;
			}

			auto loaderIt = m_Loaders.find(type);
			if (loaderIt == m_Loaders.end())
			{
				return;
			}

			asset = it->second;
			loader = loaderIt->second.get();
			path = asset->GetPath();
			if (path.empty())
			{
				return;
			}
		}

		std::string resolved = ResolvePath(path);
		asset->SetState(AssetState::Loading);
		if (ExecuteLoad(asset, loader, resolved))
		{
			asset->OnLoaded();
			asset->SetState(AssetState::Ready);

			// Update content hash in .meta after successful reload
			if (std::filesystem::exists(resolved))
			{
				auto metaPath = GetMetaPath(resolved);
				if (std::filesystem::exists(metaPath))
				{
					auto meta = ReadMeta(metaPath);
					meta.contentHash = ComputeFileHash(resolved);
					WriteMeta(metaPath, meta);
				}
			}
		}
	}

	void AssetManager::RetryFailedAsset(AssetHandle handle, AssetType type)
	{
		std::lock_guard<std::recursive_mutex> lock(m_AssetLock);

		auto it = m_AssetCache.find(handle);
		if (it == m_AssetCache.end())
		{
			return;
		}

		auto& asset = it->second;
		if (!asset || asset->GetState() != AssetState::Failed)
		{
			return;
		}

		auto loaderIt = m_Loaders.find(type);
		if (loaderIt == m_Loaders.end())
		{
			return;
		}

		std::string path = asset->GetPath();
		if (path.empty())
		{
			return;
		}

		std::string resolved = ResolvePath(path);
		auto* loader = loaderIt->second.get();
		asset->SetState(AssetState::Loading);
		asset->ClearError();

		CH_CORE_INFO("AssetManager: Retrying failed asset '{}'", path);

		StartAsyncLoad(asset, loader, resolved);
	}

	void AssetManager::Invalidate(const std::string& path, bool deleteFromDisk)
	{
		if (path.empty())
		{
			return;
		}

		std::string resolved = ResolvePath(path);

		if (deleteFromDisk)
		{
			DeleteChasset(resolved);
		}

		std::lock_guard<std::recursive_mutex> lock(m_AssetLock);

		AssetHandle handle = m_PathResolver.ResolveToHandle(resolved);
		if (handle != AssetHandle(0))
		{
			m_AssetCache.erase(handle);
		}
		m_PathResolver.ResetPath(path);

		CH_CORE_INFO("AssetManager: Invalidated cache for '{}'", resolved);
	}

	void AssetManager::Unload(AssetHandle handle)
	{
		if (handle == AssetHandle(0))
		{
			return;
		}

		std::shared_ptr<Asset> asset;
		{
			std::lock_guard<std::recursive_mutex> lock(m_AssetLock);
			if (auto it = m_AssetCache.find(handle); it != m_AssetCache.end())
			{
				asset = it->second;
			}
		}

		asset->Unload();

		{
			std::lock_guard<std::recursive_mutex> lock(m_AssetLock);
			m_AssetCache.erase(handle);
		}

		// Remove path-to-handle mappings for this handle
		m_PathResolver.UnregisterHandle(handle);

		CH_CORE_INFO("AssetManager: Unloaded asset {}", (uint64_t)handle);
	}

	void AssetManager::Unload(const std::string& path)
	{
		AssetHandle handle = m_PathResolver.ResolveToHandle(path);
		if (handle != AssetHandle(0))
		{
			Unload(handle);
		}
	}

	void AssetManager::UnloadUnused()
	{
		std::vector<AssetHandle> toUnload;
		{
			std::lock_guard<std::recursive_mutex> lock(m_AssetLock);
			for (const auto& [handle, asset] : m_AssetCache)
			{
				if (asset && asset.use_count() == 1)
				{
					toUnload.push_back(handle);
				}
			}
		}

		for (AssetHandle handle : toUnload)
		{
			Unload(handle);
		}

		if (!toUnload.empty())
		{
			CH_CORE_INFO("AssetManager: Unloaded {} unused assets", toUnload.size());
		}
	}

	bool AssetManager::DeleteChasset(const std::string& path)
	{
		std::filesystem::path modelPath = path;
		std::filesystem::path chassetPath = modelPath;
		chassetPath.replace_extension(".chasset");

		std::error_code ec;
		if (std::filesystem::exists(chassetPath, ec))
		{
			std::filesystem::remove(chassetPath, ec);
			if (!ec)
			{
				CH_CORE_INFO("AssetManager: Deleted .chasset '{}'", chassetPath.filename().string());
				return true;
			}
			CH_CORE_WARN("AssetManager: Failed to delete .chasset '{}': {}", chassetPath.string(), ec.message());
		}
		return false;
	}

	size_t AssetManager::DeleteAllChassets()
	{
		if (m_PathResolver.GetAssetDirectory().empty())
		{
			CH_CORE_WARN("AssetManager: Asset directory not set, cannot clear .chasset cache");
			return 0;
		}

		size_t deleted = 0;
		std::error_code ec;
		for (const auto& entry : std::filesystem::recursive_directory_iterator(m_PathResolver.GetAssetDirectory(), ec))
		{
			if (entry.is_regular_file() && entry.path().extension() == ".chasset")
			{
				std::filesystem::remove(entry.path(), ec);
				if (!ec)
				{
					CH_CORE_INFO("AssetManager: Deleted .chasset '{}'", entry.path().filename().string());
					++deleted;
				}
				else
				{
					CH_CORE_WARN("AssetManager: Failed to delete .chasset '{}': {}", entry.path().string(),
								 ec.message());
				}
			}
		}

		CH_CORE_INFO("AssetManager: Cleared .chasset cache ({} file(s) deleted)", deleted);

		if (deleted > 0)
		{
			std::lock_guard<std::recursive_mutex> lock(m_AssetLock);
			m_AssetCache.clear();
			m_PathResolver.ClearCache();
		}

		return deleted;
	}

	std::vector<std::string> AssetManager::GetStaleAssets() const
	{
		auto stale = CollectStaleAssets(30);
		std::vector<std::string> paths;
		paths.reserve(stale.size());
		for (auto& s : stale)
		{
			paths.push_back(std::move(s.path));
		}
		return paths;
	}

	size_t AssetManager::ReloadAllStale()
	{
		auto stale = CollectStaleAssets(30);
		for (const auto& [handle, type, path] : stale)
		{
			ReloadAsset(handle, type);
		}

		if (!stale.empty())
		{
			CH_CORE_INFO("AssetManager: Reloaded {} stale assets", stale.size());
		}

		return stale.size();
	}

	bool AssetManager::OpenPack(const std::filesystem::path& packPath)
	{
		std::lock_guard<std::recursive_mutex> lock(m_AssetLock);
		return m_PackStore ? m_PackStore->OpenPack(packPath) : false;
	}

	size_t AssetManager::OpenAllPacksInDirectory(const std::filesystem::path& dir)
	{
		std::lock_guard<std::recursive_mutex> lock(m_AssetLock);
		return m_PackStore ? m_PackStore->OpenAllPacksInDirectory(dir) : 0;
	}

	void AssetManager::CloseAllPacks()
	{
		std::lock_guard<std::recursive_mutex> lock(m_AssetLock);
		if (m_PackStore)
		{
			m_PackStore->CloseAllPacks();
		}
	}

	size_t AssetManager::GetOpenPackCount() const
	{
		return m_PackStore ? m_PackStore->GetOpenPackCount() : 0;
	}

	bool AssetManager::IsPacked() const
	{
		return m_PackStore ? m_PackStore->IsPacked() : false;
	}

	std::vector<uint8_t> AssetManager::ReadAssetData(const std::string& assetPath)
	{
		std::lock_guard<std::recursive_mutex> lock(m_AssetLock);
		return m_PackStore->ReadAssetData(assetPath);
	}

	std::string AssetManager::ReadText(const std::string& path)
	{
		auto data = ReadAssetData(path);
		if (data.empty())
		{
			return {};
		}
		return std::string(data.begin(), data.end());
	}

	bool AssetManager::FileExists(const std::string& path) const
	{
		std::lock_guard<std::recursive_mutex> lock(m_AssetLock);
		return m_PackStore->FileExists(path);
	}

	bool AssetManager::HasAsset(const std::string& path) const
	{
		return FileExists(path);
	}

	void AssetManager::EnumeratePackedPaths(const std::function<void(std::string_view)>& callback) const
	{
		m_PackStore->EnumeratePackedPaths(callback);
	}

	std::vector<uint8_t> AssetManager::ReadProjectAsset(const std::filesystem::path& absolutePath)
	{
		std::lock_guard<std::recursive_mutex> lock(m_AssetLock);
		return m_PackStore->ReadProjectAsset(absolutePath);
	}

} // namespace Chained
