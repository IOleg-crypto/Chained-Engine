#include "inspector_panel.h"
#include "thirdparty/IconsFontAwesome6.h"
#include "gui.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/types/model_asset.h"
#include "engine/scene/components.h"
#include "engine/project/project.h"
#include "engine/scene/scene_events.h"

#include "imgui.h"
#include "property_editor.h"

namespace Chained
{
	InspectorPanel::InspectorPanel()
	{
		m_Name = "Inspector";
	}

	void InspectorPanel::OnImGuiRender(bool readOnly)
	{
		if (!m_IsOpen)
		{
			return;
		}

		ImGui::Begin(m_Name.c_str(), &m_IsOpen);

		if (m_SelectedEntity && (!m_Context || m_SelectedEntity.GetRegistryPtr() != m_Context->GetRegistryPtr() ||
								 !m_SelectedEntity.IsValid()))
		{
			m_SelectedEntity = {};
		}

		if (m_SelectedEntity)
		{

			bool isTransitioning = EditorLayer::Get().GetSceneManager().IsTransitioning();
			DrawComponents(m_SelectedEntity, readOnly || isTransitioning);
		}
		else
		{
			ImGui::Text("Selection: None");
			ImGui::TextDisabled("Select an entity in the Hierarchy to view its components.");
		}
		ImGui::End();
	}

	void InspectorPanel::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<EntitySelectedEvent>([this](EntitySelectedEvent& ev) {
			if (m_Context && ev.GetEntity() != entt::null)
			{
				m_SelectedEntity = Entity(ev.GetEntity(), m_Context->GetRegistryPtr());
			}
			else
			{
				m_SelectedEntity = {};
			}
			m_SelectedMeshIndex = ev.GetMeshIndex();
			return false;
		});
	}

	void InspectorPanel::SetContext(const std::shared_ptr<Scene>& context)
	{
		if (m_Context.get() != context.get())
		{
			m_SelectedEntity = {};
		}
		Panel::SetContext(context);
	}

	void InspectorPanel::DrawComponents(Entity entity, bool readOnly)
	{
		ImGui::PushID((uint32_t)entity);

		if (entity.HasComponent<IDComponent>())
		{
			uint64_t uuid = (uint64_t)entity.GetComponent<IDComponent>().ID;
			ImGui::TextDisabled("UUID: %llu", uuid);
		}

		PropertyEditor::DrawEntityHeader(entity);

		// Delegate all component drawing logic to PropertyEditor registry
		PropertyEditor::DrawEntityProperties(entity);

		ImGui::PopID();
	}
	void InspectorPanel::SetSelectedMeshIndex(int index)
	{
		m_SelectedMeshIndex = index;
	}
} // namespace Chained
