#include "property_editor.h"
#include <any>
#include "engine/scene/components/core/tag_component.h"
#include "engine/scene/components/core/transform_component.h"
#include "engine/reflection/reflection_rfl.h"
#include "engine/reflection/reflection_rfl_impl.h"
#include "engine/scene/component_registry.h"
#include "thirdparty/IconsFontAwesome6.h"
#include "editor/undo/command_history.h"
#include "editor/undo/component_commands.h"
#include "editor/undo/modify_component_command.h"
#include "engine/core/service_locator.h"
#include "gui.h"
#include <algorithm>

#include "engine/physics/physics.h"
#include "engine/scene/scene_settings.h"
#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"
#include "ui_properties.h" // Included here to break circular dependency
#include <memory>
#include "engine/scripting/scriptengine.h"
#include <Coral/ManagedObject.hpp>

#include "engine/app/application.h"
#include "engine/scene/components/ui/control_component.h"
#include <yaml-cpp/yaml.h>
#include "engine/assets/asset_manager.h"
#include "engine/assets/types/model_asset.h"
#include "engine/scene/components/animation/animation_component.h"
#include "engine/assets/loaders/anim_graph_loader.h"
#include "engine/assets/types/animation_graph_asset.h"
#include "engine/ui/ui_font_registry.h"
#include "engine/ui/widget_renderer.h"

namespace
{
	static Chained::CommandHistory* s_CommandHistory = nullptr;
}
namespace Chained
{

	// --- UI Widget Data Drawers (extracted from UIControlComponent lambda) ---

	static bool DrawButtonData(ButtonData& data, UIProperties& ui)
	{
		bool changed = false;
		if (ui.Property("Label", data.Label))
		{
			changed = true;
		}
		if (ui.Property("Interactable", data.IsInteractable))
		{
			changed = true;
		}
		if (ui.Property("Auto Size", data.AutoSize))
		{
			changed = true;
		}
		return changed;
	}

	static bool DrawLabelData(LabelData& data, UIProperties& ui)
	{
		bool changed = false;
		if (ui.Property("Text", data.Text))
		{
			changed = true;
		}
		if (ui.Property("Auto Size", data.AutoSize))
		{
			changed = true;
		}
		return changed;
	}

	static bool DrawCheckboxData(CheckboxData& data, UIProperties& ui)
	{
		bool changed = false;
		if (ui.Property("Label", data.Label))
		{
			changed = true;
		}
		if (ui.Property("Checked", data.Checked))
		{
			changed = true;
		}
		return changed;
	}

	static bool DrawSliderData(SliderData& data, UIProperties& ui)
	{
		bool changed = false;
		if (ui.Property("Label", data.Label))
		{
			changed = true;
		}
		if (ui.Property("Value", data.Value, PropertyMeta(data.Min, data.Max, 0.01f)))
		{
			changed = true;
		}
		if (ui.Property("Min", data.Min))
		{
			changed = true;
		}
		if (ui.Property("Max", data.Max))
		{
			changed = true;
		}
		return changed;
	}

	static bool DrawProgressBarData(ProgressBarData& data, UIProperties& ui)
	{
		bool changed = false;
		if (ui.Property("Progress", data.Progress, PropertyMeta(0.0f, 1.0f, 0.01f)))
		{
			changed = true;
		}
		if (ui.Property("Overlay Text", data.OverlayText))
		{
			changed = true;
		}
		if (ui.Property("Show %", data.ShowPercentage))
		{
			changed = true;
		}
		return changed;
	}

	static bool DrawImageData(ImageData& data, UIProperties& ui)
	{
		bool changed = false;
		if (ui.File("Texture Path", data.TexturePath, ".png,.jpg,.jpeg,.bmp,.tga"))
		{
			changed = true;
		}
		if (ui.Property("Tint Color", data.TintColor))
		{
			changed = true;
		}
		if (ui.Property("Border Color", data.BorderColor))
		{
			changed = true;
		}
		return changed;
	}

