#include "engine/app/entry_point.h"
#include "engine/core/platform.h"
#include "engine/runtime/runtime_layer.h"

namespace Chained
{
	Application* CreateApplication(ApplicationCommandLineArgs args)
	{
		ApplicationSpecification spec;
		spec.Name = "ChainedDecos";
		spec.CommandLineArgs = args;
		spec.Headless = false;
		spec.EnableScripting = true;
		spec.EngineRoot = Platform::GetExecutableDirectory();
		spec.WorkingDirectory = Platform::GetExecutableDirectory().string();

		// Source directories for dev mode — set by CMake compile definitions
#ifdef CH_SOURCE_RESOURCES_DIR
		spec.SourceResourcesDir = CH_SOURCE_RESOURCES_DIR;
#endif
#ifdef CH_SOURCE_ASSETS_DIR
		spec.SourceAssetsDir = CH_SOURCE_ASSETS_DIR;
#endif

		// Resolve project path: first CLI arg or default
		std::filesystem::path projectPath;
		for (int i = 0; i < args.Count; ++i)
		{
			std::string arg = args.Args[i];
			if (arg.ends_with(".chproject"))
			{
				projectPath = arg;
				break;
			}
		}

		if (projectPath.empty() || !std::filesystem::exists(projectPath))
		{
			projectPath = std::filesystem::path(spec.WorkingDirectory) / (spec.Name + ".chproject");
		}

		auto* app = new Application(spec);
		app->PushLayer(std::make_unique<RuntimeLayer>(projectPath.string()));
		return app;
	}
} // namespace Chained
