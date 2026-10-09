// service_locator_tests.cpp
// Consolidated scenario-based tests for ServiceLocator and Service lifecycle.
// Follows Arrange-Act-Assert (AAA) pattern without fixtures.

#include "engine/core/service.h"
#include "engine/core/service_locator.h"
#include "gtest/gtest.h"

#include <vector>

using namespace Chained;

namespace
{
	class MockServiceA : public Service
	{
	public:
		bool* shutdownFlag = nullptr;
		void Initialize() override
		{
		}
		void Shutdown() override
		{
			if (shutdownFlag)
			{
				*shutdownFlag = true;
			}
		}
	};

	class MockServiceB : public Service
	{
	public:
		bool* shutdownFlag = nullptr;
		void Initialize() override
		{
		}
		void Shutdown() override
		{
			if (shutdownFlag)
			{
				*shutdownFlag = true;
			}
		}
	};

	class OrderProbeService1 : public Service
	{
	public:
		std::vector<std::string>* shutdownLog = nullptr;
		OrderProbeService1(std::vector<std::string>* log)
			: shutdownLog(log)
		{
		}
		void Initialize() override
		{
		}
		void Shutdown() override
		{
			if (shutdownLog)
			{
				shutdownLog->push_back("First");
			}
		}
	};

	class OrderProbeService2 : public Service
	{
	public:
		std::vector<std::string>* shutdownLog = nullptr;
		OrderProbeService2(std::vector<std::string>* log)
			: shutdownLog(log)
		{
		}
		void Initialize() override
		{
		}
		void Shutdown() override
		{
			if (shutdownLog)
			{
				shutdownLog->push_back("Second");
			}
		}
	};

	class OrderProbeService3 : public Service
	{
	public:
		std::vector<std::string>* shutdownLog = nullptr;
		OrderProbeService3(std::vector<std::string>* log)
			: shutdownLog(log)
		{
		}
		void Initialize() override
		{
		}
		void Shutdown() override
		{
			if (shutdownLog)
			{
				shutdownLog->push_back("Third");
			}
		}
	};
} // namespace

// ============================================================================
// 1. POSITIVE SCENARIO (Registration, Availability, LIFO Reverse Shutdown)
// ============================================================================
TEST(ServiceLocatorModule, Positive_RegistrationLookupAndReverseShutdown)
{
	// Arrange
	ServiceLocator::Reset();
	EXPECT_FALSE(ServiceLocator::IsAvailable());

	// Act (Step 1: Provide service)
	auto sA = std::make_unique<MockServiceA>();
	auto* rawA = sA.get();
	ServiceLocator::Provide(std::move(sA));

	// Assert
	EXPECT_TRUE(ServiceLocator::IsAvailable());
	EXPECT_TRUE(ServiceLocator::Has<MockServiceA>());
	EXPECT_FALSE(ServiceLocator::Has<MockServiceB>());
	EXPECT_EQ(ServiceLocator::Get<MockServiceA>(), rawA);
	EXPECT_EQ(ServiceLocator::TryGet<MockServiceA>(), rawA);

	// Act (Step 2: Disable service)
	rawA->SetEnabled(false);

	// Assert
	EXPECT_EQ(ServiceLocator::TryGet<MockServiceA>(), nullptr);

	// Act (Re-enable)
	rawA->SetEnabled(true);
	EXPECT_EQ(ServiceLocator::TryGet<MockServiceA>(), rawA);

	// Arrange & Act (Step 3: Strict LIFO reverse shutdown order)
	ServiceLocator::Reset();

	std::vector<std::string> shutdownOrder;
	ServiceLocator::Provide(std::make_unique<OrderProbeService1>(&shutdownOrder));
	ServiceLocator::Provide(std::make_unique<OrderProbeService2>(&shutdownOrder));
	ServiceLocator::Provide(std::make_unique<OrderProbeService3>(&shutdownOrder));

	ServiceLocator::Shutdown();

	// Assert
	ASSERT_EQ(shutdownOrder.size(), 3u);
	EXPECT_EQ(shutdownOrder[0], "Third");
	EXPECT_EQ(shutdownOrder[1], "Second");
	EXPECT_EQ(shutdownOrder[2], "First");
	EXPECT_FALSE(ServiceLocator::IsAvailable());
}

// ============================================================================
// 2. NEGATIVE SCENARIO (Lock Protection, Duplicate Rejection Safety)
// ============================================================================
TEST(ServiceLocatorModule, Negative_LockAndRejectionSafety)
{
	// Arrange
	ServiceLocator::Reset();

	// Act & Assert (Step 1: Querying non-existent service returns nullptr safely)
	EXPECT_EQ(ServiceLocator::TryGet<MockServiceA>(), nullptr);

	// Act & Assert (Step 2: Duplicate registration rejected without leaks)
	auto original = std::make_unique<MockServiceA>();
	auto* rawOrig = original.get();
	ServiceLocator::Provide(std::move(original));
	ServiceLocator::Provide(std::make_unique<MockServiceA>()); // duplicate rejected

	EXPECT_EQ(ServiceLocator::TryGet<MockServiceA>(), rawOrig);

	// Act (Step 3: Lock prevents subsequent registrations)
	ServiceLocator::Lock();

	bool rejectedShutdownCalled = false;
	auto rejected = std::make_unique<MockServiceB>();
	rejected->shutdownFlag = &rejectedShutdownCalled;
	ServiceLocator::Provide(std::move(rejected));

	// Assert
	EXPECT_FALSE(ServiceLocator::Has<MockServiceB>());

	ServiceLocator::Shutdown();
	EXPECT_FALSE(rejectedShutdownCalled);
}