	static bool DrawPanelData(PanelData& data, UIProperties& ui)
	{
		bool changed = false;
		if (ui.File("Texture Path", data.TexturePath, ".png,.jpg,.jpeg"))
		{
			changed = true;
		}
		if (ui.Property("Full Screen", data.FullScreen))
		{
			changed = true;
		}
		return changed;
	}

	static bool DrawComboBoxData(ComboBoxData& data, UIProperties& ui)
	{
		bool changed = false;
		if (ui.Property("Label", data.Label))
		{
			changed = true;
		}
		if (!data.Items.empty())
		{
			if (ui.Property("Selected", data.SelectedIndex, PropertyMeta(0, (int)data.Items.size() - 1, 1)))
			{
				changed = true;
			}
		}

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted("Items");
		ImGui::TableSetColumnIndex(1);
		int removeIdx = -1;
		for (int i = 0; i < (int)data.Items.size(); i++)
		{
			ImGui::PushID(i);
			if (ImGui::InputText("##item", &data.Items[i]))
			{
				changed = true;
			}
			ImGui::SameLine();
			if (ImGui::SmallButton(ICON_FA_TRASH))
			{
				removeIdx = i;
				changed = true;
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Remove this item");
			}
			ImGui::PopID();
		}
		if (removeIdx >= 0)
		{
			data.Items.erase(data.Items.begin() + removeIdx);
		}
		if (ImGui::SmallButton(ICON_FA_PLUS " Add Item"))
		{
			data.Items.push_back("");
			changed = true;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Add a new item to the combo box");
		}
		return changed;
	}

	static bool DrawInputTextData(InputTextData& data, UIProperties& ui)
	{
		bool changed = false;
		if (ui.Property("Text", data.Text))
		{
			data.InputBuffer.clear();
			changed = true;
		}
		if (ui.Property("Placeholder", data.Placeholder))
		{
			changed = true;
		}
		if (ui.Property("Max Length", data.MaxLength, PropertyMeta(1, 1024, 1)))
		{
			changed = true;
		}
		if (ui.Property("Multiline", data.Multiline))
		{
			changed = true;
		}
		if (ui.Property("Read Only", data.ReadOnly))
		{
			changed = true;
		}
		if (ui.Property("Password", data.Password))
		{
			changed = true;
		}
		return changed;
	}

	static bool DrawImageButtonData(ImageButtonData& data, UIProperties& ui)
	{
		bool changed = false;
		if (ui.File("Texture Path", data.TexturePath, ".png,.jpg,.jpeg"))
		{
			changed = true;
		}
		if (ui.Property("Label", data.Label))
		{
			changed = true;
		}
		return changed;
	}

	static bool DrawRadioButtonData(RadioButtonData& data, UIProperties& ui)
	{
		bool changed = false;
		if (ui.Property("Label", data.Label))
		{
			changed = true;
		}
		if (!data.Options.empty())
		{
			if (ui.Property("Selected", data.SelectedIndex, PropertyMeta(0, (int)data.Options.size() - 1, 1)))
			{
				changed = true;
			}
		}
		if (ui.Property("Horizontal", data.Horizontal))
		{
			changed = true;
		}
		return changed;
	}

	static bool DrawDragFloatData(DragFloatData& data, UIProperties& ui)
	{
		bool changed = false;
		if (ui.Property("Label", data.Label))
		{
			changed = true;
		}
		if (ui.Property("Value", data.Value, PropertyMeta(data.Min, data.Max, data.Speed)))
		{
			changed = true;
		}
		if (ui.Property("Min", data.Min))
		{
			changed = true;
		}
		if (ui.Property("Max", data.Max))
		{
			changed = true;
		}
		return changed;
	}

	static bool DrawDragIntData(DragIntData& data, UIProperties& ui)
	{
		bool changed = false;
		if (ui.Property("Label", data.Label))
		{
			changed = true;
		}
		if (ui.Property("Value", data.Value, PropertyMeta(data.Min, data.Max, 1)))
		{
			changed = true;
		}
		if (ui.Property("Min", data.Min))
		{
			changed = true;
		}
		if (ui.Property("Max", data.Max))
		{
			changed = true;
		}
		return changed;
	}

	// --- Template Implementations (Moved from Header) ---

