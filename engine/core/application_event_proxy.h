#ifndef CH_APPLICATION_EVENT_PROXY_H
#define CH_APPLICATION_EVENT_PROXY_H

#include "engine/core/events/events.h"
#include "engine/core/window.h"
#include "engine/common/base.h"

namespace Chained
{
	// Lightweight decoupling shim so lower-level modules (engine_scene,
	// engine_scripting, engine_imgui) can fire application-level events and
	// access the main window without depending on engine_app / Application
	// directly, which would create a circular static-library link dependency.
	//
	// Application registers its OnEvent method and Window pointer here at
	// construction time. All other callers use ApplicationEventProxy::Dispatch()
	// and ApplicationEventProxy::GetWindow().
	class CH_API ApplicationEventProxy
	{
	public:
		static void Register(EventCallbackFn callback, Window* window = nullptr)
		{
			s_Callback = std::move(callback);
			s_Window = window;
		}

		static void Dispatch(Event& e)
		{
			if (s_Callback)
			{
				s_Callback(e);
			}
		}

		static Window* GetWindow()
		{
			return s_Window;
		}

	private:
		inline static EventCallbackFn s_Callback;
		inline static Window* s_Window = nullptr;
	};

} // namespace Chained

#endif // CH_APPLICATION_EVENT_PROXY_H
