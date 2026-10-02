#include <imgui.h>
#include <GraphEditor.h>
#include "engine/assets/types/animation_graph_asset.h"
#include "engine/scene/components/animation/animation_component.h"
#include "engine/scene/components/render/model_component.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/types/model_asset.h"
#include "engine/assets/loaders/anim_graph_loader.h"
#include "engine/core/service_locator.h"
#include "thirdparty/IconsFontAwesome6.h"
#include "editor/panels/anim_graph_panel.h"
#include "editor/layer.h"

namespace Chained
{

	static const char* s_InputNames[] = {"In"};
	static const char* s_OutputNames[] = {"Out"};

	static const GraphEditor::Template s_Templates[] = {
		// 0: Entry node — gold header, no inputs, 1 output
		{IM_COL32(255, 200, 0, 255), IM_COL32(60, 60, 40, 255), IM_COL32(80, 80, 50, 255), 0, nullptr, nullptr, 1,
		 s_OutputNames, nullptr},
		// 1: State node — blue header, 1 input, 1 output
		{IM_COL32(100, 150, 200, 255), IM_COL32(60, 80, 100, 255), IM_COL32(70, 90, 110, 255), 1, s_InputNames, nullptr,
		 1, s_OutputNames, nullptr}};

	// ── Delegate ──────────────────────────────────────────────────────

	void AnimGraphPanel::Delegate::SyncSelection()
	{
		if (!graph)
		{
			nodeSelected.clear();
			return;
		}
		nodeSelected.resize(graph->Nodes.size(), false);
	}

	bool AnimGraphPanel::Delegate::AllowedLink(GraphEditor::NodeIndex from, GraphEditor::NodeIndex to)
	{
		return from != to;
	}

	void AnimGraphPanel::Delegate::SelectNode(GraphEditor::NodeIndex nodeIndex, bool selected)
	{
		if (nodeIndex < nodeSelected.size())
		{
			nodeSelected[nodeIndex] = selected;
		}

		// Override/restore animation preview in main viewport
		if (panel)
		{
			Entity entity = EditorLayer::Get().GetSelectedEntity();
			if (selected)
			{
				panel->ApplyPreview(graph, (int)nodeIndex, entity);
			}
			else
			{
				panel->RestorePreview();
			}
		}
	}

	void AnimGraphPanel::Delegate::MoveSelectedNodes(const ImVec2 delta)
	{
		if (!graph)
		{
			return;
		}
		for (size_t i = 0; i < graph->Nodes.size(); i++)
		{
			if (i < nodeSelected.size() && nodeSelected[i])
			{
				graph->Nodes[i].EditorX += delta.x;
				graph->Nodes[i].EditorY += delta.y;
			}
		}
	}

	void AnimGraphPanel::Delegate::AddLink(GraphEditor::NodeIndex inputNodeIndex, GraphEditor::SlotIndex,
										   GraphEditor::NodeIndex outputNodeIndex, GraphEditor::SlotIndex)
	{
		if (!graph)
		{
			return;
		}
		if (inputNodeIndex >= graph->Nodes.size() || outputNodeIndex >= graph->Nodes.size())
		{
			return;
		}

		AnimTransition tr;
		tr.ID = graph->NextLinkID++;
		tr.SourceNodeID = graph->Nodes[outputNodeIndex].ID;
		tr.TargetNodeID = graph->Nodes[inputNodeIndex].ID;
		tr.BlendDuration = 0.2f;
		graph->Transitions.push_back(tr);
		if (changedFlag)
		{
			*changedFlag = true;
		}
	}

	void AnimGraphPanel::Delegate::DelLink(GraphEditor::LinkIndex linkIndex)
	{
		if (!graph || linkIndex >= graph->Transitions.size())
		{
			return;
		}
		graph->Transitions.erase(graph->Transitions.begin() + linkIndex);
		if (changedFlag)
		{
			*changedFlag = true;
		}
	}

	void AnimGraphPanel::Delegate::RightClick(GraphEditor::NodeIndex, GraphEditor::SlotIndex, GraphEditor::SlotIndex)
	{
		// Because Delegate is abstract class , but don`t implement any right now.
	}

