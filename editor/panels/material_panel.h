#ifndef CH_MATERIAL_PANEL_H
#define CH_MATERIAL_PANEL_H

#include "panel.h"
#include "engine/scene/entity.h"
#include "engine/scene/components/render/model_component.h"
#include "engine/graphics/api/renderer_types.h"

namespace Chained
{
	class MaterialPanel : public Panel
	{
	public:
		MaterialPanel();
		virtual void OnImGuiRender(bool readOnly = false) override;
		virtual void OnEvent(Event& e) override;
		virtual void SetContext(const std::shared_ptr<Scene>& context) override;

	private:
		void DrawMaterialSlot(Material& slot);
		void SaveMaterials();
		void DeleteMaterials();

	private:
		Entity m_SelectedEntity;
		std::vector<Material> m_Materials;
		std::string m_LoadedModelPath; ///< Model path whose materials are currently in m_Materials.
		int m_SelectedMeshIndex = -1;
		int m_SelectedMaterialIndex = 0;
		char m_FilterBuffer[128] = "";
	};
} // namespace Chained

#endif // CH_MATERIAL_PANEL_H
