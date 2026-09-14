#include <gtest/gtest.h>

#include "engine/core/log.h"
#include "engine/scene/component_registry.h"

int main(int argc, char** argv)
{
	::Chained::Log::Init();
	::Chained::ComponentRegistry::RegisterEngineComponents();
	::testing::InitGoogleTest(&argc, argv);
	const int result = RUN_ALL_TESTS();
	::Chained::Log::Shutdown();
	return result;
}
