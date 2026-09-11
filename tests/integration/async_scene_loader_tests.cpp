#include "engine/scene/async_scene_loader.h"
#include "engine/common/thread_pool.h"
#include "engine/core/service_locator.h"
#include "engine/scene/scene_serializer.h"
#include <gtest/gtest.h>
#include <chrono>
#include <thread>

namespace Chained
{

	class AsyncSceneLoaderTest : public ::testing::Test
	{
	};

	TEST_F(AsyncSceneLoaderTest, InitialStateIsIdle)
	{
		AsyncSceneLoader loader;
		EXPECT_FALSE(loader.IsLoading());
		EXPECT_FLOAT_EQ(loader.GetProgress(), 0.0f);
	}

	TEST_F(AsyncSceneLoaderTest, FailsGracefullyOnNonExistentFile)
	{
		AsyncSceneLoader loader;
		bool callbackInvoked = false;
		std::shared_ptr<Scene> loadedScene = nullptr;
		std::string loadError;

		bool started = loader.LoadSceneAsync("non_existent_scene_path.chscene",
											 [&](std::shared_ptr<Scene> scene, const std::string& err) {
												 callbackInvoked = true;
												 loadedScene = scene;
												 loadError = err;
											 });

		EXPECT_TRUE(started);
		EXPECT_TRUE(loader.IsLoading());

		// Pump update loop until completion
		auto start = std::chrono::steady_clock::now();
		while (loader.IsLoading() && std::chrono::steady_clock::now() - start < std::chrono::seconds(5))
		{
			loader.OnUpdate(Timestep(0.016f));
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}

		EXPECT_TRUE(callbackInvoked);
		EXPECT_FALSE(loader.IsLoading());
		EXPECT_EQ(loadedScene, nullptr);
		EXPECT_FALSE(loadError.empty());
	}

} // namespace Chained