	void AnimGraphPanel::Delegate::CustomDraw(ImDrawList* drawList, ImRect rectangle, GraphEditor::NodeIndex nodeIndex)
	{
		if (!graph || nodeIndex >= graph->Nodes.size())
		{
			return;
		}

		auto& node = graph->Nodes[nodeIndex];
		ImVec2 textPos = rectangle.Min + ImVec2(8, 28);

		if (node.ID == graph->EntryNodeID)
		{
			drawList->AddText(textPos, IM_COL32(255, 220, 80, 255), "[Entry]");
			textPos.y += 16;
		}
		else
		{
			char buf[64];
			snprintf(buf, sizeof(buf), "Anim: %d", node.AnimationIndex);
			drawList->AddText(textPos, IM_COL32(200, 200, 200, 200), buf);
			textPos.y += 16;
		}

		// Frame range
		char rangeBuf[64];
		if (node.EndFrame < 0)
		{
			snprintf(rangeBuf, sizeof(rangeBuf), "Frames: %d-end", node.StartFrame);
		}
		else
		{
			snprintf(rangeBuf, sizeof(rangeBuf), "Frames: %d-%d", node.StartFrame, node.EndFrame);
		}
		drawList->AddText(textPos, IM_COL32(160, 160, 160, 200), rangeBuf);
		textPos.y += 14;

		// Speed
		if (node.Speed != 1.0f)
		{
			char spdBuf[32];
			snprintf(spdBuf, sizeof(spdBuf), "Speed: %.1fx", node.Speed);
			drawList->AddText(textPos, IM_COL32(180, 220, 180, 200), spdBuf);
		}
	}

	const size_t AnimGraphPanel::Delegate::GetTemplateCount()
	{
		return 2;
	}

	const GraphEditor::Template AnimGraphPanel::Delegate::GetTemplate(GraphEditor::TemplateIndex index)
	{
		if (index < 2)
		{
			return s_Templates[index];
		}
		return s_Templates[1];
	}

	const size_t AnimGraphPanel::Delegate::GetNodeCount()
	{
		return graph ? graph->Nodes.size() : 0;
	}

	const GraphEditor::Node AnimGraphPanel::Delegate::GetNode(GraphEditor::NodeIndex index)
	{
		if (!graph || index >= graph->Nodes.size())
		{
			return {};
		}

		auto& node = graph->Nodes[index];
		GraphEditor::TemplateIndex tpl = (node.ID == graph->EntryNodeID) ? 0 : 1;
		bool sel = (index < nodeSelected.size()) ? nodeSelected[index] : false;

		return {node.Name.c_str(), tpl,
				ImRect(ImVec2(node.EditorX, node.EditorY), ImVec2(node.EditorX + 180, node.EditorY + 80)), sel};
	}

	const size_t AnimGraphPanel::Delegate::GetLinkCount()
	{
		return graph ? graph->Transitions.size() : 0;
	}

	const GraphEditor::Link AnimGraphPanel::Delegate::GetLink(GraphEditor::LinkIndex index)
	{
		if (!graph || index >= graph->Transitions.size())
		{
			return {};
		}

		auto& tr = graph->Transitions[index];

		auto findIndex = [&](int nodeID) -> GraphEditor::NodeIndex {
			for (size_t i = 0; i < graph->Nodes.size(); i++)
			{
				if (graph->Nodes[i].ID == nodeID)
				{
					return i;
				}
			}
			return 0;
		};

		return {findIndex(tr.TargetNodeID), 0, findIndex(tr.SourceNodeID), 0};
	}

	// ── Panel ─────────────────────────────────────────────────────────

	AnimGraphPanel::AnimGraphPanel()
	{
		m_Name = "Animation Graph";
		m_Delegate.panel = this;
	}

	AnimGraphPanel::~AnimGraphPanel()
	{
		RestorePreview();
	}

