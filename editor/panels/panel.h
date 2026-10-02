#ifndef CH_PANEL_H
#define CH_PANEL_H

#include "engine/common/base.h"
#include "engine/core/events/events.h"
#include "engine/common/timestep.h"
#include "engine/scene/scene.h"
#include <memory>
#include <string>
#include <vector>

namespace Chained
{
	// Base class for dockable editor panels with optional scene context.
	class Panel
	{
	public:
		Panel() = default;
		virtual ~Panel() = default;

		virtual void OnImGuiRender(bool readOnly = false)
		{
		}
		virtual void OnUpdate(Timestep ts)
		{
		}
		virtual void OnEvent(Event& e)
		{
		}

		virtual void SetContext(const std::shared_ptr<Scene>& context)
		{
			m_Context = context;
		}

		bool& IsOpen()
		{
			return m_IsOpen;
		}
		const std::string& GetName() const
		{
			return m_Name;
		}

		bool IsPendingKill() const
		{
			return m_PendingKill;
		}
		void MarkForDelete()
		{
			m_PendingKill = true;
		}

	protected:
		std::string m_Name;
		std::shared_ptr<Scene> m_Context;
		bool m_IsOpen = true;
		bool m_PendingKill = false;
	};
} // namespace Chained

#endif // CH_PANEL_H
