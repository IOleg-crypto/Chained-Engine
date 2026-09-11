
#include "scene_scripting_manager.h"
#include "engine/core/profiler.h"
#include "engine/core/service_locator.h"
#include "engine/physics/physics.h"
#include "engine/scene/scene.h"
#include "engine/scripting/scriptengine.h"
#include "engine/scripting/scriptengine_services.h"
#include "engine/scripting/script_glue_internal.h"
#include "engine/scripting/script_interop_pointers.h"
#include <Coral/String.hpp>
#include <Coral/Type.hpp>

namespace Chained
{
	static std::vector<SceneScriptingManager*> s_Managers;
	static std::mutex s_ManagersMutex;

	void SceneScriptingManager::Register(SceneScriptingManager* manager)
	{
		std::lock_guard<std::mutex> lock(s_ManagersMutex);
		s_Managers.push_back(manager);
	}

	void SceneScriptingManager::Unregister(SceneScriptingManager* manager)
	{
		std::lock_guard<std::mutex> lock(s_ManagersMutex);
		auto it = std::find(s_Managers.begin(), s_Managers.end(), manager);
		if (it != s_Managers.end())
		{
			s_Managers.erase(it);
		}
	}

	void SceneScriptingManager::ResetAll()
	{
		// Snapshot under lock, then iterate without holding it.
		// OnRuntimeStop() may call Unregister() which also takes s_ManagersMutex —
		// iterating while holding the lock would cause a deadlock.
		std::vector<SceneScriptingManager*> snapshot;
		{
			std::lock_guard<std::mutex> lock(s_ManagersMutex);
			snapshot = s_Managers;
		}
		for (auto* manager : snapshot)
		{
			manager->OnRuntimeStop();
		}
	}

	SceneScriptingManager::SceneScriptingManager(Scene* scene)
		: m_Scene(scene)
	{
		SceneScriptingManager::Register(this);
	}

	SceneScriptingManager::ScriptEngineContext SceneScriptingManager::AcquireScriptEngine()
	{
		ScriptEngineContext ctx;
		ctx.engine = ServiceLocator::TryGet<ScriptEngine>();
		if (!ctx.engine || !ctx.engine->GetHost().IsInitialized())
		{
			return {};
		}
		auto* coreAssembly = ctx.engine->GetHost().GetCoreAssembly();
		if (!coreAssembly)
		{
			return {};
		}
		ctx.scriptEngineType = coreAssembly->GetLocalType("Chained.ScriptEngine");
		return ctx;
	}

	SceneScriptingManager::~SceneScriptingManager()
	{
		SceneScriptingManager::Unregister(this);

		if (m_Scene && ServiceLocator::IsAvailable())
		{
			if (m_Scene->IsSimulationRunning())
			{
				auto ctx = AcquireScriptEngine();
				if (ctx.engine && ctx.scriptEngineType)
				{
					if (g_ScriptClearAll)
					{
						g_ScriptClearAll();
					}
				}
			}
		}
	}

	void SceneScriptingManager::OnRuntimeStart()
	{
		CH_CORE_INFO("SceneScriptingManager::OnRuntimeStart - C# ScriptEngine based (Delegated)");
		if (!m_Scene)
		{
			return;
		}

		auto& registry = m_Scene->GetRegistry();
		auto view = registry.view<ManagedScriptComponent>();
		for (auto entity : view)
		{
			auto& msc = registry.get<Chained::ManagedScriptComponent>(entity);
			for (auto& script : msc.Scripts)
			{
				script.IsInstantiated = false;
			}
		}

		entt::registry* registryPtr = m_Scene->GetRegistryPtr();

		// Hook entity destruction so C# scripts are torn down (DestroyScript),
		// preventing leaked script instances / stale entity handles.
		auto& reg = m_Scene->GetRegistry();
		reg.on_destroy<ManagedScriptComponent>().disconnect<&SceneScriptingManager::OnManagedScriptDestroyed>(this);
		reg.on_destroy<ManagedScriptComponent>().connect<&SceneScriptingManager::OnManagedScriptDestroyed>(this);
	}

	void SceneScriptingManager::OnRuntimeStop()
	{
		if (m_Scene)
		{
			m_Scene->GetRegistry()
				.on_destroy<ManagedScriptComponent>()
				.disconnect<&SceneScriptingManager::OnManagedScriptDestroyed>(this);
		}

		auto ctx = AcquireScriptEngine();
		if (ctx.engine && ctx.scriptEngineType && !m_ReloadInProgress)
		{
			if (g_ScriptClearAll)
			{
				g_ScriptClearAll();
			}
		}

		auto& registry = m_Scene->GetRegistry();
		auto view = registry.view<ManagedScriptComponent>();
		for (auto entity : view)
		{
			auto& msc = registry.get<Chained::ManagedScriptComponent>(entity);
			for (auto& script : msc.Scripts)
			{
				script.IsInstantiated = false;
			}
		}
	}

