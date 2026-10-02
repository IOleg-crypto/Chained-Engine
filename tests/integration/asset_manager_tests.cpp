// asset_manager_tests.cpp
// Consolidated scenario-based tests for AssetManager.
// Follows Arrange-Act-Assert (AAA) pattern without fixtures.

#include "engine/assets/asset_manager.h"
#include "engine/assets/loaders/iasset_loader.h"
#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <future>
#include <string>
#include <thread>
#include <vector>

using namespace Chained;

namespace
{
	class TestAsset final : public Asset
	{
	public:
		TestAsset()
			: Asset(GetStaticType())
		{
		}
		static AssetType GetStaticType()
		{
			return AssetType::Texture;
		}

		int OnLoadedCalls = 0;
		void OnLoaded() override
		{
			++OnLoadedCalls;
		}
	};

	class TestAssetLoader final : public IAssetLoader
	{
	public:
		bool IsAsync() const override
		{
			return m_Async;
		}
		std::shared_ptr<Asset> Create() override
		{
			return std::make_shared<TestAsset>();
		}

		bool Load(std::shared_ptr<Asset> asset, const std::string& path, std::string* outError) override
		{
			++LoadCount;
			if (!m_ShouldSucceed)
			{
				if (outError)
				{
					*outError = "TestAssetLoader: simulated failure for " + path;
				}
				return false;
			}
			asset->SetPath(path);
			return true;
		}

		bool m_Async = false;
		bool m_ShouldSucceed = true;
		std::atomic<int> LoadCount{0};
	};

	struct ScopedAssetEnvironment
	{
		std::filesystem::path dir;
		ScopedAssetEnvironment(const std::string& name)
		{
			dir = std::filesystem::temp_directory_path() / ("chained_test_" + name);
			std::error_code ec;
			std::filesystem::remove_all(dir, ec);
			std::filesystem::create_directories(dir, ec);
		}
		~ScopedAssetEnvironment()
		{
			std::error_code ec;
			std::filesystem::remove_all(dir, ec);
		}
	};
} // namespace

// ============================================================================
// 1. POSITIVE SCENARIO (Happy Path: Full Lifecycle, Cache, and GC)
// ============================================================================
TEST(AssetManagerTest, Positive_FullLifecycleAndCache)
{
	// Arrange
	ScopedAssetEnvironment env("test_env_lifecycle");

	AssetManager am;
	am.SetProjectDirectory(std::filesystem::current_path());
	am.SetAssetDirectory(env.dir);
	am.SetEngineRoot(std::filesystem::current_path());

	auto loader = std::make_unique<TestAssetLoader>();
	auto* loaderPtr = loader.get();
	am.RegisterLoader(TestAsset::GetStaticType(), std::move(loader));

	const std::string path = "textures/character.png";

	// Act (Step 1: First load creates and caches asset)
	auto asset1 = am.Get<TestAsset>(path);

	// Assert
	ASSERT_NE(asset1, nullptr);
	EXPECT_EQ(asset1->GetState(), AssetState::Ready);
	EXPECT_EQ(asset1->OnLoadedCalls, 1);
	EXPECT_EQ(loaderPtr->LoadCount, 1);
	EXPECT_FALSE(asset1->GetPath().empty());

	// Act (Step 2: Second load with same path returns identical cached instance)
	auto asset2 = am.Get<TestAsset>(path);

	// Assert
	ASSERT_NE(asset2, nullptr);
	EXPECT_EQ(asset1.get(), asset2.get());
	EXPECT_EQ(loaderPtr->LoadCount, 1);

	// Act (Step 3: Resolve to handle and query by ID)
	AssetHandle handle = am.ResolveToHandle(path);
	auto assetByHandle = am.Get<TestAsset>(handle);

	// Assert
	EXPECT_NE((uint64_t)handle, 0ull);
	EXPECT_EQ(assetByHandle.get(), asset1.get());

	// Act (Step 4: Garbage Collection — while local references exist, asset stays in cache)
	am.UnloadUnused();

	// Assert
	EXPECT_EQ(am.Get<TestAsset>(handle).get(), asset1.get());

	// Act (Step 5: Release all local references and invoke UnloadUnused)
	asset1.reset();
	asset2.reset();
	assetByHandle.reset();
	am.UnloadUnused();

	// Act (Step 6: Requesting after purge reloads cleanly)
	auto reloaded = am.Get<TestAsset>(path);

	// Assert
	ASSERT_NE(reloaded, nullptr);
	EXPECT_EQ(reloaded->GetState(), AssetState::Ready);
	EXPECT_EQ(loaderPtr->LoadCount, 2);

	am.Shutdown();
}

