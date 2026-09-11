#ifndef CH_APPLICATION_TYPES_H
#define CH_APPLICATION_TYPES_H

#include "engine/core/window.h"
#include <filesystem>
#include <string>

namespace Chained
{
	struct ApplicationCommandLineArgs
	{
		int Count = 0;
		char** Args = nullptr;
	};
	struct ApplicationSpecification
	{
		std::string Name;
		std::string WorkingDirectory;
		std::filesystem::path EngineRoot;
		std::filesystem::path SourceResourcesDir; // Engine resources from source tree (dev mode)
		std::filesystem::path SourceAssetsDir;	  // Game assets from source tree (dev mode)
		std::string ResourcesDir;
		WindowProperties Window;
		ApplicationCommandLineArgs CommandLineArgs;
		bool Headless = false;
		bool EnableScripting = true;
	};

} // namespace Chained

#endif // CH_APPLICATION_TYPES_H
