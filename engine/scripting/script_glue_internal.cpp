#include "script_glue_internal.h"
#include "engine/core/service_locator.h"
#include "scriptengine.h"

Chained::Scene* Chained::GetActiveScene()
{
	auto* scriptEngine = ServiceLocator::TryGet<ScriptEngine>();
	if (!scriptEngine)
	{
		return nullptr;
	}
	return scriptEngine->GetContextScene();
}

Chained::Entity Chained::GetEntity(uint64_t entityID)
{
	if (entityID == static_cast<uint64_t>(entt::null) || entityID == static_cast<uint32_t>(entt::null) ||
		entityID == ~0ull)
	{
		return {};
	}
	Scene* scene = GetActiveScene();
	if (!scene)
	{
		return {};
	}
	const auto handle = static_cast<entt::entity>(static_cast<uint32_t>(entityID));
	if (!scene->GetRegistry().valid(handle))
	{
		return {};
	}
	return Entity(handle, &scene->GetRegistry());
}