	namespace
	{
		std::unordered_map<entt::entity, std::unordered_map<entt::id_type, std::any>> g_InitialStates;
		entt::registry* g_LastRegistry = nullptr;

		void ValidateRegistry(entt::registry* current)
		{
			if (g_LastRegistry != current)
			{
				g_InitialStates.clear();
				g_LastRegistry = current;
			}
		}
	} // namespace

	template <typename T>
	void PropertyEditor::DrawComponentReflection(const std::string& name, const char* icon, Entity entity)
	{
		entt::entity e = (entt::entity)entity;
		entt::id_type compId = entt::type_hash<T>::value();

		ValidateRegistry(&entity.GetRegistry());

		DrawComponentContainer<T>(name, icon, entity, [&](T& comp, Entity ent) {
			UIProperties ui;
			Properties props(ui);

			if constexpr (is_rfl_component<T>::value)
			{
				ReflectFromRfl(comp, props);
			}
			else
			{
				comp.Reflect(props);
			}

			// Finish first: consume the stored baseline before a same-frame
			// activation of another field overwrites it.
			if (ui.HasFinished())
			{
				if (g_InitialStates[e].contains(compId))
				{
					auto oldState = std::any_cast<T>(g_InitialStates[e][compId]);
					auto newState = comp;

					if (s_CommandHistory)
					{
						s_CommandHistory->PushCommand(
							std::make_unique<ModifyComponentCommand<T>>(entity, oldState, newState, "Modify " + name));
					}

					g_InitialStates[e].erase(compId);
				}
			}

			if (ui.HasStarted())
			{
				g_InitialStates[e][compId] = entity.GetComponent<T>();
			}

			return props.HasChanged();
		});
	}

	template <typename T, typename F>
	void PropertyEditor::DrawComponentContainer(const std::string& name, const char* icon, Entity entity, F&& drawer)
	{
		if (entity.HasComponent<T>())
		{
			DrawComponentInternal(
				entt::type_hash<T>::value(), name, icon, entity,
				[&]() {
					auto& component = entity.GetComponent<T>();
					T componentCopy = component;
					if (drawer(componentCopy, entity))
					{
						// Live preview / immediate update
						entity.GetRegistry().template patch<T>(entity,
															   [&componentCopy](T& comp) { comp = componentCopy; });
						return true;
					}
					return false;
				},
				[&]() {
					if (s_CommandHistory)
					{
						s_CommandHistory->PushCommand(std::make_unique<RemoveComponentCommand<T>>(entity));
					}
					else
					{
						entity.RemoveComponent<T>();
					}
				});
		}
	}

	void PropertyEditor::DrawGenericReflection(entt::id_type typeId, const ComponentMetadata& metadata, Entity entity)
	{
		// Pass the real EnTT type hash so that DrawComponentInternal's
		// canDeleteComponent guard correctly identifies protected components
		// (TagComponent, TransformComponent, ControlComponent).
		DrawComponentInternal(
			typeId, metadata.Name, metadata.Icon, entity,
			[&]() {
				UIProperties ui;
				metadata.ReflectInternal(entity, ui, ReflectionMode::UI);
				bool changed = ui.HasChanged();
				if (changed && metadata.NotifyUpdate)
				{
					// Fire registry.patch() so on_update observers (e.g. MarkPrimitiveDirty) run.
					metadata.NotifyUpdate(entity);
				}
				return changed;
			},
			[&]() {
				if (metadata.Remove)
				{
					metadata.Remove(entity);
				}
			});
	}

