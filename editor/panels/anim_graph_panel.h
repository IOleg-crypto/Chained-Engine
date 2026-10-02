#ifndef CH_ANIM_GRAPH_PANEL_H
#define CH_ANIM_GRAPH_PANEL_H

#include "engine/scene/entity.h"
#include "engine/assets/types/animation_graph_asset.h"
#include <GraphEditor.h>
#include <vector>
#include <string>
#include <unordered_map>

#include "editor/panels/panel.h"
#include <imgui.h>

namespace Chained
{

	class AnimGraphPanel : public Panel
	{
	public:
		AnimGraphPanel();
		~AnimGraphPanel() override;

		void OnImGuiRender(bool readOnly = false) override;

	private:
		bool m_ChangedGraph = false;

		enum class VarType
		{
			Float,
			Bool
		};
		std::unordered_map<std::string, VarType> m_VariableTypes; // tracks type per variable name
		bool m_AddVarAsFloat = true;							  // which type to add next

		GraphEditor::ViewState m_ViewState;
		GraphEditor::Options m_Options;
		GraphEditor::FitOnScreen m_Fit = GraphEditor::Fit_None;

		struct Delegate : public GraphEditor::Delegate
		{
			AnimGraphPanel* panel = nullptr;
			AnimationGraphAsset* graph = nullptr;
			bool* changedFlag = nullptr;
			std::vector<bool> nodeSelected;

			void SyncSelection();

			bool AllowedLink(GraphEditor::NodeIndex from, GraphEditor::NodeIndex to) override;
			void SelectNode(GraphEditor::NodeIndex nodeIndex, bool selected) override;
			void MoveSelectedNodes(const ImVec2 delta) override;
			void AddLink(GraphEditor::NodeIndex inputNodeIndex, GraphEditor::SlotIndex inputSlotIndex,
						 GraphEditor::NodeIndex outputNodeIndex, GraphEditor::SlotIndex outputSlotIndex) override;
			void DelLink(GraphEditor::LinkIndex linkIndex) override;
			void RightClick(GraphEditor::NodeIndex nodeIndex, GraphEditor::SlotIndex slotIndexInput,
							GraphEditor::SlotIndex slotIndexOutput) override;
			void CustomDraw(ImDrawList* drawList, ImRect rectangle, GraphEditor::NodeIndex nodeIndex) override;

			const size_t GetTemplateCount() override;
			const GraphEditor::Template GetTemplate(GraphEditor::TemplateIndex index) override;
			const size_t GetNodeCount() override;
			const GraphEditor::Node GetNode(GraphEditor::NodeIndex index) override;
			const size_t GetLinkCount() override;
			const GraphEditor::Link GetLink(GraphEditor::LinkIndex index) override;
		};

		Delegate m_Delegate;

		// Preview state: temporarily override AnimationComponent when node is selected
		int m_PreviewNodeIdx = -1;
		int m_OrigAnimIndex = 0;
		int m_OrigFrame = 0;
		int m_OrigStartFrame = 0;
		int m_OrigEndFrame = -1;
		float m_OrigSpeed = 1.0f;
		bool m_OrigIsLooping = true;
		bool m_OrigIsPlaying = true;
		Entity m_PreviewEntity;

		void RestorePreview();
		void ApplyPreview(AnimationGraphAsset* graph, int nodeIdx, Entity entity);
		void DrawProperties(AnimationGraphAsset* graph, Entity entity);
		void SaveGraph(AnimationGraphAsset* graph, const std::string& path);
	};

} // namespace Chained
#endif
