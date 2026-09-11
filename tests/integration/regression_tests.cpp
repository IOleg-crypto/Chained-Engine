#include "engine/core/service_locator.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/loaders/iasset_loader.h"
#include "engine/scene/scene.h"
#include "engine/scene/components.h"
#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

using namespace Chained;

namespace
{
	class RegressionAsset final : public Asset
	{
	public:
		RegressionAsset()
			: Asset(GetStaticType())
		{
		}

		static AssetType GetStaticType()
		{
			return AssetType::None;
		}

		void OnLoaded() override
		{
		}
	};

	class RegressionLoader final : public IAssetLoader
	{
	public:
		bool IsAsync() const override
		{
			return m_Async;
		}
		std::shared_ptr<Asset> Create() override
		{
			return std::make_shared<RegressionAsset>();
		}
		bool Load(std::shared_ptr<Asset> asset, const std::string& path, std::string*) override
		{
			auto dummy = std::dynamic_pointer_cast<RegressionAsset>(asset);
			if (!dummy)
			{
				return false;
			}
			dummy->SetPath(path);
			return true;
		}
		bool m_Async = true;
	};

	std::string RegressionPath(const char* suffix)
	{
		static std::atomic<uint64_t> counter{0};
		return std::string("regression/") + suffix + "_" + std::to_string(++counter) + ".dummy";
	}
} // namespace

class RegressionTest : public ::testing::Test
{
protected:
	void SetUp() override
	{
		m_AssetManager = std::make_shared<AssetManager>();
		auto currentPath = std::filesystem::current_path();
		m_AssetManager->SetProjectDirectory(currentPath);
		m_AssetManager->SetAssetDirectory(currentPath / "test_regression");
		m_AssetManager->SetEngineRoot(currentPath);
		std::filesystem::create_directories(m_AssetManager->GetAssetDirectory());

		auto loader = std::make_unique<RegressionLoader>();
		loader->m_Async = true;
		m_AssetManager->RegisterLoader(RegressionAsset::GetStaticType(), std::move(loader));
	}

	void TearDown() override
	{
		if (m_AssetManager)
		{
			m_AssetManager->Shutdown();
		}
		m_AssetManager.reset();
	}

	std::shared_ptr<AssetManager> m_AssetManager;
};

// Regression: MultipleAsyncLoads only finalized 2 of 5 assets per frame
// due to 5ms time budget in FinalizePendingLoads().
// Fixed by calling Update() in a loop until GetPendingFinalizeCount() == 0.
TEST_F(RegressionTest, MultipleAsyncLoadsFinalizeAll)
{
	const int count = 5;
	std::vector<std::shared_ptr<RegressionAsset>> assets;

	for (int i = 0; i < count; ++i)
	{
		const std::string path = RegressionPath("async");
		assets.push_back(m_AssetManager->Get<RegressionAsset>(path));
	}

	for (int attempt = 0; attempt < 2000 && m_AssetManager->GetPendingFinalizeCount() < count; ++attempt)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	EXPECT_EQ(m_AssetManager->GetPendingFinalizeCount(), (size_t)count);

	for (int attempt = 0; attempt < 1000 && m_AssetManager->GetPendingFinalizeCount() > 0; ++attempt)
	{
		m_AssetManager->Update(Timestep(0.016f));
	}

	EXPECT_EQ(m_AssetManager->GetPendingFinalizeCount(), 0u);
	for (auto& asset : assets)
	{
		EXPECT_EQ(asset->GetState(), AssetState::Ready);
	}
}

// Regression: CopyEntity must reset UUID and script state
TEST_F(RegressionTest, CopyEntityResetsUUID)
{
	Scene scene;
	Entity src = scene.CreateEntity("Original");
	src.AddComponent<CameraComponent>();

	Entity dst = {scene.CopyEntity(src), scene.GetRegistryPtr()};
	EXPECT_NE(src.GetUUID(), dst.GetUUID());
	EXPECT_TRUE(dst.HasComponent<CameraComponent>());
}
