#ifndef CH_SERVICE_LOCATOR_H
#define CH_SERVICE_LOCATOR_H

#include "engine/core/service.h"
#include "engine/core/service_registry.h"
#include "engine/common/engine_assert.h"
#include "engine/common/base.h"
#include <filesystem>
#include <memory>

namespace Chained
{
	class CH_API ServiceLocator
	{
	public:
		// Registers a service by its type. Takes ownership of the service.
		// T must inherit from Service.
		// On rejection (locked locator, duplicate registration) the service is destroyed
		// by the unique_ptr — no leak, unlike the old raw-pointer overload.
		template <typename T> static void Provide(std::unique_ptr<T> service);
		template <typename T, typename Factory> static void Provide(Factory&& factory);

		static void Lock();
		static void InitializeModule();
		static void Shutdown();
		// Clears the registry and re-arms the locator after Shutdown().
		// Test/reset helper only — production teardown should use Shutdown(),
		// which is terminal.
		static void Reset();
		static bool IsAvailable();

		template <typename T> static T* Get();
		template <typename T> static T* TryGet();
		template <typename T> static bool Has();

	private:
		static class ServiceRegistry& GetRegistry();
		inline static bool s_IsShutDown;
	};

	template <typename T> void ServiceLocator::Provide(std::unique_ptr<T> service)
	{
		if (s_IsShutDown)
		{
			return;
		}
		GetRegistry().Provide(std::move(service));
	}

	template <typename T, typename Factory> void ServiceLocator::Provide(Factory&& factory)
	{
		if (s_IsShutDown)
		{
			return;
		}
		GetRegistry().Provide<T>(factory());
	}

	template <typename T> T* ServiceLocator::Get()
	{
		if (s_IsShutDown)
		{
			return nullptr;
		}
		return GetRegistry().Get<T>();
	}

	template <typename T> T* ServiceLocator::TryGet()
	{
		if (s_IsShutDown)
		{
			return nullptr;
		}
		return GetRegistry().TryGet<T>();
	}

	template <typename T> bool ServiceLocator::Has()
	{
		if (s_IsShutDown)
		{
			return false;
		}
		return GetRegistry().Has<T>();
	}

} // namespace Chained

#endif // CH_SERVICE_LOCATOR_H