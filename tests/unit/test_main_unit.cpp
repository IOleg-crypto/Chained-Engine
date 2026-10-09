#include <gtest/gtest.h>

#include "engine/core/log.h"

int main(int argc, char** argv)
{
	::Chained::Log::Init();
	::testing::InitGoogleTest(&argc, argv);
	const int result = RUN_ALL_TESTS();
	::Chained::Log::Shutdown();
	return result;
}