	void AnimGraphPanel::OnImGuiRender(bool readOnly)
	{
		if (!m_IsOpen)
		{
			return;
		}

		ImGui::Begin("Animation Graph", &m_IsOpen);

		Entity selectedEntity = EditorLayer::Get().GetSelectedEntity();

		// Restore preview if entity changed
		if (m_PreviewNodeIdx >= 0 && m_PreviewEntity != selectedEntity)
		{
			RestorePreview();
		}

		if (!selectedEntity || !selectedEntity.HasComponent<AnimationComponent>())
		{
			ImGui::Text("Select an entity with an AnimationComponent.");
			ImGui::End();
			return;
		}

		if (!selectedEntity.HasComponent<ModelComponent>())
		{
			ImGui::Text("Entity must have a ModelComponent.");
			ImGui::End();
			return;
		}

		auto& animComp = selectedEntity.GetComponent<AnimationComponent>();

		AnimationGraphAsset* graph = nullptr;
		if (!animComp.GraphPath.empty())
		{
			auto* assets = ServiceLocator::TryGet<AssetManager>();
			if (assets)
			{
				auto asset = assets->Get<AnimationGraphAsset>(animComp.GraphPath);
				if (asset)
				{
					graph = asset.get();
				}
			}
		}

		if (!graph)
		{
			RestorePreview();
			ImGui::TextDisabled("No animation graph loaded.");

			// Generate unique name for entity
			std::string baseName = "anim_graph";
			if (selectedEntity.HasComponent<TagComponent>())
			{
				std::string tag = selectedEntity.GetComponent<TagComponent>().Tag;
				if (!tag.empty())
				{
					std::string cleanTag = tag;
					std::replace(cleanTag.begin(), cleanTag.end(), ' ', '_');
					std::replace(cleanTag.begin(), cleanTag.end(), '/', '_');
					std::replace(cleanTag.begin(), cleanTag.end(), '\\', '_');
					std::replace(cleanTag.begin(), cleanTag.end(), '#', '_');
					std::transform(cleanTag.begin(), cleanTag.end(), cleanTag.begin(), ::tolower);
					baseName = cleanTag + "_graph";
				}
			}
			else if (selectedEntity.HasComponent<ModelComponent>())
			{
				std::string mPath = selectedEntity.GetComponent<ModelComponent>().ModelPath;
				if (!mPath.empty())
				{
					baseName = std::filesystem::path(mPath).stem().string() + "_graph";
				}
			}

			auto* assets = ServiceLocator::TryGet<AssetManager>();
			std::string uniqueGraphPath = "animations/" + baseName + ".chag";
			if (assets)
			{
				int counter = 1;
				while (std::filesystem::exists(assets->ResolvePath(uniqueGraphPath)))
				{
					uniqueGraphPath = "animations/" + baseName + "_" + std::to_string(counter++) + ".chag";
				}
			}

			if (ImGui::Button("Create New Graph"))
			{
				animComp.GraphPath = uniqueGraphPath;
				animComp.GraphAssetHandle = 0;
				m_ChangedGraph = true;

				AnimationGraphAsset newGraph;
				SaveGraph(&newGraph, animComp.GraphPath);
				if (assets)
				{
					assets->Invalidate(animComp.GraphPath);
				}
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Create a new animation graph asset for this entity");
			}
			ImGui::End();
			return;
		}

		// Toolbar
		{
			ImVec4 btnColor = m_ChangedGraph ? ImVec4(0.8f, 0.4f, 0.1f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f);
			ImGui::PushStyleColor(ImGuiCol_Button, btnColor);
			if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save"))
			{
				SaveGraph(graph, animComp.GraphPath);
				m_ChangedGraph = false;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Save the animation graph to disk");
			}
			ImGui::PopStyleColor();

			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_COPY " Clone Graph"))
			{
				auto* assets = ServiceLocator::TryGet<AssetManager>();
				if (assets && !animComp.GraphPath.empty())
				{
					std::filesystem::path p(animComp.GraphPath);
					std::string newPath = (p.parent_path() / (p.stem().string() + "_copy.chag")).generic_string();
					int counter = 1;
					while (std::filesystem::exists(assets->ResolvePath(newPath)))
					{
						newPath =
							(p.parent_path() / (p.stem().string() + "_copy" + std::to_string(counter++) + ".chag"))
								.generic_string();
					}
					SaveGraph(graph, newPath);
					animComp.GraphPath = newPath;
					animComp.GraphAssetHandle = 0;
					assets->Invalidate(newPath);
					m_ChangedGraph = false;
				}
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Duplicate the current graph as a new file");
			}

			ImGui::SameLine();
			if (ImGui::Button("Add State"))
			{
				AnimNode newNode;
				newNode.ID = graph->NextNodeID++;
				newNode.Name = "State " + std::to_string(newNode.ID);
				newNode.EditorX = 100.0f + (graph->Nodes.size() % 3) * 220.0f;
				newNode.EditorY = 100.0f + (graph->Nodes.size() / 3) * 120.0f;
				graph->Nodes.push_back(newNode);
				m_Delegate.nodeSelected.push_back(false);
				m_ChangedGraph = true;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Add a new animation state node to the graph");
			}

			ImGui::SameLine();
			if (ImGui::Button("Fit All"))
			{
				m_Fit = GraphEditor::Fit_AllNodes;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Zoom and pan to fit all nodes in view");
			}

			ImGui::SameLine();
			if (ImGui::Button("Fit Selected"))
			{
				m_Fit = GraphEditor::Fit_SelectedNodes;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Zoom and pan to fit the selected node in view");
			}

