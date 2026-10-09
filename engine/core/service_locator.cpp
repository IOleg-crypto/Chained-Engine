#include "engine/core/service_locator.h"
#include "engine/core/service_registry.h"

namespace Chained
{
	ServiceRegistry& ServiceLocator::GetRegistry()
	{
		static ServiceRegistry registry;
		return registry;
	}

	CH_API void ServiceLocator::Lock()
	{
		if (s_IsShutDown)
		{
			return;
		}
		GetRegistry().Lock();
	}

	CH_API void ServiceLocator::InitializeModule()
	{
		if (s_IsShutDown)
		{
			return;
		}
		GetRegistry().InitializeModules();
	}

	CH_API void ServiceLocator::Shutdown()
	{
		if (s_IsShutDown)
		{
			return;
		}
		GetRegistry().Shutdown();
		s_IsShutDown = true;
	}

	CH_API bool ServiceLocator::IsAvailable()
	{
		if (s_IsShutDown)
		{
			return false;
		}
		return GetRegistry().IsAvailable();
	}

} // namespace Chained