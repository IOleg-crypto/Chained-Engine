#ifndef CH_EDITOR_PANELS_H
#define CH_EDITOR_PANELS_H

#include "engine/common/timestep.h"
#include "panels/panel.h"
#include <memory>
#include <string>
#include <vector>

namespace Chained
{

	class EditorPanels
	{
	public:
		EditorPanels() = default;
		~EditorPanels() = default;

		void Init();

		template <typename T, typename... Args> std::shared_ptr<T> Register(Args&&... args)
		{
			auto panel = std::make_shared<T>(std::forward<Args>(args)...);
			m_Panels.push_back(panel);
			return panel;
		}

		template <typename T> std::shared_ptr<T> Get()
		{
			for (auto& panel : m_Panels)
			{
				if (auto cast = std::dynamic_pointer_cast<T>(panel))
				{
					return cast;
				}
			}
			return nullptr;
		}

		std::shared_ptr<Panel> Get(const std::string& name)
		{
			for (auto& panel : m_Panels)
			{
				if (panel->GetName() == name)
				{
					return panel;
				}
			}
			return nullptr;
		}

		template <typename F> void ForEach(F&& func)
		{
			for (auto& panel : m_Panels)
			{
				func(panel);
			}
		}

		void OnUpdate(Timestep ts);
		void OnImGuiRender(bool readOnly);
		void OnEvent(Event& e);
		void SetContext(const std::shared_ptr<Scene>& context);

		std::vector<std::shared_ptr<Panel>>& GetPanels()
		{
			return m_Panels;
		}

	private:
		std::vector<std::shared_ptr<Panel>> m_Panels;
	};

} // namespace Chained

#endif // CH_EDITOR_PANELS_H
