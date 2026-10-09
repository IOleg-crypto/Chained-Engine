#include <gtest/gtest.h>

#include "engine/core/log.h"

#include <cstdio>

namespace
{
	// Temporary CI diagnostics: records how far the process gets so a hang at
	// exit can be localized from the uploaded discovery_markers.log artifact.
	void Mark(const char* text)
	{
		std::fprintf(stdout, "MARK %s\n", text);
		std::fflush(stdout);
		if (FILE* f = std::fopen("discovery_markers.log", "a"))
		{
			std::fprintf(f, "%s\n", text);
			std::fclose(f);
		}
	}

	struct StaticDtorMarker
	{
		~StaticDtorMarker()
		{
			Mark("STATIC_DTOR");
		}
	};
	StaticDtorMarker s_staticDtorMarker;
} // namespace

int main(int argc, char** argv)
{
	Mark("MAIN_BEGIN");
	::Chained::Log::Init();
	Mark("LOG_INIT");
	::testing::InitGoogleTest(&argc, argv);
	Mark("GTEST_INIT");
	const int result = RUN_ALL_TESTS();
	Mark("RUN_DONE");
	::Chained::Log::Shutdown();
	Mark("LOG_SHUTDOWN");
	return result;
}
