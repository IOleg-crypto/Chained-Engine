#ifndef CH_PROPERTY_EDITOR_H
#define CH_PROPERTY_EDITOR_H

#include <functional>
#include <string>

#include "engine/scene/component_registry.h"
#include "engine/scene/entity.h"

namespace Chained
{
	namespace PropertyEditor
	{
		void Init();

		// Registry API
		void DrawEntityProperties(Entity entity);
		void DrawAddComponentPopup(Entity entity);

		// Automation: Register using Reflection
		template <typename T> void Register(const std::string& name, const char* icon = nullptr);

		// Custom Drawer Registration
		template <typename T, typename F>
		void RegisterCustom(const std::string& name, F&& drawer, const char* icon = nullptr);

		void DrawEntityHeader(Entity entity);

		// Shared registration logic
		template <typename T>
		void RegisterComponentImpl(const std::string& name, const char* icon, std::function<void(Entity)> drawUI);

		// Internal template helpers (Implementations moved to .cpp or a separate _impl.h if needed elsewhere)
		template <typename T> void DrawComponentReflection(const std::string& name, const char* icon, Entity entity);
		void DrawGenericReflection(const ComponentMetadata& metadata, Entity entity);

		template <typename T, typename F>
		void DrawComponentContainer(const std::string& name, const char* icon, Entity entity, F&& drawer);

		// Final non-template drawing core
		void DrawComponentInternal(::entt::id_type typeId, const std::string& name, const char* icon, Entity entity,
								   std::function<bool()> contentDrawer, std::function<void()> remover);
	} // namespace PropertyEditor

} // namespace Chained

#endif // CH_PROPERTY_EDITOR_H