	void SceneScriptingManager::OnUpdate(Timestep deltaTime)
	{
		CH_PROFILE_FUNCTION();

		if (!m_Scene || !m_Scene->IsSimulationRunning())
		{
			return;
		}

		auto ctx = AcquireScriptEngine();
		if (!ctx.engine || !ctx.scriptEngineType)
		{
			return;
		}

		ctx.engine->SetContextScene(m_Scene);

		auto& registry = m_Scene->GetRegistry();
		auto view = registry.view<ManagedScriptComponent>();
		for (auto entity : view)
		{
			auto& msc = registry.get<Chained::ManagedScriptComponent>(entity);
			for (auto& script : msc.Scripts)
			{
				if (!script.HasInstance() && !script.ClassName.empty() && !script.InstantiateTried)
				{
					try
					{
						CH_CORE_TRACE("C++ calling InstantiateScript for {}", script.ClassName);
						std::u16string classNameStr = ch_utf8_to_u16(script.ClassName);
						if (g_ScriptInstantiate)
						{
							uint8_t ok = g_ScriptInstantiate(static_cast<uint64_t>(entity), classNameStr.c_str());
							script.IsInstantiated = ok != 0;
							script.InstantiateTried = true;

							if (ok == 0)
							{
								continue;
							}
						}
						else
						{
							script.InstantiateTried = true;
							continue;
						}

						for (const auto& [fieldName, field] : script.Fields)
						{
							CH_CORE_TRACE("C++ setting field {}", fieldName);
							Coral::String fNameStr = Coral::String::New(fieldName);
							Coral::String cNameStr = Coral::String::New(script.ClassName);
							if (field.Type == ScriptFieldType::Float)
							{
								if (auto* val = std::get_if<float>(&field.Value))
								{
									ctx.scriptEngineType.InvokeStaticMethod(
										"SetFieldFloat", static_cast<uint64_t>(entity), cNameStr, fNameStr, *val);
								}
							}
							else if (field.Type == ScriptFieldType::Int)
							{
								if (auto* val = std::get_if<int>(&field.Value))
								{
									ctx.scriptEngineType.InvokeStaticMethod(
										"SetFieldInt", static_cast<uint64_t>(entity), cNameStr, fNameStr, *val);
								}
							}
							else if (field.Type == ScriptFieldType::Bool)
							{
								if (auto* val = std::get_if<bool>(&field.Value))
								{
									ctx.scriptEngineType.InvokeStaticMethod(
										"SetFieldBool", static_cast<uint64_t>(entity), cNameStr, fNameStr, *val);
								}
							}
							else if (field.Type == ScriptFieldType::String)
							{
								if (auto* val = std::get_if<std::string>(&field.Value))
								{
									Coral::String vStr = Coral::String::New(*val);
									ctx.scriptEngineType.InvokeStaticMethod(
										"SetFieldString", static_cast<uint64_t>(entity), cNameStr, fNameStr, vStr);
									Coral::String::Free(vStr);
								}
							}
							Coral::String::Free(cNameStr);
							Coral::String::Free(fNameStr);
						}
					} catch (const std::exception& e)
					{
						CH_CORE_ERROR("ScriptEngine: Exception instantiating '{}': {}", script.ClassName, e.what());
					}
				}
			}
		}

		try
		{
			if (g_ScriptOnUpdate)
			{
				g_ScriptOnUpdate((float)deltaTime);
			}
		} catch (const std::exception& e)
		{
			CH_CORE_ERROR("ScriptEngine: Exception in OnUpdate Loop: {}", e.what());
		}
	}

	void SceneScriptingManager::OnEvent(Event& e)
	{
		if (m_ReloadInProgress || !m_Scene || e.GetEventType() == EventType::None)
		{
			return;
		}

		auto ctx = AcquireScriptEngine();
		if (!ctx.engine)
		{
			return;
		}

		ctx.engine->SetContextScene(m_Scene);

		if (ctx.scriptEngineType)
		{
			if (g_ScriptOnEvent)
			{
				g_ScriptOnEvent((int)e.GetEventType());
			}
		}

		ctx.engine->SetContextScene(nullptr);
	}

	void SceneScriptingManager::OnRenderUI()
	{
		if (m_ReloadInProgress)
		{
			return;
		}

		auto ctx = AcquireScriptEngine();
		if (!ctx.engine)
		{
			return;
		}

		ctx.engine->SetContextScene(m_Scene);

		if (ctx.scriptEngineType)
		{
			if (g_ScriptOnRenderUI)
			{
				g_ScriptOnRenderUI();
			}
		}

		ctx.engine->SetContextScene(nullptr);
	}

	void SceneScriptingManager::OnManagedScriptDestroyed(entt::registry& registry, entt::entity entity)
	{
		if (!g_ScriptDestroy)
		{
			return;
		}

		// Entity is still valid inside the on_destroy callback; read its scripts.
		if (!registry.all_of<ManagedScriptComponent>(entity))
		{
			return;
		}
		auto& msc = registry.get<ManagedScriptComponent>(entity);
		for (auto& script : msc.Scripts)
		{
			if (script.HasInstance() && !script.ClassName.empty())
			{
				std::u16string classNameStr = ch_utf8_to_u16(script.ClassName);
				g_ScriptDestroy(static_cast<uint64_t>(entity), classNameStr.c_str());
			}
		}
	}

} // namespace Chained
