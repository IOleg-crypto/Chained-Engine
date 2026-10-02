#include "scene_hierarchy_panel.h"
#include "editor/events.h"
#include "editor/layer.h"
#include "editor/types.h"
#include "editor/undo/command_history.h"
#include "engine/app/application.h"
#include "engine/platform/dialogs/dialogs.h"
#include "engine/scene/components.h"
#include "engine/scene/scene_events.h"
#include "engine/scene/scene_settings.h"
#include "thirdparty/IconsFontAwesome6.h"

#include "engine/core/input.h"
#include "engine/core/platform.h"
#include "engine/scene/prefab_serializer.h"
#include "engine/scene/scene_serializer.h"
#include "events.h"
#include "imgui.h"
#include "undo/entity_commands.h"
#include <functional>
#include <queue>
#include <vector>

namespace
{
	bool IsDescendant(Chained::Entity child, Chained::Entity possibleParent)
	{
		if (child == possibleParent)
		{
			return true;
		}

		if (!possibleParent.HasComponent<Chained::HierarchyComponent>())
		{
			return false;
		}

		std::queue<entt::entity> queue;
		for (entt::entity c : possibleParent.GetComponent<Chained::HierarchyComponent>().Children)
		{
			queue.push(c);
		}

		while (!queue.empty())
		{
			entt::entity current = queue.front();
			queue.pop();

			Chained::Entity currentEnt(current, child.GetRegistryPtr());
			if (currentEnt == child)
			{
				return true;
			}

			if (currentEnt.HasComponent<Chained::HierarchyComponent>())
			{
				for (entt::entity c : currentEnt.GetComponent<Chained::HierarchyComponent>().Children)
				{
					queue.push(c);
				}
			}
		}
		return false;
	}
} // namespace

namespace Chained
{
	SceneHierarchyPanel::SceneHierarchyPanel()
	{
		m_Name = "Scene Hierarchy";
	}