// ============================================================================
// 2. NEGATIVE SCENARIO (Error Handling, Invalid Inputs, and Crash-Safety)
// ============================================================================
TEST(AssetManagerTest, Negative_ErrorHandlingAndInvalidInputs)
{
	// Arrange
	ScopedAssetEnvironment env("test_env_errors");

	AssetManager am;
	am.SetProjectDirectory(std::filesystem::current_path());
	am.SetAssetDirectory(env.dir);

	auto loader = std::make_unique<TestAssetLoader>();
	loader->m_ShouldSucceed = false;
	auto* loaderPtr = loader.get();
	am.RegisterLoader(TestAsset::GetStaticType(), std::move(loader));

	// Act & Assert (Step 1: Empty path returns empty resolve safely)
	EXPECT_TRUE(am.ResolvePath("").empty());

	// Act & Assert (Step 2: Failed load transitions asset to Failed state safely)
	auto failedAsset = am.Get<TestAsset>("corrupted_file.dummy");
	EXPECT_EQ(loaderPtr->LoadCount, 1);
	if (failedAsset)
	{
		EXPECT_EQ(failedAsset->GetState(), AssetState::Failed);
	}

	// Act & Assert (Step 3: Invalid / zero handle queries return nullptr)
	EXPECT_EQ(am.Get<TestAsset>(AssetHandle(0)), nullptr);
	EXPECT_EQ(am.Get<TestAsset>(AssetHandle(0xDEADBEEFCAFEBABEull)), nullptr);

	// Act & Assert (Step 4: Unloading non-existent handle is safe)
	AssetHandle dummyHandle(12345);
	EXPECT_NO_THROW(am.Unload(dummyHandle));
	EXPECT_NO_THROW(am.Unload(dummyHandle));
	EXPECT_NO_THROW(am.Unload("non_existent_path.dummy"));

	am.Shutdown();
}

// ============================================================================
// 3. STRESS SCENARIO (Concurrency and Async Loading Batch)
// ============================================================================
TEST(AssetManagerTest, Stress_ConcurrentAndAsyncLoading)
{
	// Arrange
	ScopedAssetEnvironment env("test_env_stress");

	AssetManager am;
	am.SetProjectDirectory(std::filesystem::current_path());
	am.SetAssetDirectory(env.dir);

	auto loader = std::make_unique<TestAssetLoader>();
	loader->m_Async = true;
	auto* loaderPtr = loader.get();
	am.RegisterLoader(TestAsset::GetStaticType(), std::move(loader));

	const std::string sharedPath = "stress/shared_mesh.dummy";

	// Act (Step 1: 8 concurrent threads query the exact same asset)
	std::vector<std::future<std::shared_ptr<TestAsset>>> futures;
	for (int i = 0; i < 8; ++i)
	{
		futures.push_back(
			std::async(std::launch::async, [&am, &sharedPath]() { return am.Get<TestAsset>(sharedPath); }));
	}

	std::vector<std::shared_ptr<TestAsset>> results;
	for (auto& f : futures)
	{
		results.push_back(f.get());
	}

	// Assert
	for (size_t i = 1; i < results.size(); ++i)
	{
		ASSERT_NE(results[i], nullptr);
		EXPECT_EQ(results[i].get(), results[0].get()) << "Thread " << i << " got different asset pointer";
	}
	EXPECT_EQ(loaderPtr->LoadCount, 1);

	// Act (Step 2: Batch async load 10 distinct assets)
	std::vector<std::shared_ptr<TestAsset>> batch;
	for (int i = 0; i < 10; ++i)
	{
		batch.push_back(am.Get<TestAsset>("stress/batch_" + std::to_string(i) + ".dummy"));
	}

	// Pump until all background work and finalizations complete
	for (int attempt = 0; attempt < 2000 && (am.HasBackgroundWork() || am.GetPendingFinalizeCount() > 0); ++attempt)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
		am.Update(Timestep(0.016f));
	}

	// Assert
	EXPECT_EQ(am.GetPendingFinalizeCount(), 0u);
	for (auto& asset : batch)
	{
		EXPECT_EQ(asset->GetState(), AssetState::Ready);
		EXPECT_EQ(asset->OnLoadedCalls, 1);
	}

	am.Shutdown();
}
