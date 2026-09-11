#ifndef CH_ENTRY_POINT_H
#define CH_ENTRY_POINT_H

#include "engine/app/application.h"

extern Chained::Application* Chained::CreateApplication(Chained::ApplicationCommandLineArgs args);

#if CH_PLATFORM_WINDOWS
extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement = 1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

#include <cstdlib>

int main(int argc, char** argv)
{
#if CH_PLATFORM_LINUX
	// Request discrete GPU offload on Linux (Mesa / AMD / NVIDIA PRIME)
	setenv("DRI_PRIME", "1", 0);
	setenv("__NV_PRIME_RENDER_OFFLOAD", "1", 0);
	setenv("__GLX_VENDOR_LIBRARY_NAME", "nvidia", 0);
	setenv("__VK_LAYER_NV_optimus", "NVIDIA_only", 0);
#endif

	Chained::ApplicationCommandLineArgs args;
	args.Count = argc;
	args.Args = argv;

	auto app = Chained::CreateApplication(args);
	app->Run();
	delete app;
	return 0;
}

#endif // CH_ENTRY_POINT_H
