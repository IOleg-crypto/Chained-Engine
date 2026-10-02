// thread_pool_tests.cpp
// Consolidated scenario-based tests for ThreadPool.
// Follows Arrange-Act-Assert (AAA) pattern without fixtures.

#include "engine/common/thread_pool.h"
#include "gtest/gtest.h"

#include <atomic>
#include <chrono>
#include <vector>

using namespace Chained;

// ============================================================================
// 1. POSITIVE SCENARIO (Happy Path: Execution, Futures, and Concurrency)
// ============================================================================
TEST(ThreadPoolModule, Positive_TaskExecutionAndConcurrency)
{
	// Arrange
	ThreadPool pool(4);

	// Act (Enqueue single task with return value)
	auto future = pool.Enqueue([](int a, int b) { return a + b; }, 3, 7);

	// Assert
	EXPECT_EQ(future.get(), 10);

	// Act (Concurrent execution of 20 tasks)
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

	// Assert
	EXPECT_EQ(counter.load(), 20);

	// Act (WaitIdle synchronization)
	std::atomic<int> idleCounter{0};
	for (int i = 0; i < 5; ++i)
	{
		pool.QueueTask([&idleCounter]() {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			idleCounter.fetch_add(1, std::memory_order_relaxed);
		});
	}
	pool.WaitIdle();

	// Assert
	EXPECT_EQ(idleCounter.load(), 5);
}

// ============================================================================
// 2. NEGATIVE SCENARIO (Shutdown Rejection Safety)
// ============================================================================
TEST(ThreadPoolModule, Negative_EnqueueAfterShutdownThrows)
{
	// Arrange
	ThreadPool pool(2);

	// Act
	pool.Shutdown();

	// Assert
	EXPECT_THROW(pool.Enqueue([]() { return 1; }), std::runtime_error);
	EXPECT_THROW(pool.QueueTask([]() {}), std::runtime_error);
}
