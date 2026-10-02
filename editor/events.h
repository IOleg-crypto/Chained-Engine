#ifndef CH_EDITOR_EVENTS_H
#define CH_EDITOR_EVENTS_H

#include "engine/core/events/events.h"
#include "engine/scene/entity.h"

namespace Chained
{

	// Forward declarations to avoid heavy includes in header.
	class Scene;

	void SelectEntity(Entity entity, Scene* scene);
	void DeselectEntity(Scene* scene);

	// Event to trigger an layout reset.
	class AppResetLayoutEvent : public Event
	{
	public:
		AppResetLayoutEvent() = default;
		EVENT_CLASS_TYPE(AppResetLayout)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	};

	// Event to trigger launching the game in runtime mode.
	class AppLaunchRuntimeEvent : public Event
	{
	public:
		AppLaunchRuntimeEvent() = default;
		EVENT_CLASS_TYPE(AppLaunchRuntime)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	};

	// Event to signal focusing on a specific entity in the viewport
	class ViewportFocusEntityEvent : public Event
	{
	public:
		ViewportFocusEntityEvent(Entity entity)
			: m_Entity(entity)
		{
		}
		Entity GetEntity() const
		{
			return m_Entity;
		}

		EVENT_CLASS_TYPE(ViewportFocusEntity)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	private:
		Entity m_Entity;
	};

} // namespace Chained

#endif // CH_EDITOR_EVENTS_H