	void SceneHierarchyPanel::OnImGuiRender(bool readOnly)
	{
		ImGui::Begin("Scene Hierarchy###SceneHierarchyPanel");

		// Search Bar
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{4, 4});
		ImGui::InputTextWithHint("##Search", ICON_FA_MAGNIFYING_GLASS " Search...", m_SearchBuffer,
								 sizeof(m_SearchBuffer));
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Filter entities by name");
		}
		ImGui::PopStyleVar();
		ImGui::Separator();

		if (m_Context)
		{
			m_DrawnEntities.clear();
			m_EntitiesToDestroyPending.clear();

			bool isTransitioning = EditorLayer::Get().GetSceneManager().IsTransitioning();
			ImGui::BeginDisabled(readOnly || isTransitioning);

			std::string filter = m_SearchBuffer;
			std::transform(filter.begin(), filter.end(), filter.begin(), ::tolower);

			// Draw entities
			auto view = m_Context->GetRegistry().view<IDComponent>();
			for (auto entityID : view)
			{
				Entity entity(entityID, &m_Context->GetRegistry());

				// Skip child entities, they will be drawn recursively
				if (entity.HasComponent<HierarchyComponent>() &&
					entity.GetComponent<HierarchyComponent>().Parent != entt::null)
				{
					continue;
				}

				// Skip hidden UI components
				if (entity.HasComponent<ControlComponent>() &&
					entity.GetComponent<ControlComponent>().HiddenInHierarchy)
				{
					continue;
				}

				// Apply search filter (if not empty)
				if (!filter.empty())
				{
					std::string tag = entity.GetComponent<TagComponent>().Tag;
					std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);
					if (tag.find(filter) == std::string::npos)
					{
						continue;
					}
				}

				DrawEntityNodeRecursive(entity, readOnly);
			}

			if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
			{
				DeselectEntity(m_Context.get());
			}

			// Focus Shortcut
			if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) && Core::Input::IsKeyPressed(KeyCode::F))
			{
				Entity selected = EditorLayer::Get().GetEditorState().SelectedEntity;
				if (selected)
				{
					ViewportFocusEntityEvent e(selected);
					Application::Get().OnEvent(e);
				}
			}

			// Duplicate Shortcut
			if (ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows) &&
				Core::Input::IsKeyDown(KeyCode::LeftControl) && Core::Input::IsKeyPressed(KeyCode::D))
			{
				Entity selected = EditorLayer::Get().GetEditorState().SelectedEntity;
				if (selected)
				{
					EditorLayer::Get().GetCommandHistory().PushCommand(
						std::make_unique<DuplicateEntityCommand>(selected));
				}
			}

			// Blank space context menu
			if (!readOnly &&
				ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
			{
				DrawContextMenu();
				ImGui::EndPopup();
			}

			// Blank space drop target to unparent
			ImGui::Dummy(ImGui::GetContentRegionAvail());
			if (!readOnly && ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY"))
				{
					uint64_t droppedUUID = *(const uint64_t*)payload->Data;
					Entity sourceEntity = m_Context->GetEntityByUUID(droppedUUID);
					if (sourceEntity)
					{
						EditorLayer::Get().GetCommandHistory().PushCommand(
							std::make_unique<ParentEntityCommand>(sourceEntity, Entity{}, m_Context.get()));
					}
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::EndDisabled();
		}

		ImGui::End();

		if (!m_EntitiesToDestroyPending.empty())
		{
			for (auto ent : m_EntitiesToDestroyPending)
			{
				Entity entity(ent, &m_Context->GetRegistry());
				if (entity.IsValid())
				{
					EditorLayer::Get().GetCommandHistory().PushCommand(std::make_unique<DestroyEntityCommand>(entity));
				}
			}
			m_EntitiesToDestroyPending.clear();
		}
	}

	const char* SceneHierarchyPanel::GetEntityIcon(Entity entity)
	{
		// Priority 1: UIControlComponent — detect widget subtype
		if (entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<ButtonData>(widget.Data))
			{
				return ICON_FA_ARROW_POINTER;
			}
			if (std::holds_alternative<LabelData>(widget.Data))
			{
				return ICON_FA_FONT;
			}
			if (std::holds_alternative<SliderData>(widget.Data))
			{
				return ICON_FA_SLIDERS;
			}
			if (std::holds_alternative<CheckboxData>(widget.Data))
			{
				return ICON_FA_SQUARE_CHECK;
			}
			if (std::holds_alternative<ImageData>(widget.Data) || std::holds_alternative<ImageButtonData>(widget.Data))
			{
				return ICON_FA_IMAGE;
			}
			return ICON_FA_WINDOW_MAXIMIZE;
		}

		// Priority 2+: Use ComponentRegistry icon lookup
		auto& registry = entity.GetRegistry();
		auto& compRegistry = ComponentRegistry::GetRegistry();

		// Check components in a fixed priority order for consistent icon display
		static const entt::id_type priorityOrder[] = {
			entt::type_hash<ControlComponent>::value(),		  entt::type_hash<LightComponent>::value(),
			entt::type_hash<CameraComponent>::value(),		  entt::type_hash<AudioComponent>::value(),
			entt::type_hash<ManagedScriptComponent>::value(), entt::type_hash<ModelComponent>::value(),
			entt::type_hash<RigidBodyComponent>::value(),	  entt::type_hash<ColliderComponent>::value(),
			entt::type_hash<AnimationComponent>::value(),
		};

		for (auto typeId : priorityOrder)
		{
			if (compRegistry.contains(typeId))
			{
				auto& meta = compRegistry.at(typeId);
				if (meta.Icon && meta.Has && meta.Has(entity))
				{
					return meta.Icon;
				}
			}
		}

		return ICON_FA_CUBE;
	}

	void SceneHierarchyPanel::DrawEntityNodeRecursive(Entity entity, bool readOnly)
	{
		if (!entity || !entity.IsValid() || m_DrawnEntities.contains(entity))
		{
			return;
		}

		m_DrawnEntities.insert(entity);

		auto& tag = entity.GetComponent<TagComponent>().Tag;
		std::string label = std::string(GetEntityIcon(entity)) + "  " + tag;

		auto selectedEntity = EditorLayer::Get().GetEditorState().SelectedEntity;
		ImGuiTreeNodeFlags flags = ((selectedEntity == entity) ? ImGuiTreeNodeFlags_Selected : 0);
		flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

		if (!entity.HasComponent<HierarchyComponent>() || entity.GetComponent<HierarchyComponent>().Children.empty())
		{
			flags |= ImGuiTreeNodeFlags_Leaf;
		}

		ImGui::PushID((int)(uint32_t)entity);

		bool opened = false;
		bool renamed = false;

		if (m_Renaming && m_RenamingEntity == entity)
		{
			ImGui::SetKeyboardFocusHere();
			if (ImGui::InputText("##Rename", m_RenameBuffer, sizeof(m_RenameBuffer),
								 ImGuiInputTextFlags_EnterReturnsTrue) ||
				(ImGui::IsMouseClicked(0) && !ImGui::IsItemHovered()))
			{
				tag = m_RenameBuffer;
				m_Renaming = false;
				renamed = true;
			}
		}
		else
		{
			opened = ImGui::TreeNodeEx(label.c_str(), flags);
		}

		// Drag & Drop Source
		if (!readOnly && ImGui::BeginDragDropSource())
		{
			uint64_t uuid = entity.GetUUID();
			ImGui::SetDragDropPayload("ENTITY", &uuid, sizeof(uint64_t));
			ImGui::Text("%s", label.c_str());
			ImGui::EndDragDropSource();
		}

		// Drag & Drop Target
		if (!readOnly && ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY"))
			{
				uint64_t droppedUUID = *(const uint64_t*)payload->Data;
				Entity sourceEntity = m_Context->GetEntityByUUID(droppedUUID);
				if (sourceEntity && sourceEntity != entity && !IsDescendant(entity, sourceEntity))
				{
					EditorLayer::Get().GetCommandHistory().PushCommand(
						std::make_unique<ParentEntityCommand>(sourceEntity, entity, m_Context.get()));
				}
			}
			ImGui::EndDragDropTarget();
		}

		if (ImGui::IsItemClicked())
		{
			SelectEntity(entity, m_Context.get());
		}

		// Rename on F2
		if (selectedEntity == entity && ImGui::IsKeyPressed(ImGuiKey_F2) && !m_Renaming)
		{
			StartRename(entity);
		}

		if (!readOnly && ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem(ICON_FA_PEN " Rename", "F2"))
			{
				StartRename(entity);
			}
			if (ImGui::MenuItem(ICON_FA_COPY " Duplicate", "Ctrl+D"))
			{
				EditorLayer::Get().GetCommandHistory().PushCommand(std::make_unique<DuplicateEntityCommand>(entity));
			}
			ImGui::Separator();
			if (ImGui::MenuItem(ICON_FA_TRASH " Delete Entity", "Del"))
			{
				m_EntitiesToDestroyPending.push_back((entt::entity)entity);
			}
			ImGui::Separator();
			if (ImGui::MenuItem(ICON_FA_FILE_EXPORT " Save as Prefab..."))
			{
				std::vector<DialogFilter> filters = {{"Chained Prefab", "chprefab"}};
				auto path = Chained::Dialogs::SaveFile(filters);
				if (path)
				{
					if (path->extension().empty())
					{
						path->replace_extension(".chprefab");
					}
					PrefabSerializer::Serialize(entity, path->string());
				}
			}

			ImGui::EndPopup();
		}

		if (opened)
		{
			if (entity.HasComponent<HierarchyComponent>())
			{
				auto children = entity.GetComponent<HierarchyComponent>().Children; // Copy to avoid iteration issues
				for (auto childID : children)
				{
					DrawEntityNodeRecursive(Entity(childID, &m_Context->GetRegistry()), readOnly);
				}
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	void SceneHierarchyPanel::DrawContextMenu()
	{
		if (ImGui::MenuItem("Create Empty Entity"))
		{
			m_Context->CreateEntity("Empty Entity");
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Create a new empty entity");
		}

		if (ImGui::MenuItem(ICON_FA_FILE_IMPORT " Load Prefab..."))
		{
			std::vector<DialogFilter> filters = {{"Chained Prefab", "chprefab"}};
			auto path = Chained::Dialogs::OpenFile(filters);
			if (path)
			{
				PrefabSerializer::Deserialize(m_Context.get(), path->string());
			}
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Import a prefab file into the scene");
		}

		ImGui::Separator();

		// --- Quick Create: Lights & Camera ---
		if (m_Context->GetSettings().Type != SceneType::UI ||
			m_Context->GetSettings().Mode == BackgroundMode::Environment3D)
		{
			struct QuickCreateEntry
			{
				const char* label;
				const char* icon;
				std::function<void()> action;
			};
			static const QuickCreateEntry quickCreates[] = {
				{"Camera", ICON_FA_VIDEO,
				 [this]() {
					 auto e = m_Context->CreateEntity("Camera");
					 e.AddComponent<CameraComponent>();
				 }},
				{"Point Light", ICON_FA_LIGHTBULB,
				 [this]() {
					 auto e = m_Context->CreateEntity("Point Light");
					 e.AddComponent<LightComponent>().Type = LightType::Point;
				 }},
				{"Spot Light", ICON_FA_LIGHTBULB,
				 [this]() {
					 auto e = m_Context->CreateEntity("Spot Light");
					 e.AddComponent<LightComponent>().Type = LightType::Spot;
				 }},
				{"Directional Light", ICON_FA_LIGHTBULB,
				 [this]() {
					 auto e = m_Context->CreateEntity("Directional Light");
					 e.AddComponent<LightComponent>().Type = LightType::Directional;
				 }},
			};
			for (auto& entry : quickCreates)
			{
				std::string label = entry.icon ? std::string(entry.icon) + " " + entry.label : entry.label;
				if (ImGui::MenuItem(label.c_str()))
				{
					entry.action();
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Create a new %s entity", entry.label);
				}
			}

			ImGui::Separator();

			// --- 3D Object Submenu ---
			struct PrimitiveEntry
			{
				const char* label;
				const char* mesh;
			};
			static const PrimitiveEntry primitives[] = {
				{"Cube", "models/primitives/Cube.obj"},		  {"Sphere", "models/primitives/Sphere.obj"},
				{"Capsule", "models/primitives/Capsule.obj"}, {"Cylinder", "models/primitives/Cylinder.obj"},
				{"Plane", "models/primitives/Plane.obj"},	  {"Cone", "models/primitives/Cone.obj"},
				{"Torus", "models/primitives/Torus.obj"},	  {"Hemisphere", "models/primitives/Hemisphere.obj"},
			};
			if (ImGui::BeginMenu("3D Object"))
			{
				for (auto& p : primitives)
				{
					if (ImGui::MenuItem(p.label))
					{
						EditorLayer::Get().GetCommandHistory().PushCommand(
							std::make_unique<CreateEntityCommand>(m_Context.get(), p.label, p.mesh));
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Create a %s primitive", p.label);
					}
				}
				ImGui::EndMenu();
			}
		}

		// --- UI Widget Submenus ---
		if (m_Context->GetSettings().Type == SceneType::UI ||
			m_Context->GetSettings().Mode != BackgroundMode::Environment3D)
		{
			struct WidgetEntry
			{
				const char* label;
				WidgetType type;
			};
			struct WidgetCategory
			{
				const char* label;
				const WidgetEntry* entries;
				int count;
			};

			static const WidgetEntry basicWidgets[] = {
				{"Panel", WidgetType_Panel},	   {"Button", WidgetType_Button},
				{"Label", WidgetType_Label},	   {"Slider", WidgetType_Slider},
				{"Checkbox", WidgetType_Checkbox}, {"InputText", WidgetType_InputText},
				{"ComboBox", WidgetType_ComboBox}, {"ProgressBar", WidgetType_ProgressBar},
			};
			static const WidgetEntry visualWidgets[] = {
				{"Image", WidgetType_Image},
				{"Image Button", WidgetType_ImageButton},
				{"Separator", WidgetType_Separator},
			};
			static const WidgetEntry inputWidgets[] = {
				{"RadioButton", WidgetType_RadioButton},
				{"ColorPicker", WidgetType_ColorPicker},
				{"Drag Float", WidgetType_DragFloat},
				{"Drag Int", WidgetType_DragInt},
			};
			static const WidgetEntry structuralWidgets[] = {
				{"Tree Node", WidgetType_TreeNode},
				{"Tab Bar", WidgetType_TabBar},
				{"Tab Item", WidgetType_TabItem},
				{"Collapsing Header", WidgetType_CollapsingHeader},
			};
			static const WidgetEntry chartWidgets[] = {
				{"Plot Lines", WidgetType_PlotLines},
				{"Plot Histogram", WidgetType_PlotHistogram},
			};

			static const WidgetCategory widgetCategories[] = {
				{"Basic", basicWidgets, (int)std::size(basicWidgets)},
				{"Visual", visualWidgets, (int)std::size(visualWidgets)},
				{"Input", inputWidgets, (int)std::size(inputWidgets)},
				{"Structural", structuralWidgets, (int)std::size(structuralWidgets)},
				{"Charts", chartWidgets, (int)std::size(chartWidgets)},
			};

			if (ImGui::BeginMenu("Control"))
			{
				for (auto& cat : widgetCategories)
				{
					if (ImGui::BeginMenu(cat.label))
					{
						for (int i = 0; i < cat.count; ++i)
						{
							if (ImGui::MenuItem(cat.entries[i].label))
							{
								m_Context->CreateUIEntity(cat.entries[i].type);
							}
							if (ImGui::IsItemHovered())
							{
								ImGui::SetTooltip("Create a %s UI widget", cat.entries[i].label);
							}
						}
						ImGui::EndMenu();
					}
				}
				ImGui::EndMenu();
			}
		}
	}
	void SceneHierarchyPanel::StartRename(Entity entity)
	{
		m_Renaming = true;
		m_RenamingEntity = entity;
		snprintf(m_RenameBuffer, sizeof(m_RenameBuffer), "%s", entity.GetComponent<TagComponent>().Tag.c_str());
	}

} // namespace Chained
