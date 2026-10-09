#ifndef CH_SERVICE_REGISTRY_H
#define CH_SERVICE_REGISTRY_H

#include "engine/core/service.h"
#include "engine/common/engine_assert.h"
#include <shared_mutex>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <memory>

namespace Chained
{

	class ServiceRegistry
	{
	public:
		ServiceRegistry() = default;
		~ServiceRegistry() = default;

		ServiceRegistry(const ServiceRegistry&) = delete;
		ServiceRegistry& operator=(const ServiceRegistry&) = delete;
		ServiceRegistry(ServiceRegistry&&) = delete;
		ServiceRegistry& operator=(ServiceRegistry&&) = delete;

		template <typename T> void Provide(std::unique_ptr<T> service)
		{
			static_assert(std::is_class_v<T>, "ServiceRegistry: Provided type T must be a class or a struct!");
			static_assert(std::is_base_of_v<Service, T>, "ServiceRegistry: Provided type T must inherit from Service!");
			static_assert(std::has_virtual_destructor_v<Service>,
						  "ServiceRegistry: Service must have a virtual destructor!");

			std::unique_lock<std::shared_mutex> lock(m_Mutex);

			if (m_IsLocked)
			{
				CH_CORE_ERROR("ServiceRegistry: Provide<{}> rejected — registry is locked.", typeid(T).name());
				return;
			}

			if (m_Services.find(typeid(T)) != m_Services.end())
			{
				CH_CORE_ERROR("ServiceRegistry: Provide<{}> rejected — service already provided.", typeid(T).name());
				return;
			}

			std::shared_ptr<T> typedService = std::move(service);
			std::shared_ptr<Service> sharedService = std::static_pointer_cast<Service>(typedService);
			m_Services[typeid(T)] = sharedService;
			m_Order.push_back(sharedService);
		}

		template <typename T, typename Factory> void Provide(Factory&& factory)
		{
			Provide(std::unique_ptr<T>(factory()));
		}

		void Lock()
		{
			std::unique_lock<std::shared_mutex> lock(m_Mutex);
			m_IsLocked = true;
		}

		void InitializeModules()
		{
			std::vector<std::shared_ptr<Service>> modules;
			{
				std::shared_lock<std::shared_mutex> lock(m_Mutex);
				modules = m_Order;
			}

			for (auto& module : modules)
			{
				if (!module->IsEnabled())
				{
					continue;
				}

				try
				{
					module->Initialize();
				} catch (const std::exception& e)
				{
					CH_CORE_ERROR("ServiceRegistry: Module initialization failed with exception: {}", e.what());
					module->SetEnabled(false);
				} catch (...)
				{
					CH_CORE_ERROR("ServiceRegistry: Module initialization failed with an unknown exception.");
					module->SetEnabled(false);
				}
			}
		}

		void Shutdown()
		{
			std::vector<std::shared_ptr<Service>> modulesToShutdown;
			std::unordered_map<std::type_index, std::shared_ptr<Service>> servicesToClear;
			{
				std::unique_lock<std::shared_mutex> lock(m_Mutex);
				m_IsLocked = false;
				m_IsShuttingDown = true;
				modulesToShutdown = std::move(m_Order);
				servicesToClear = std::move(m_Services);
			}

			for (auto it = modulesToShutdown.rbegin(); it != modulesToShutdown.rend(); ++it)
			{
				if ((*it)->IsEnabled())
				{
					(*it)->Shutdown();
				}
				(*it)->SetEnabled(false);
			}

			// Destroy services outside the mutex lock so destructors (like GLTexture, AssetManager)
			// can query ServiceLocator without recursive deadlock on m_Mutex.
			modulesToShutdown.clear();
			servicesToClear.clear();

			{
				std::unique_lock<std::shared_mutex> lock(m_Mutex);
				m_IsShuttingDown = false;
			}
		}

		bool IsAvailable() const
		{
			std::shared_lock<std::shared_mutex> lock(m_Mutex);
			return !m_Services.empty();
		}

		template <typename T> T* Get()
		{
			static_assert(std::is_base_of_v<Service, T>,
						  "ServiceRegistry: Requested type T must inherit from Service!");

			std::shared_lock<std::shared_mutex> lock(m_Mutex);
			auto it = m_Services.find(typeid(T));

			if (it != m_Services.end())
			{
				T* svc = std::static_pointer_cast<T>(it->second).get();
				if (!svc->IsEnabled())
				{
					if (m_IsShuttingDown)
					{
						CH_CORE_ERROR("ServiceRegistry: Get<{}> during shutdown — service already shut down.",
									  typeid(T).name());
					}
					else
					{
						CH_CORE_ASSERT(false,
									   "ServiceRegistry: Requested service is disabled (initialization failed?)!");
						CH_CORE_ERROR("ServiceRegistry: Get<{}> returning nullptr — service is disabled.",
									  typeid(T).name());
					}
					return nullptr;
				}
				return svc;
			}

			CH_CORE_ASSERT(false, "ServiceRegistry: Requested service not found!");
			return nullptr;
		}

		template <typename T> T* TryGet()
		{
			static_assert(std::is_base_of_v<Service, T>,
						  "ServiceRegistry: Requested type T must inherit from Service!");

			std::shared_lock<std::shared_mutex> lock(m_Mutex);
			auto it = m_Services.find(typeid(T));

			if (it != m_Services.end())
			{
				T* svc = std::static_pointer_cast<T>(it->second).get();
				if (svc && svc->IsEnabled())
				{
					return svc;
				}
			}
			return nullptr;
		}

		template <typename T> bool Has()
		{
			static_assert(std::is_base_of_v<Service, T>, "ServiceRegistry: Checked type T must inherit from Service!");

			std::shared_lock<std::shared_mutex> lock(m_Mutex);
			return m_Services.find(typeid(T)) != m_Services.end();
		}

	private:
		mutable std::shared_mutex m_Mutex;
		std::unordered_map<std::type_index, std::shared_ptr<Service>> m_Services;
		std::vector<std::shared_ptr<Service>> m_Order;
		bool m_IsLocked = false;
		bool m_IsShuttingDown = false;
	};

} // namespace Chained

#endif // CH_SERVICE_REGISTRY_H