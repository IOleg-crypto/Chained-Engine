#ifndef CH_PLATFORM_H
#define CH_PLATFORM_H

#include "engine/common/base.h"
#include <filesystem>
#include <string>
#include <vector>
#include <optional>

namespace Chained
{
	namespace Platform
	{
		// Returns the absolute path to the directory containing the current executable.
		CH_API std::filesystem::path GetExecutableDirectory();
		// Returns the current system time in seconds.
		CH_API float GetTime();
		// Sleeps the current thread for the specified milliseconds.
		CH_API void Sleep(uint32_t milliseconds);
	} // namespace Platform
} // namespace Chained

#endif // CH_PLATFORM_H
