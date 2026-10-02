#include "engine/common/thread_pool.h"
#include "gtest/gtest.h"
#include <atomic>
#include <chrono>
#include <vector>

using namespace Chained;

TEST(ThreadPoolTest, EnqueueReturnsCorrectResult)
{
	ThreadPool pool(2);
	auto future = pool.Enqueue([]() { return 42; });
	EXPECT_EQ(future.get(), 42);
}

TEST(ThreadPoolTest, EnqueueWithArguments)
{
	ThreadPool pool(2);
	auto future = pool.Enqueue([](int a, int b) { return a + b; }, 3, 7);
	EXPECT_EQ(future.get(), 10);
}

TEST(ThreadPoolTest, MultipleTasksExecute)
{
	ThreadPool pool(4);
	std::vector<std::future<int>> futures;

	for (int i = 0; i < 10; ++i)
	{
		futures.push_back(pool.Enqueue([i]() { return i * 2; }));
	}

	for (int i = 0; i < 10; ++i)
	{
		EXPECT_EQ(futures[i].get(), i * 2);
	}
}

TEST(ThreadPoolTest, TasksRunConcurrently)
{
	ThreadPool pool(4);
	std::atomic<int> counter{0};
	std::vector<std::future<void>> futures;

	for (int i = 0; i < 20; ++i)
	{
		futures.push_back(pool.Enqueue([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); }));
	}

	for (auto& f : futures)
	{
		f.get();
	}

	EXPECT_EQ(counter.load(), 20);
}

TEST(ThreadPoolTest, QueueTaskExecutesLambda)
{
	ThreadPool pool(1);
	std::atomic<bool> executed{false};
	pool.QueueTask([&executed]() { executed.store(true); });

	auto future = pool.Enqueue([]() { return 0; });
	future.get();
	EXPECT_TRUE(executed.load());
}

TEST(ThreadPoolTest, EnqueueAfterShutdownThrows)
{
	ThreadPool pool(1);
	pool.Shutdown();

	EXPECT_THROW(pool.Enqueue([]() { return 1; }), std::runtime_error);
}

TEST(ThreadPoolTest, QueueTaskAfterShutdownThrows)
{
	ThreadPool pool(1);
	pool.Shutdown();

	EXPECT_THROW(pool.QueueTask([]() {}), std::runtime_error);
}

TEST(ThreadPoolTest, WaitIdleWaitsForActiveTasks)
{
	ThreadPool pool(2);
	std::atomic<int> completed{0};
	for (int i = 0; i < 5; ++i)
	{
		pool.QueueTask([&completed]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			completed.fetch_add(1, std::memory_order_relaxed);
		});
	}
	pool.WaitIdle();
	EXPECT_EQ(completed.load(), 5);
}