	template <typename T>
	void PropertyEditor::RegisterComponentImpl(const std::string& name, const char* icon,
											   std::function<void(Entity)> drawUI)
	{
		auto typeId = entt::type_hash<T>::value();

		// Register fresh metadata if the component type doesn't exist yet
		if (!ComponentRegistry::Exists(typeId))
		{
			ComponentMetadata fresh;
			fresh.Name = name;
			fresh.Icon = icon;
			fresh.Category = "Engine";
			fresh.SerializationKey = name + "Component";
			ComponentRegistry::Register(typeId, fresh);
		}

		// Apply editor-specific overrides (undo/redo, custom DrawUI)
		ComponentMetadata override;
		override.Name = name;
		override.Icon = icon;
		override.DrawUI = drawUI;
		override.Add = [](Entity e) {
			if (!e.HasComponent<T>())
			{
				if (s_CommandHistory)
				{
					s_CommandHistory->PushCommand(std::make_unique<AddComponentCommand<T>>(e));
				}
				else
				{
					e.AddComponent<T>();
				}
			}
		};
		override.Remove = [](Entity e) {
			if (s_CommandHistory)
			{
				s_CommandHistory->PushCommand(std::make_unique<RemoveComponentCommand<T>>(e));
			}
			else
			{
				e.RemoveComponent<T>();
			}
		};
		ComponentRegistry::OverrideMetadata(typeId, override);
	}

	template <typename T> void PropertyEditor::Register(const std::string& name, const char* icon)
	{
		RegisterComponentImpl<T>(name, icon, [name, icon](Entity e) { DrawComponentReflection<T>(name, icon, e); });
	}

	template <typename T, typename F>
	void PropertyEditor::RegisterCustom(const std::string& name, F&& drawer, const char* icon)
	{
		RegisterComponentImpl<T>(name, icon, [name, icon, drawer = std::forward<F>(drawer)](Entity e) {
			DrawComponentContainer<T>(name, icon, e, drawer);
		});
	}

	// --- Implementation ---

	void PropertyEditor::SetCommandHistory(CommandHistory* commandHistory)
	{
		if (commandHistory == nullptr)
		{
			CH_CORE_WARN("PropertyEditor::SetCommandHistory — commandHistory is nullptr. Undo/Redo will be disabled.");
		}
		s_CommandHistory = commandHistory;
	}

	void PropertyEditor::Init(CommandHistory* commandHistory)
	{
		s_CommandHistory = commandHistory;
#include "property_editor_drawers.inl"
	}

	void PropertyEditor::DrawComponentInternal(entt::id_type typeId, const std::string& name, const char* icon,
											   Entity entity, std::function<bool()> contentDrawer,
											   std::function<void()> remover)
	{
		const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
												 ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap |
												 ImGuiTreeNodeFlags_FramePadding;

		float contentRegionWidth = ImGui::GetContentRegionAvail().x;

		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{4, 4});

		float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;

