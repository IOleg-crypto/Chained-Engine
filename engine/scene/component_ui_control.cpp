#include "component_registry.h"
#include "engine/reflection/reflection_rfl_impl.h"
#include "components/ui/control_component.h"
#include "engine/ui/ui_data_components.h"
#include "engine/ui/ui_style.h"
#include "engine/scene/yaml.h"
#include "thirdparty/IconsFontAwesome6.h"

namespace Chained
{
	template <typename T> static void ReadField(YAML::Node node, const char* key, T& out)
	{
		if (node[key])
		{
			out = node[key].as<T>();
		}
	}

	void RegisterUIControlComponent()
	{
		ComponentMetadata metadata;
		metadata.Name = "Widget";
		metadata.SerializationKey = "WidgetComponent";
		metadata.Icon = ICON_FA_WINDOW_RESTORE;
		metadata.Category = "UI";

		metadata.Has = [](Entity e) { return e.HasComponent<UIControlComponent>(); };
		metadata.GetAll = [](class Scene* s) {
			std::vector<uint64_t> ids;
			for (auto ent : s->GetRegistry().view<UIControlComponent>())
			{
				ids.push_back(static_cast<uint64_t>(ent));
			}
			return ids;
		};
		metadata.Add = [](Entity e) {
			if (!e.HasComponent<UIControlComponent>())
			{
				e.AddComponent<UIControlComponent>();
			}
		};
		metadata.Remove = [](Entity e) {
			if (e.HasComponent<UIControlComponent>())
			{
				e.RemoveComponent<UIControlComponent>();
			}
		};
		metadata.Copy = [](Entity src, Entity dst) {
			if (src.HasComponent<UIControlComponent>())
			{
				dst.AddOrReplaceComponent<UIControlComponent>(src.GetComponent<UIControlComponent>());
			}
		};

		// Custom serialize — delegates to per-type Serialize methods
		metadata.Serialize = [](YAML::Emitter& out, Entity entity) {
			if (!entity.HasComponent<UIControlComponent>())
			{
				return;
			}
			auto& widgetComp = entity.GetComponent<UIControlComponent>();

			out << YAML::Key << "WidgetComponent" << YAML::Value << YAML::BeginMap;

			// Box Style
			out << YAML::Key << "Box Style" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "BG Color" << YAML::Value << widgetComp.BoxStyle.BackgroundColor;
			out << YAML::Key << "Hover Color" << YAML::Value << widgetComp.BoxStyle.HoverColor;
			out << YAML::Key << "Pressed Color" << YAML::Value << widgetComp.BoxStyle.PressedColor;
			out << YAML::Key << "Rounding" << YAML::Value << widgetComp.BoxStyle.Rounding;
			out << YAML::Key << "Border Size" << YAML::Value << widgetComp.BoxStyle.BorderSize;
			out << YAML::Key << "Border Color" << YAML::Value << widgetComp.BoxStyle.BorderColor;
			out << YAML::Key << "Gradient" << YAML::Value << widgetComp.BoxStyle.UseGradient;
			out << YAML::Key << "Gradient Color" << YAML::Value << widgetComp.BoxStyle.GradientColor;
			out << YAML::Key << "Padding" << YAML::Value << widgetComp.BoxStyle.Padding;
			out << YAML::Key << "Hover Scale" << YAML::Value << widgetComp.BoxStyle.HoverScale;
			out << YAML::Key << "Pressed Scale" << YAML::Value << widgetComp.BoxStyle.PressedScale;
			out << YAML::Key << "Transition Speed" << YAML::Value << widgetComp.BoxStyle.TransitionSpeed;
			out << YAML::EndMap;

			// Text Style
			out << YAML::Key << "Text Style" << YAML::Value << YAML::BeginMap;
			out << YAML::Key << "Font Name" << YAML::Value << widgetComp.TextStyle.FontName;
			out << YAML::Key << "Font Size" << YAML::Value << widgetComp.TextStyle.FontSize;
			out << YAML::Key << "Text Color" << YAML::Value << widgetComp.TextStyle.TextColor;
			out << YAML::Key << "Shadow" << YAML::Value << widgetComp.TextStyle.Shadow;
			out << YAML::Key << "Shadow Offset" << YAML::Value << widgetComp.TextStyle.ShadowOffset;
			out << YAML::Key << "Shadow Color" << YAML::Value << widgetComp.TextStyle.ShadowColor;
			out << YAML::Key << "Letter Spacing" << YAML::Value << widgetComp.TextStyle.LetterSpacing;
			out << YAML::Key << "Line Height" << YAML::Value << widgetComp.TextStyle.LineHeight;
			out << YAML::Key << "H Align" << YAML::Value << static_cast<int>(widgetComp.TextStyle.Horizontal);
			out << YAML::Key << "V Align" << YAML::Value << static_cast<int>(widgetComp.TextStyle.Vertical);
			out << YAML::EndMap;

			// Widget data — delegate to per-type Serialize
			std::visit(
				[&](auto& data) {
					using DataType = std::decay_t<decltype(data)>;
					if constexpr (!std::is_same_v<DataType, std::monostate>)
					{
						data.Serialize(out);
					}
				},
				widgetComp.Data);

			out << YAML::EndMap;
		};

		// Custom deserialize — delegates to DeserializeControlData
		metadata.Deserialize = [](Entity entity, YAML::Node node) {
			if (!node["WidgetComponent"])
			{
				return;
			}
			auto widgetNode = node["WidgetComponent"];
			auto& widgetComp = entity.AddComponent<UIControlComponent>();

			// Box Style
			if (auto boxStyleNode = widgetNode["Box Style"])
			{
				ReadField(boxStyleNode, "BG Color", widgetComp.BoxStyle.BackgroundColor);
				ReadField(boxStyleNode, "Hover Color", widgetComp.BoxStyle.HoverColor);
				ReadField(boxStyleNode, "Pressed Color", widgetComp.BoxStyle.PressedColor);
				ReadField(boxStyleNode, "Rounding", widgetComp.BoxStyle.Rounding);
				ReadField(boxStyleNode, "Border Size", widgetComp.BoxStyle.BorderSize);
				ReadField(boxStyleNode, "Border Color", widgetComp.BoxStyle.BorderColor);
				ReadField(boxStyleNode, "Gradient", widgetComp.BoxStyle.UseGradient);
				ReadField(boxStyleNode, "Gradient Color", widgetComp.BoxStyle.GradientColor);
				ReadField(boxStyleNode, "Padding", widgetComp.BoxStyle.Padding);
				ReadField(boxStyleNode, "Hover Scale", widgetComp.BoxStyle.HoverScale);
				ReadField(boxStyleNode, "Pressed Scale", widgetComp.BoxStyle.PressedScale);
				ReadField(boxStyleNode, "Transition Speed", widgetComp.BoxStyle.TransitionSpeed);
			}

			// Text Style
			if (auto textStyleNode = widgetNode["Text Style"])
			{
				ReadField(textStyleNode, "Font Name", widgetComp.TextStyle.FontName);
				ReadField(textStyleNode, "Font Size", widgetComp.TextStyle.FontSize);
				ReadField(textStyleNode, "Text Color", widgetComp.TextStyle.TextColor);
				ReadField(textStyleNode, "Shadow", widgetComp.TextStyle.Shadow);
				ReadField(textStyleNode, "Shadow Offset", widgetComp.TextStyle.ShadowOffset);
				ReadField(textStyleNode, "Shadow Color", widgetComp.TextStyle.ShadowColor);
				ReadField(textStyleNode, "Letter Spacing", widgetComp.TextStyle.LetterSpacing);
				ReadField(textStyleNode, "Line Height", widgetComp.TextStyle.LineHeight);
				{
					int h = static_cast<int>(widgetComp.TextStyle.Horizontal);
					ReadField(textStyleNode, "H Align", h);
					widgetComp.TextStyle.Horizontal = static_cast<HorizontalAlignment>(h);
				}
				{
					int v = static_cast<int>(widgetComp.TextStyle.Vertical);
					ReadField(textStyleNode, "V Align", v);
					widgetComp.TextStyle.Vertical = static_cast<VerticalAlignment>(v);
				}
			}

			// Widget data — delegate to DeserializeControlData
			int widgetType = widgetNode["Widget Type"] ? widgetNode["Widget Type"].as<int>() : 0;
			widgetComp.Data = DeserializeControlData(widgetType, widgetNode);
		};

		// Keep ReflectInternal for editor UI (property panel)
		metadata.IsReflective = true;
		metadata.ReflectInternal = [](Entity e, IPropertyArchiveBase& archive, ReflectionMode mode) {
			if (mode == ReflectionMode::Deserialize)
			{
				auto& comp = e.AddOrReplaceComponent<UIControlComponent>();
				GenericProperties props(archive);
				ReflectFromRfl(comp, props);
			}
			else if (e.HasComponent<UIControlComponent>())
			{
				GenericProperties props(archive);
				auto& comp = e.GetComponent<UIControlComponent>();
				ReflectFromRfl(comp, props);
			}
		};

		ComponentRegistry::Register(entt::type_hash<UIControlComponent>::value(), metadata);
	}
} // namespace Chained