			ImGui::SameLine();
			ImGui::TextDisabled("File: %s", animComp.GraphPath.c_str());
		}

		// Graph editor | Properties side-by-side
		m_Delegate.graph = graph;
		m_Delegate.changedFlag = &m_ChangedGraph;
		m_Delegate.SyncSelection();

		float avail = ImGui::GetContentRegionAvail().x;
		float graphWidth = avail * 0.72f;

		ImGui::BeginChild("GraphEditor", ImVec2(graphWidth, 0));
		GraphEditor::Show(m_Delegate, m_Options, m_ViewState, true, &m_Fit);
		ImGui::EndChild();

		ImGui::SameLine();

		ImGui::BeginChild("Properties", ImVec2(0, 0));
		DrawProperties(graph, selectedEntity);
		ImGui::EndChild();

		ImGui::End();
	}

	void AnimGraphPanel::RestorePreview()
	{
		if (!m_PreviewEntity || !m_PreviewEntity.HasComponent<AnimationComponent>())
		{
			return;
		}
		auto& anim = m_PreviewEntity.GetComponent<AnimationComponent>();
		anim.CurrentAnimationIndex = m_OrigAnimIndex;
		anim.CurrentFrame = m_OrigFrame;
		anim.StartFrame = m_OrigStartFrame;
		anim.EndFrame = m_OrigEndFrame;
		anim.Speed = m_OrigSpeed;
		anim.IsLooping = m_OrigIsLooping;
		anim.IsPlaying = m_OrigIsPlaying;
		m_PreviewNodeIdx = -1;
	}

	void AnimGraphPanel::ApplyPreview(AnimationGraphAsset* graph, int nodeIdx, Entity entity)
	{
		if (!graph || nodeIdx < 0 || nodeIdx >= (int)graph->Nodes.size())
		{
			return;
		}
		if (!entity || !entity.HasComponent<AnimationComponent>())
		{
			return;
		}

		auto& anim = entity.GetComponent<AnimationComponent>();
		auto& node = graph->Nodes[nodeIdx];

		// Save original state
		m_OrigAnimIndex = anim.CurrentAnimationIndex;
		m_OrigFrame = anim.CurrentFrame;
		m_OrigStartFrame = anim.StartFrame;
		m_OrigEndFrame = anim.EndFrame;
		m_OrigSpeed = anim.Speed;
		m_OrigIsLooping = anim.IsLooping;
		m_OrigIsPlaying = anim.IsPlaying;
		m_PreviewEntity = entity;

		// Override with node's animation
		anim.CurrentNodeID = node.ID;
		anim.CurrentAnimationIndex = node.AnimationIndex;
		anim.StartFrame = node.StartFrame;
		anim.EndFrame = node.EndFrame;
		anim.Speed = (node.Speed > 0.0f) ? node.Speed : 1.0f;
		anim.IsLooping = node.IsLooping;
		anim.CurrentFrame = node.StartFrame;
		anim.IsPlaying = true;

		m_PreviewNodeIdx = nodeIdx;
	}

	void AnimGraphPanel::DrawProperties(AnimationGraphAsset* graph, Entity entity)
	{
		if (!entity || !entity.HasComponent<AnimationComponent>())
		{
			return;
		}
		auto& animComp = entity.GetComponent<AnimationComponent>();

		ImGui::Text("Is Playing");
		ImGui::SameLine(120);
		ImGui::Checkbox("##isPlaying", &animComp.IsPlaying);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Toggle animation playback on/off");
		}

		ImGui::Separator();

		ImGui::Text("Properties");
		ImGui::Separator();

		// Find first selected node
		int selectedIdx = -1;
		for (size_t i = 0; i < m_Delegate.nodeSelected.size(); i++)
		{
			if (m_Delegate.nodeSelected[i])
			{
				selectedIdx = (int)i;
				break;
			}
		}

		if (selectedIdx != -1 && selectedIdx < (int)graph->Nodes.size())
		{
			AnimNode& node = graph->Nodes[selectedIdx];

			// Keep entity preview synced to selected node in Edit mode
			bool isSimulation = EditorLayer::Get().GetSceneState() != SceneState::Edit;
			if (!isSimulation)
			{
				animComp.CurrentNodeID = node.ID;
				animComp.CurrentAnimationIndex = node.AnimationIndex;
				animComp.StartFrame = node.StartFrame;
				animComp.EndFrame = node.EndFrame;
				animComp.Speed = (node.Speed > 0.0f) ? node.Speed : 1.0f;
				animComp.IsLooping = node.IsLooping;
			}

			char buffer[256];
			strncpy(buffer, node.Name.c_str(), sizeof(buffer));
			buffer[sizeof(buffer) - 1] = '\0';
			if (ImGui::InputText("Name", buffer, sizeof(buffer)))
			{
				node.Name = buffer;
				m_ChangedGraph = true;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Set the display name for this state node");
			}

			if (node.ID == graph->EntryNodeID)
			{
				ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.0f, 1.0f), "Entry Node");
			}
			else
			{
				if (ImGui::Button("Set as Entry"))
				{
					graph->EntryNodeID = node.ID;
					m_ChangedGraph = true;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Mark this node as the entry point of the graph");
				}
			}

			// Animation picker
			{
				bool foundModel = false;
				if (entity.HasComponent<ModelComponent>())
				{
					auto& modelComp = entity.GetComponent<ModelComponent>();
					auto* assets = ServiceLocator::TryGet<AssetManager>();
					if (assets)
					{
						auto modelAsset = assets->Get<ModelAsset>(modelComp.ModelPath);
						if (modelAsset && modelAsset->GetAnimationCount() > 0)
						{
							foundModel = true;
							int animCount = modelAsset->GetAnimationCount();
							int idx = node.AnimationIndex;
							if (idx < 0 || idx >= animCount)
							{
								idx = 0;
							}

							char labelBuf[128];
							std::string animName = modelAsset->GetAnimationName(idx);
							if (animName.empty())
							{
								snprintf(labelBuf, sizeof(labelBuf), "Animation %d", idx);
							}
							else
							{
								snprintf(labelBuf, sizeof(labelBuf), "%s (%d)", animName.c_str(), idx);
							}

							if (ImGui::BeginCombo("Animation", labelBuf))
							{
								for (int i = 0; i < animCount; i++)
								{
									bool isSel = (idx == i);
									std::string name = modelAsset->GetAnimationName(i);
									char itemBuf[128];
									if (name.empty())
									{
										snprintf(itemBuf, sizeof(itemBuf), "Animation %d", i);
									}
									else
									{
										snprintf(itemBuf, sizeof(itemBuf), "%s (%d)", name.c_str(), i);
									}
									if (ImGui::Selectable(itemBuf, isSel))
									{
										node.AnimationIndex = i;
										node.StartFrame = 0;
										node.EndFrame = -1;
										m_ChangedGraph = true;
									}
								}
								ImGui::EndCombo();
							}
							if (ImGui::IsItemHovered())
							{
								ImGui::SetTooltip("Select an animation clip from the model");
							}

							// Show frame range info
							const auto& rawAnims = modelAsset->GetAnimations();
							if (idx >= 0 && idx < (int)rawAnims.size())
							{
								int totalFrames = rawAnims[idx].frameCount;
								float fps = rawAnims[idx].frameRate;
								float duration = (float)totalFrames / fps;
								ImGui::TextDisabled("Total: %d frames (%.2fs @ %.0f fps)", totalFrames, duration, fps);

								// Start Frame
								int sf = node.StartFrame;
								if (ImGui::DragInt("Start Frame", &sf, 1, 0, totalFrames - 1))
								{
									node.StartFrame = std::clamp(sf, 0, totalFrames - 1);
									if (node.EndFrame >= 0 && node.EndFrame < node.StartFrame)
									{
										node.EndFrame = node.StartFrame;
									}
									m_ChangedGraph = true;
								}
								if (ImGui::IsItemHovered())
								{
									ImGui::SetTooltip("First frame of the animation clip to play");
								}

								// End Frame
								int ef = node.EndFrame;
								const char* endLabel = (node.EndFrame < 0) ? "End Frame (auto)" : "End Frame";
								if (ImGui::DragInt(endLabel, &ef, 1, -1, totalFrames - 1))
								{
									node.EndFrame = ef;
									if (node.EndFrame >= 0 && node.EndFrame < node.StartFrame)
									{
										node.EndFrame = node.StartFrame;
									}
									m_ChangedGraph = true;
								}
								if (ImGui::IsItemHovered())
								{
									ImGui::SetTooltip("Last frame (-1 = play to end)");
								}

								// Duration preview
								int endFrame = (node.EndFrame < 0) ? (totalFrames - 1) : node.EndFrame;
								int clipLength = endFrame - node.StartFrame + 1;
								float clipDuration = (float)clipLength / fps;
								float speedDuration = (node.Speed > 0.0f) ? clipDuration / node.Speed : clipDuration;
								ImGui::Text("Clip: %d frames (%.2fs)", clipLength, clipDuration);
								if (node.Speed != 1.0f)
								{
									ImGui::Text("Effective: %.2fs (speed %.1fx)", speedDuration, node.Speed);
								}
							}
						}
					}
				}
				if (!foundModel)
				{
					if (ImGui::InputInt("Animation Index", &node.AnimationIndex))
					{
						m_ChangedGraph = true;
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Index of the animation clip in the model");
					}
					if (ImGui::InputInt("Start Frame", &node.StartFrame))
					{
						m_ChangedGraph = true;
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("First frame of the animation clip");
					}
					if (ImGui::InputInt("End Frame", &node.EndFrame))
					{
						m_ChangedGraph = true;
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Last frame (-1 = play to end)");
					}
				}
			}

			if (ImGui::Checkbox("Is Looping", &node.IsLooping))
			{
				m_ChangedGraph = true;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Loop the animation when it reaches the end");
			}

			if (ImGui::DragFloat("Speed", &node.Speed, 0.05f, 0.01f, 10.0f, "%.2f"))
			{
				m_ChangedGraph = true;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Playback speed multiplier (1.0 = normal)");
			}

			if (m_ChangedGraph)
			{
				animComp.CurrentNodeID = node.ID;
				animComp.CurrentAnimationIndex = node.AnimationIndex;
				animComp.StartFrame = node.StartFrame;
				animComp.EndFrame = node.EndFrame;
				animComp.Speed = (node.Speed > 0.0f) ? node.Speed : 1.0f;
				animComp.IsLooping = node.IsLooping;
				if (animComp.CurrentFrame < animComp.StartFrame ||
					(animComp.EndFrame >= 0 && animComp.CurrentFrame > animComp.EndFrame))
				{
					animComp.CurrentFrame = animComp.StartFrame;
				}
			}

			ImGui::Separator();
			if (ImGui::Button("Delete Node"))
			{
				int nodeId = node.ID;
				graph->Nodes.erase(std::remove_if(graph->Nodes.begin(), graph->Nodes.end(),
												  [nodeId](const AnimNode& n) { return n.ID == nodeId; }),
								   graph->Nodes.end());
				graph->Transitions.erase(std::remove_if(graph->Transitions.begin(), graph->Transitions.end(),
														[nodeId](const AnimTransition& t) {
															return t.SourceNodeID == nodeId || t.TargetNodeID == nodeId;
														}),
										 graph->Transitions.end());
				if (graph->EntryNodeID == nodeId)
				{
					graph->EntryNodeID = -1;
				}
				m_Delegate.SyncSelection();
				m_ChangedGraph = true;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Remove this node and all its transitions");
			}
		}
		else
		{
			ImGui::TextDisabled("Select a node to edit properties");
		}

		// Transitions section
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text("Transitions");
		ImGui::Separator();

		if (ImGui::Button("+ Add Transition"))
		{
			AnimTransition tr;
			tr.ID = graph->NextLinkID++;
			tr.BlendDuration = 0.2f;
			graph->Transitions.push_back(tr);
			m_ChangedGraph = true;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Add a new transition between nodes");
		}

		for (size_t i = 0; i < graph->Transitions.size(); i++)
		{
			AnimTransition& tr = graph->Transitions[i];

			auto findNodeName = [&](int nodeID) -> std::string {
				for (auto& n : graph->Nodes)
				{
					if (n.ID == nodeID)
					{
						return n.Name;
					}
				}
				return "Unknown";
			};

			ImGui::PushID((int)i);

			bool open = ImGui::TreeNode("##tr", "%s -> %s", findNodeName(tr.SourceNodeID).c_str(),
										findNodeName(tr.TargetNodeID).c_str());
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
			{
				// Select this transition
			}

			if (open)
			{
				// Source node picker
				{
					int srcIdx = -1;
					for (size_t j = 0; j < graph->Nodes.size(); j++)
					{
						if (graph->Nodes[j].ID == tr.SourceNodeID)
						{
							srcIdx = (int)j;
							break;
						}
					}

					char srcLabel[128];
					snprintf(srcLabel, sizeof(srcLabel), "%s###src",
							 (srcIdx >= 0) ? graph->Nodes[srcIdx].Name.c_str() : "None");
					if (ImGui::BeginCombo("From", srcLabel))
					{
						for (size_t j = 0; j < graph->Nodes.size(); j++)
						{
							bool isSel = ((int)j == srcIdx);
							if (ImGui::Selectable(graph->Nodes[j].Name.c_str(), isSel))
							{
								tr.SourceNodeID = graph->Nodes[j].ID;
								m_ChangedGraph = true;
							}
						}
						ImGui::EndCombo();
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Source node for this transition");
					}
				}

				// Target node picker
				{
					int tgtIdx = -1;
					for (size_t j = 0; j < graph->Nodes.size(); j++)
					{
						if (graph->Nodes[j].ID == tr.TargetNodeID)
						{
							tgtIdx = (int)j;
							break;
						}
					}

					char tgtLabel[128];
					snprintf(tgtLabel, sizeof(tgtLabel), "%s###tgt",
							 (tgtIdx >= 0) ? graph->Nodes[tgtIdx].Name.c_str() : "None");
					if (ImGui::BeginCombo("To", tgtLabel))
					{
						for (size_t j = 0; j < graph->Nodes.size(); j++)
						{
							bool isSel = ((int)j == tgtIdx);
							if (ImGui::Selectable(graph->Nodes[j].Name.c_str(), isSel))
							{
								tr.TargetNodeID = graph->Nodes[j].ID;
								m_ChangedGraph = true;
							}
						}
						ImGui::EndCombo();
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Target node for this transition");
					}
				}

				if (ImGui::DragFloat("Blend Duration", &tr.BlendDuration, 0.01f, 0.0f, 5.0f, "%.2f s"))
				{
					m_ChangedGraph = true;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Time in seconds to blend between states");
				}

				if (ImGui::Checkbox("Has Exit Time", &tr.HasExitTime))
				{
					m_ChangedGraph = true;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Require a specific time before transitioning");
				}

				if (tr.HasExitTime)
				{
					if (ImGui::DragFloat("Exit Time", &tr.ExitTime, 0.01f, 0.0f, 1.0f, "%.2f"))
					{
						m_ChangedGraph = true;
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Normalized time (0-1) at which exit is allowed");
					}
				}

				// Conditions
				ImGui::Text("Conditions:");
				if (ImGui::Button("+ Condition"))
				{
					AnimCondition cond;
					cond.Op = AnimConditionOp::Greater;
					tr.Conditions.push_back(cond);
					m_ChangedGraph = true;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Add a new condition to this transition");
				}

				for (size_t c = 0; c < tr.Conditions.size(); c++)
				{
					AnimCondition& cond = tr.Conditions[c];
					ImGui::PushID((int)c);

					char varBuf[128];
					strncpy(varBuf, cond.VariableName.c_str(), sizeof(varBuf));
					varBuf[sizeof(varBuf) - 1] = '\0';
					if (ImGui::InputText("Var", varBuf, sizeof(varBuf)))
					{
						cond.VariableName = varBuf;
						m_ChangedGraph = true;
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Variable name to evaluate");
					}

					const char* ops[] = {"==", "!=", ">", "<", ">=", "<="};
					int opIdx = (int)cond.Op;
					if (ImGui::Combo("Op", &opIdx, ops, IM_ARRAYSIZE(ops)))
					{
						cond.Op = (AnimConditionOp)opIdx;
						m_ChangedGraph = true;
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Comparison operator for this condition");
					}

					if (ImGui::DragFloat("Value", &cond.Value, 0.01f))
					{
						m_ChangedGraph = true;
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Value to compare against");
					}

					ImGui::SameLine();
					if (ImGui::SmallButton("X"))
					{
						tr.Conditions.erase(tr.Conditions.begin() + c);
						m_ChangedGraph = true;
						ImGui::PopID();
						break;
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Remove this condition");
					}

					ImGui::PopID();
				}

				if (ImGui::SmallButton("Delete Transition"))
				{
					graph->Transitions.erase(graph->Transitions.begin() + i);
					m_ChangedGraph = true;
					ImGui::TreePop();
					ImGui::PopID();
					break;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Remove this transition");
				}

				ImGui::TreePop();
			}

			ImGui::PopID();
		}

		// Variables section
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Text("Variables");
		ImGui::Separator();

		// Two buttons: + Float  |  + Bool
		if (ImGui::Button("+ Float"))
		{
			std::string key = "new_float";
			int idx = 0;
			while (animComp.Variables.count(key))
			{
				key = "new_float_" + std::to_string(++idx);
			}
			animComp.Variables[key] = 0.0f;
			graph->DefaultVariables[key] = 0.0f; // sync to graph schema
			m_VariableTypes[key] = VarType::Float;
			m_ChangedGraph = true;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Add a new float variable");
		}
		ImGui::SameLine();
		if (ImGui::Button("+ Bool"))
		{
			std::string key = "new_bool";
			int idx = 0;
			while (animComp.Variables.count(key))
			{
				key = "new_bool_" + std::to_string(++idx);
			}
			animComp.Variables[key] = 0.0f;
			graph->DefaultVariables[key] = 0.0f; // sync to graph schema
			m_VariableTypes[key] = VarType::Bool;
			m_ChangedGraph = true;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Add a new bool variable");
		}

		auto it = animComp.Variables.begin();
		while (it != animComp.Variables.end())
		{
			ImGui::PushID(it->first.c_str());

			// Infer type: if not in our map, guess by name
			if (m_VariableTypes.find(it->first) == m_VariableTypes.end())
			{
				std::string lowerKey = it->first;
				std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(), ::tolower);

				bool looksLikeBool =
					(lowerKey.rfind("is", 0) == 0 || lowerKey.rfind("has", 0) == 0 || lowerKey.rfind("can", 0) == 0 ||
					 lowerKey.rfind("should", 0) == 0 || lowerKey.find("bool") != std::string::npos ||
					 lowerKey.find("flag") != std::string::npos);
				m_VariableTypes[it->first] = looksLikeBool ? VarType::Bool : VarType::Float;
			}

			VarType varType = m_VariableTypes[it->first];

			// Type badge button: clickable [F] or [B] to toggle type
			if (varType == VarType::Float)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.7f, 1.0f));
				if (ImGui::Button("[F]"))
				{
					m_VariableTypes[it->first] = VarType::Bool;
				}
				ImGui::PopStyleColor();
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Float (Click to switch to Bool)");
				}
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.5f, 0.1f, 1.0f));
				if (ImGui::Button("[B]"))
				{
					m_VariableTypes[it->first] = VarType::Float;
				}
				ImGui::PopStyleColor();
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Bool (Click to switch to Float)");
				}
			}
			ImGui::SameLine();

			// Name field
			char varBuf[128];
			strncpy(varBuf, it->first.c_str(), sizeof(varBuf));
			varBuf[sizeof(varBuf) - 1] = '\0';
			ImGui::SetNextItemWidth(120.0f);
			if (ImGui::InputText("##name", varBuf, sizeof(varBuf), ImGuiInputTextFlags_EnterReturnsTrue))
			{
				float val = it->second;
				VarType t = m_VariableTypes[it->first];
				std::string oldKey = it->first;
				std::string newKey = varBuf;
				if (newKey == oldKey || (!graph->DefaultVariables.count(newKey) && !animComp.Variables.count(newKey)))
				{
					m_VariableTypes.erase(oldKey);
					// Rename in graph DefaultVariables
					graph->DefaultVariables.erase(oldKey);
					graph->DefaultVariables[newKey] = val;
					animComp.Variables.erase(it);
					animComp.Variables[newKey] = val;
					m_VariableTypes[newKey] = t;
					m_ChangedGraph = true;
				}
				ImGui::PopID();
				break;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Variable name (press Enter to rename)");
			}
			ImGui::SameLine();

			// Value widget: checkbox for bool, drag for float
			if (varType == VarType::Bool)
			{
				bool bval = (it->second >= 0.5f);
				if (ImGui::Checkbox("##val", &bval))
				{
					it->second = bval ? 1.0f : 0.0f;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Toggle this bool variable's value");
				}
			}
			else
			{
				float val = it->second;
				ImGui::SetNextItemWidth(80.0f);
				if (ImGui::DragFloat("##val", &val, 0.01f))
				{
					it->second = val;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Drag to change this float variable's value");
				}
			}

			ImGui::SameLine();
			if (ImGui::SmallButton("X"))
			{
				graph->DefaultVariables.erase(it->first); // sync to graph schema
				m_VariableTypes.erase(it->first);
				it = animComp.Variables.erase(it);
				m_ChangedGraph = true;
				ImGui::PopID();
			}
			else
			{
				++it;
				ImGui::PopID();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Delete this variable");
			}
		}
	}

	void AnimGraphPanel::SaveGraph(AnimationGraphAsset* graph, const std::string& path)
	{
		if (!graph || path.empty())
		{
			return;
		}

		auto* assets = ServiceLocator::TryGet<AssetManager>();
		if (assets)
		{
			std::string resolved = assets->ResolvePath(path);
			AnimGraphLoader loader;
			if (loader.Save(*graph, resolved))
			{
				CH_CORE_INFO("Animation graph saved: {}", resolved);
			}
			else
			{
				CH_CORE_ERROR("Failed to save animation graph: {}", resolved);
			}
		}
	}

} // namespace Chained