		ImGui::PushStyleColor(ImGuiCol_Header, {0.2f, 0.25f, 0.35f, 0.8f});
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, {0.3f, 0.4f, 0.6f, 1.0f});
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {0.25f, 0.35f, 0.5f, 1.0f});

		std::string headerName = (icon ? std::string(icon) + " " : "") + name;

		bool open = ImGui::TreeNodeEx((void*)typeId, treeNodeFlags, "%s", headerName.c_str());

		ImGui::PopStyleColor(3);

		bool removed = false;

		// Components that must never be removed via the Inspector
		const bool canDeleteComponent = typeId != entt::type_hash<TagComponent>::value() &&
										typeId != entt::type_hash<TransformComponent>::value() &&
										typeId != entt::type_hash<ControlComponent>::value();

		if (canDeleteComponent)
		{
			ImGui::PushID((void*)typeId);

			ImGui::SameLine(contentRegionWidth - lineHeight * 0.8f);

			ImGui::PushStyleColor(ImGuiCol_Button, {0, 0, 0, 0});
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.3f, 0.35f, 0.45f, 0.8f});
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.2f, 0.25f, 0.35f, 0.8f});

			if (ImGui::Button(ICON_FA_GEAR, ImVec2{lineHeight, lineHeight}))
			{
				ImGui::OpenPopup("ComponentSettings");
			}

			ImGui::PopStyleColor(3);

			if (ImGui::BeginPopup("ComponentSettings"))
			{
				if (ImGui::MenuItem("Remove Component"))
				{
					remover();
					removed = true;
				}

				ImGui::EndPopup();
			}

			ImGui::PopID();
		}

		ImGui::PopStyleVar();

		if (open)
		{
			if (!removed)
			{
				EditorGUI::BeginPropertyGrid();
				contentDrawer();
				EditorGUI::EndPropertyGrid();
			}

			ImGui::TreePop();
			ImGui::Spacing();
		}
	}

	void PropertyEditor::DrawEntityProperties(Chained::Entity entity)
	{
		static ImGuiTextFilter s_ComponentFilter;
		s_ComponentFilter.Draw(" Search Components...", ImGui::GetContentRegionAvail().x);
		ImGui::Spacing();
		auto& registry = entity.GetRegistry();
		bool isUI = entity.HasComponent<ControlComponent>();

		auto& compRegistry = ComponentRegistry::GetRegistry();

		// 2. Draw components efficiently
		for (auto [id, storage] : registry.storage())
		{
			if (storage.contains(entity) && compRegistry.contains(id))
			{
				auto& metadata = compRegistry.at(id);
				if (!metadata.Visible)
				{
					continue;
				}

				if (!s_ComponentFilter.PassFilter(metadata.Name.c_str()))
				{
					continue;
				}

				// Logic to reduce clutter
				if (isUI && id == entt::type_hash<TransformComponent>::value())
				{
					continue;
				}

				ImGui::PushID((int)id);
				if (metadata.DrawUI)
				{
					metadata.DrawUI(entity);
				}
				else if (metadata.IsReflective && metadata.ReflectInternal)
				{
					DrawGenericReflection(id, metadata, entity);
				}
				ImGui::PopID();
			}
		}
	}

	void PropertyEditor::DrawEntityHeader(Chained::Entity entity)
	{
		if (entity.HasComponent<TagComponent>())
		{
			auto& tag = entity.GetComponent<TagComponent>().Tag;

			// Entity Icon and Label
			ImGui::BeginGroup();
			ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
			ImGui::TextColored({0.4f, 0.6f, 0.9f, 1.0f}, ICON_FA_CUBE " Entity");
			ImGui::PopFont();

			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strncpy(buffer, tag.c_str(), sizeof(buffer) - 1);

			ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 120.0f);
			if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
			{
				tag = std::string(buffer);
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Entity name / tag");
			}
			ImGui::PopItemWidth();

			ImGui::SameLine();
			if (ImGui::Button(ICON_FA_PLUS " Add Component", ImVec2(110, 0)))
			{
				ImGui::OpenPopup("AddComponent");
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Add a new component to this entity");
			}

			DrawAddComponentPopup(entity);
			ImGui::EndGroup();

			ImGui::Spacing();
		}
	}

	void PropertyEditor::DrawAddComponentPopup(Entity entity)
	{
		if (ImGui::BeginPopup("AddComponent"))
		{
			bool isUIEntity = entity.HasComponent<ControlComponent>();
			auto* scene = entity.GetRegistry().ctx().find<Scene*>();
			bool is3DScene = scene && (*scene)->GetSettings().Mode == BackgroundMode::Environment3D;

			// Group components by category
			std::map<std::string, std::vector<const ComponentMetadata*>> categorized;

			for (auto& [id, metadata] : ComponentRegistry::GetRegistry())
			{
				if (!metadata.AllowAdd)
				{
					continue;
				}
				if (metadata.IsWidget && !isUIEntity)
				{
					continue;
				}
				if (is3DScene && (metadata.IsWidget || id == entt::type_hash<ControlComponent>::value()))
				{
					continue;
				}

				auto& registry = entity.GetRegistry();
				auto* storage = registry.storage(id);
				if (storage && storage->contains(entity))
				{
					continue;
				}

				categorized[metadata.Category].push_back(&metadata);
			}

			// Render categorized menus
			for (auto& [category, components] : categorized)
			{
				if (ImGui::BeginMenu(category.c_str()))
				{
					for (const auto* metadata : components)
					{
						std::string label = (metadata->Icon ? std::string(metadata->Icon) + " " : "") + metadata->Name;
						if (ImGui::MenuItem(label.c_str()))
						{
							metadata->Add(entity);
							ImGui::CloseCurrentPopup();
						}
					}
					ImGui::EndMenu();
				}
			}

			ImGui::EndPopup();
		}
	}
} // namespace Chained
