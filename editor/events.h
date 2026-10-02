#ifndef CH_EDITOR_EVENTS_H
#define CH_EDITOR_EVENTS_H

#include "engine/core/events/events.h"
#include "engine/scene/entity.h"
#include "engine/scene/scene_state.h"
#include <string>

namespace Chained
{
	class Scene;

	void SelectEntity(Entity entity, Scene* scene);
	void DeselectEntity(Scene* scene);

	// Event to trigger a layout reset.
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

	// Event to request opening a scene
	class EditorOpenSceneEvent : public Event
	{
	public:
		EditorOpenSceneEvent(const std::string& path = "")
			: m_Path(path)
		{
		}
		const std::string& GetPath() const
		{
			return m_Path;
		}

		EVENT_CLASS_TYPE(EditorOpenScene)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	private:
		std::string m_Path;
	};

	// Event to request saving the active scene
	class EditorSaveSceneEvent : public Event
	{
	public:
		EditorSaveSceneEvent(bool saveAs = false)
			: m_SaveAs(saveAs)
		{
		}
		bool IsSaveAs() const
		{
			return m_SaveAs;
		}

		EVENT_CLASS_TYPE(EditorSaveScene)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	private:
		bool m_SaveAs = false;
	};

	// Event to request changing scene play/edit/simulate state
	class EditorSceneStateChangeEvent : public Event
	{
	public:
		EditorSceneStateChangeEvent(SceneState state)
			: m_State(state)
		{
		}
		SceneState GetState() const
		{
			return m_State;
		}

		EVENT_CLASS_TYPE(EditorSceneStateChange)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	private:
		SceneState m_State;
	};

	// Event to request entity selection change
	class EditorSelectEntityEvent : public Event
	{
	public:
		EditorSelectEntityEvent(Entity entity)
			: m_Entity(entity)
		{
		}
		Entity GetEntity() const
		{
			return m_Entity;
		}

		EVENT_CLASS_TYPE(EditorSelectEntity)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	private:
		Entity m_Entity;
	};

	// Event to request font reload
	class EditorReloadFontsEvent : public Event
	{
	public:
		EditorReloadFontsEvent() = default;
		EVENT_CLASS_TYPE(EditorReloadFonts)
		EVENT_CLASS_CATEGORY(EventCategoryApplication)
	};

} // namespace Chained

#endif // CH_EDITOR_EVENTS_H
