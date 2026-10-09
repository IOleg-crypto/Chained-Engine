#include "ui_data_components.h"
#include "engine/scene/yaml.h"

namespace Chained
{

	void ButtonData::Serialize(YAML::Emitter& out) const
	{
		out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_Button;
		out << YAML::Key << "Label" << YAML::Value << Label;
		out << YAML::Key << "Interactable" << YAML::Value << IsInteractable;
		out << YAML::Key << "Auto Size" << YAML::Value << AutoSize;
	}

	ButtonData ButtonData::Deserialize(YAML::Node widgetNode)
	{
		ButtonData data;
		if (widgetNode["Label"])
		{
			data.Label = widgetNode["Label"].as<std::string>();
		}
		if (widgetNode["Interactable"])
		{
			data.IsInteractable = widgetNode["Interactable"].as<bool>();
		}
		if (widgetNode["Auto Size"])
		{
			data.AutoSize = widgetNode["Auto Size"].as<bool>();
		}
		return data;
	}

	void PanelData::Serialize(YAML::Emitter& out) const
	{
		out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_Panel;
		out << YAML::Key << "Texture Path" << YAML::Value << TexturePath;
		out << YAML::Key << "Full Screen" << YAML::Value << FullScreen;
	}

	PanelData PanelData::Deserialize(YAML::Node widgetNode)
	{
		PanelData data;
		if (widgetNode["Texture Path"])
		{
			data.TexturePath = widgetNode["Texture Path"].as<std::string>();
		}
		if (widgetNode["Full Screen"])
		{
			data.FullScreen = widgetNode["Full Screen"].as<bool>();
		}
		return data;
	}

	void LabelData::Serialize(YAML::Emitter& out) const
	{
		out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_Label;
		out << YAML::Key << "Text" << YAML::Value << Text;
		out << YAML::Key << "Auto Size" << YAML::Value << AutoSize;
	}

	LabelData LabelData::Deserialize(YAML::Node widgetNode)
	{
		LabelData data;
		if (widgetNode["Text"])
		{
			data.Text = widgetNode["Text"].as<std::string>();
		}
		if (widgetNode["Auto Size"])
		{
			data.AutoSize = widgetNode["Auto Size"].as<bool>();
		}
		return data;
	}

	void SliderData::Serialize(YAML::Emitter& out) const
	{
		out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_Slider;
		out << YAML::Key << "Label" << YAML::Value << Label;
		out << YAML::Key << "Value" << YAML::Value << Value;
		out << YAML::Key << "Min" << YAML::Value << Min;
		out << YAML::Key << "Max" << YAML::Value << Max;
	}

	SliderData SliderData::Deserialize(YAML::Node widgetNode)
	{
		SliderData data;
		if (widgetNode["Label"])
		{
			data.Label = widgetNode["Label"].as<std::string>();
		}
		if (widgetNode["Value"])
		{
			data.Value = widgetNode["Value"].as<float>();
		}
		if (widgetNode["Min"])
		{
			data.Min = widgetNode["Min"].as<float>();
		}
		if (widgetNode["Max"])
		{
			data.Max = widgetNode["Max"].as<float>();
		}
		return data;
	}

	void CheckboxData::Serialize(YAML::Emitter& out) const
	{
		out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_Checkbox;
		out << YAML::Key << "Label" << YAML::Value << Label;
		out << YAML::Key << "Checked" << YAML::Value << Checked;
	}

	CheckboxData CheckboxData::Deserialize(YAML::Node widgetNode)
	{
		CheckboxData data;
		if (widgetNode["Label"])
		{
			data.Label = widgetNode["Label"].as<std::string>();
		}
		if (widgetNode["Checked"])
		{
			data.Checked = widgetNode["Checked"].as<bool>();
		}
		return data;
	}

	void InputTextData::Serialize(YAML::Emitter& out) const
	{
		out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_InputText;
		out << YAML::Key << "Label" << YAML::Value << Label;
		out << YAML::Key << "Text" << YAML::Value << Text;
		out << YAML::Key << "Placeholder" << YAML::Value << Placeholder;
		out << YAML::Key << "Max Length" << YAML::Value << MaxLength;
		out << YAML::Key << "Multiline" << YAML::Value << Multiline;
		out << YAML::Key << "Read Only" << YAML::Value << ReadOnly;
		out << YAML::Key << "Password" << YAML::Value << Password;
	}

	InputTextData InputTextData::Deserialize(YAML::Node widgetNode)
	{
		InputTextData data;
		if (widgetNode["Label"])
		{
			data.Label = widgetNode["Label"].as<std::string>();
		}
		if (widgetNode["Text"])
		{
			data.Text = widgetNode["Text"].as<std::string>();
		}
		if (widgetNode["Placeholder"])
		{
			data.Placeholder = widgetNode["Placeholder"].as<std::string>();
		}
		if (widgetNode["Max Length"])
		{
			data.MaxLength = widgetNode["Max Length"].as<int>();
		}
		if (widgetNode["Multiline"])
		{
			data.Multiline = widgetNode["Multiline"].as<bool>();
		}
		if (widgetNode["Read Only"])
		{
			data.ReadOnly = widgetNode["Read Only"].as<bool>();
		}
		if (widgetNode["Password"])
		{
			data.Password = widgetNode["Password"].as<bool>();
		}
		return data;
	}

	void ComboBoxData::Serialize(YAML::Emitter& out) const
	{
		out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_ComboBox;
		out << YAML::Key << "Label" << YAML::Value << Label;
		out << YAML::Key << "Items" << YAML::Value << YAML::BeginSeq;
		for (auto& item : Items)
		{
			out << item;
		}
		out << YAML::EndSeq;
		out << YAML::Key << "Selected Index" << YAML::Value << SelectedIndex;
	}

	ComboBoxData ComboBoxData::Deserialize(YAML::Node widgetNode)
	{
		ComboBoxData data;
		if (widgetNode["Label"])
		{
			data.Label = widgetNode["Label"].as<std::string>();
		}
		if (widgetNode["Items"])
		{
			for (const auto& item : widgetNode["Items"])
			{
				data.Items.push_back(item.as<std::string>());
			}
		}
		if (widgetNode["Selected Index"])
		{
			data.SelectedIndex = widgetNode["Selected Index"].as<int>();
		}
		return data;
	}

	void ProgressBarData::Serialize(YAML::Emitter& out) const
	{
		out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_ProgressBar;
		out << YAML::Key << "Progress" << YAML::Value << Progress;
		out << YAML::Key << "Overlay Text" << YAML::Value << OverlayText;
		out << YAML::Key << "Show Percentage" << YAML::Value << ShowPercentage;
	}

	ProgressBarData ProgressBarData::Deserialize(YAML::Node widgetNode)
	{
		ProgressBarData data;
		if (widgetNode["Progress"])
		{
			data.Progress = widgetNode["Progress"].as<float>();
		}
		if (widgetNode["Overlay Text"])
		{
			data.OverlayText = widgetNode["Overlay Text"].as<std::string>();
		}
		if (widgetNode["Show Percentage"])
		{
			data.ShowPercentage = widgetNode["Show Percentage"].as<bool>();
		}
		return data;
	}

	void ImageData::Serialize(YAML::Emitter& out) const
	{
		out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_Image;
		out << YAML::Key << "Texture Path" << YAML::Value << TexturePath;
		out << YAML::Key << "Tint Color" << YAML::Value << TintColor;
		out << YAML::Key << "Border Color" << YAML::Value << BorderColor;
	}

	ImageData ImageData::Deserialize(YAML::Node widgetNode)
	{
		ImageData data;
		if (widgetNode["Texture Path"])
		{
			data.TexturePath = widgetNode["Texture Path"].as<std::string>();
		}
		if (widgetNode["Tint Color"])
		{
			data.TintColor = widgetNode["Tint Color"].as<Color>();
		}
		if (widgetNode["Border Color"])
		{
			data.BorderColor = widgetNode["Border Color"].as<Color>();
		}
		return data;
	}

	void ImageButtonData::Serialize(YAML::Emitter& out) const
	{
		out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_ImageButton;
		out << YAML::Key << "Texture Path" << YAML::Value << TexturePath;
		out << YAML::Key << "Label" << YAML::Value << Label;
		out << YAML::Key << "Tint Color" << YAML::Value << TintColor;
		out << YAML::Key << "Background Color" << YAML::Value << BackgroundColor;
		out << YAML::Key << "Frame Padding" << YAML::Value << FramePadding;
	}

	ImageButtonData ImageButtonData::Deserialize(YAML::Node widgetNode)
	{
		ImageButtonData data;
		if (widgetNode["Texture Path"])
		{
			data.TexturePath = widgetNode["Texture Path"].as<std::string>();
		}
		if (widgetNode["Label"])
		{
			data.Label = widgetNode["Label"].as<std::string>();
		}
		if (widgetNode["Tint Color"])
		{
			data.TintColor = widgetNode["Tint Color"].as<Color>();
		}
		if (widgetNode["Background Color"])
		{
			data.BackgroundColor = widgetNode["Background Color"].as<Color>();
		}
		if (widgetNode["Frame Padding"])
		{
			data.FramePadding = widgetNode["Frame Padding"].as<int>();
		}
		return data;
	}

	void SeparatorData::Serialize(YAML::Emitter& out) const
	{
		out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_Separator;
		out << YAML::Key << "Thickness" << YAML::Value << Thickness;
		out << YAML::Key << "Line Color" << YAML::Value << LineColor;
	}

	SeparatorData SeparatorData::Deserialize(YAML::Node widgetNode)
	{
		SeparatorData data;
		if (widgetNode["Thickness"])
		{
			data.Thickness = widgetNode["Thickness"].as<float>();
		}
		if (widgetNode["Line Color"])
		{
			data.LineColor = widgetNode["Line Color"].as<Color>();
		}
		return data;
	}

	void RadioButtonData::Serialize(YAML::Emitter& out) const
	{
		out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_RadioButton;
		out << YAML::Key << "Label" << YAML::Value << Label;
		out << YAML::Key << "Options" << YAML::Value << YAML::BeginSeq;
		for (auto& option : Options)
		{
			out << option;
		}
		out << YAML::EndSeq;
		out << YAML::Key << "Selected Index" << YAML::Value << SelectedIndex;
		out << YAML::Key << "Horizontal" << YAML::Value << Horizontal;
	}

	RadioButtonData RadioButtonData::Deserialize(YAML::Node widgetNode)
	{
		RadioButtonData data;
		if (widgetNode["Label"])
		{
			data.Label = widgetNode["Label"].as<std::string>();
		}
		if (widgetNode["Options"])
		{
			for (const auto& option : widgetNode["Options"])
			{
				data.Options.push_back(option.as<std::string>());
			}
		}
		if (widgetNode["Selected Index"])
		{
			data.SelectedIndex = widgetNode["Selected Index"].as<int>();
		}
		if (widgetNode["Horizontal"])
		{
			data.Horizontal = widgetNode["Horizontal"].as<bool>();
		}
		return data;
	}

	void ColorPickerData::Serialize(YAML::Emitter& out) const
	{
		out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_ColorPicker;
		out << YAML::Key << "Label" << YAML::Value << Label;
		out << YAML::Key << "Selected Color" << YAML::Value << SelectedColor;
		out << YAML::Key << "Show Alpha" << YAML::Value << ShowAlpha;
		out << YAML::Key << "Show Picker" << YAML::Value << ShowPicker;
	}

	ColorPickerData ColorPickerData::Deserialize(YAML::Node widgetNode)
	{
		ColorPickerData data;
		if (widgetNode["Label"])
		{
			data.Label = widgetNode["Label"].as<std::string>();
		}
		if (widgetNode["Selected Color"])
		{
			data.SelectedColor = widgetNode["Selected Color"].as<Color>();
		}
		if (widgetNode["Show Alpha"])
		{
			data.ShowAlpha = widgetNode["Show Alpha"].as<bool>();
		}
		if (widgetNode["Show Picker"])
		{
			data.ShowPicker = widgetNode["Show Picker"].as<bool>();
		}
		return data;
	}

	void DragFloatData::Serialize(YAML::Emitter& out) const
	{
		out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_DragFloat;
		out << YAML::Key << "Label" << YAML::Value << Label;
		out << YAML::Key << "Value" << YAML::Value << Value;
		out << YAML::Key << "Speed" << YAML::Value << Speed;
		out << YAML::Key << "Min" << YAML::Value << Min;
		out << YAML::Key << "Max" << YAML::Value << Max;
		out << YAML::Key << "Format" << YAML::Value << Format;
	}

	DragFloatData DragFloatData::Deserialize(YAML::Node widgetNode)
	{
		DragFloatData data;
		if (widgetNode["Label"])
		{
			data.Label = widgetNode["Label"].as<std::string>();
		}
		if (widgetNode["Value"])
		{
			data.Value = widgetNode["Value"].as<float>();
		}
		if (widgetNode["Speed"])
		{
			data.Speed = widgetNode["Speed"].as<float>();
		}
		if (widgetNode["Min"])
		{
			data.Min = widgetNode["Min"].as<float>();
		}
		if (widgetNode["Max"])
		{
			data.Max = widgetNode["Max"].as<float>();
		}
		if (widgetNode["Format"])
		{
			data.Format = widgetNode["Format"].as<std::string>();
		}
		return data;
	}

	void DragIntData::Serialize(YAML::Emitter& out) const
	{
		out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_DragInt;
		out << YAML::Key << "Label" << YAML::Value << Label;
		out << YAML::Key << "Value" << YAML::Value << Value;
		out << YAML::Key << "Speed" << YAML::Value << Speed;
		out << YAML::Key << "Min" << YAML::Value << Min;
		out << YAML::Key << "Max" << YAML::Value << Max;
		out << YAML::Key << "Format" << YAML::Value << Format;
	}

	DragIntData DragIntData::Deserialize(YAML::Node widgetNode)
	{
		DragIntData data;
		if (widgetNode["Label"])
		{
			data.Label = widgetNode["Label"].as<std::string>();
		}
		if (widgetNode["Value"])
		{
			data.Value = widgetNode["Value"].as<int>();
		}
		if (widgetNode["Speed"])
		{
			data.Speed = widgetNode["Speed"].as<float>();
		}
		if (widgetNode["Min"])
		{
			data.Min = widgetNode["Min"].as<int>();
		}
		if (widgetNode["Max"])
		{
			data.Max = widgetNode["Max"].as<int>();
		}
		if (widgetNode["Format"])
		{
			data.Format = widgetNode["Format"].as<std::string>();
		}
		return data;
	}

	void TreeNodeData::Serialize(YAML::Emitter& out) const
	{
		out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_TreeNode;
		out << YAML::Key << "Label" << YAML::Value << Label;
		out << YAML::Key << "Is Open" << YAML::Value << IsOpen;
		out << YAML::Key << "Default Open" << YAML::Value << DefaultOpen;
		out << YAML::Key << "Is Leaf" << YAML::Value << IsLeaf;
	}

	TreeNodeData TreeNodeData::Deserialize(YAML::Node widgetNode)
	{
		TreeNodeData data;
		if (widgetNode["Label"])
		{
			data.Label = widgetNode["Label"].as<std::string>();
		}
		if (widgetNode["Is Open"])
		{
			data.IsOpen = widgetNode["Is Open"].as<bool>();
		}
		if (widgetNode["Default Open"])
		{
			data.DefaultOpen = widgetNode["Default Open"].as<bool>();
		}
		if (widgetNode["Is Leaf"])
		{
			data.IsLeaf = widgetNode["Is Leaf"].as<bool>();
		}
		return data;
	}

	void TabBarData::Serialize(YAML::Emitter& out) const
	{
		out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_TabBar;
		out << YAML::Key << "Label" << YAML::Value << Label;
		out << YAML::Key << "Reorderable" << YAML::Value << Reorderable;
		out << YAML::Key << "Auto Select New Tabs" << YAML::Value << AutoSelectNewTabs;
	}

	TabBarData TabBarData::Deserialize(YAML::Node widgetNode)
	{
		TabBarData data;
		if (widgetNode["Label"])
		{
			data.Label = widgetNode["Label"].as<std::string>();
		}
		if (widgetNode["Reorderable"])
		{
			data.Reorderable = widgetNode["Reorderable"].as<bool>();
		}
		if (widgetNode["Auto Select New Tabs"])
		{
			data.AutoSelectNewTabs = widgetNode["Auto Select New Tabs"].as<bool>();
		}
		return data;
	}

	void TabItemData::Serialize(YAML::Emitter& out) const
	{
		out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_TabItem;
		out << YAML::Key << "Label" << YAML::Value << Label;
		out << YAML::Key << "Is Open" << YAML::Value << IsOpen;
		out << YAML::Key << "Selected" << YAML::Value << Selected;
	}

	TabItemData TabItemData::Deserialize(YAML::Node widgetNode)
	{
		TabItemData data;
		if (widgetNode["Label"])
		{
			data.Label = widgetNode["Label"].as<std::string>();
		}
		if (widgetNode["Is Open"])
		{
			data.IsOpen = widgetNode["Is Open"].as<bool>();
		}
		if (widgetNode["Selected"])
		{
			data.Selected = widgetNode["Selected"].as<bool>();
		}
		return data;
	}

	void CollapsingHeaderData::Serialize(YAML::Emitter& out) const
	{
		out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_CollapsingHeader;
		out << YAML::Key << "Label" << YAML::Value << Label;
		out << YAML::Key << "Is Open" << YAML::Value << IsOpen;
		out << YAML::Key << "Default Open" << YAML::Value << DefaultOpen;
	}

	CollapsingHeaderData CollapsingHeaderData::Deserialize(YAML::Node widgetNode)
	{
		CollapsingHeaderData data;
		if (widgetNode["Label"])
		{
			data.Label = widgetNode["Label"].as<std::string>();
		}
		if (widgetNode["Is Open"])
		{
			data.IsOpen = widgetNode["Is Open"].as<bool>();
		}
		if (widgetNode["Default Open"])
		{
			data.DefaultOpen = widgetNode["Default Open"].as<bool>();
		}
		return data;
	}

	void PlotData::Serialize(YAML::Emitter& out) const
	{
		if (Mode == PlotMode::Lines)
		{
			out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_PlotLines;
		}
		else
		{
			out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_PlotHistogram;
		}
		out << YAML::Key << "Label" << YAML::Value << Label;
		out << YAML::Key << "Values" << YAML::Value << YAML::BeginSeq;
		for (auto value : Values)
		{
			out << value;
		}
		out << YAML::EndSeq;
		out << YAML::Key << "Overlay Text" << YAML::Value << OverlayText;
		out << YAML::Key << "Scale Min" << YAML::Value << ScaleMin;
		out << YAML::Key << "Scale Max" << YAML::Value << ScaleMax;
	}

	PlotData PlotData::Deserialize(YAML::Node widgetNode)
	{
		PlotData data;
		if (widgetNode["Label"])
		{
			data.Label = widgetNode["Label"].as<std::string>();
		}
		if (widgetNode["Values"])
		{
			for (const auto& value : widgetNode["Values"])
			{
				data.Values.push_back(value.as<float>());
			}
		}
		if (widgetNode["Overlay Text"])
		{
			data.OverlayText = widgetNode["Overlay Text"].as<std::string>();
		}
		if (widgetNode["Scale Min"])
		{
			data.ScaleMin = widgetNode["Scale Min"].as<float>();
		}
		if (widgetNode["Scale Max"])
		{
			data.ScaleMax = widgetNode["Scale Max"].as<float>();
		}
		return data;
	}

	void VerticalLayoutGroupData::Serialize(YAML::Emitter& out) const
	{
		out << YAML::Key << "Widget Type" << YAML::Value << WidgetType_VerticalLayoutGroup;
		out << YAML::Key << "Spacing" << YAML::Value << Spacing;
		out << YAML::Key << "Padding" << YAML::Value << Padding;
	}

	VerticalLayoutGroupData VerticalLayoutGroupData::Deserialize(YAML::Node widgetNode)
	{
		VerticalLayoutGroupData data;
		if (widgetNode["Spacing"])
		{
			data.Spacing = widgetNode["Spacing"].as<float>();
		}
		if (widgetNode["Padding"])
		{
			data.Padding = widgetNode["Padding"].as<glm::vec2>();
		}
		return data;
	}

	ControlData DeserializeControlData(int widgetType, YAML::Node widgetNode)
	{
		if (widgetType == WidgetType_None)
		{
			if (widgetNode["ButtonControl"])
			{
				return ButtonData::Deserialize(widgetNode["ButtonControl"]);
			}
			if (widgetNode["InputTextControl"])
			{
				return InputTextData::Deserialize(widgetNode["InputTextControl"]);
			}
			if (widgetNode["LabelControl"])
			{
				return LabelData::Deserialize(widgetNode["LabelControl"]);
			}
			if (widgetNode["PanelControl"])
			{
				return PanelData::Deserialize(widgetNode["PanelControl"]);
			}
			if (widgetNode["SliderControl"])
			{
				return SliderData::Deserialize(widgetNode["SliderControl"]);
			}
			if (widgetNode["CheckboxControl"])
			{
				return CheckboxData::Deserialize(widgetNode["CheckboxControl"]);
			}
			if (widgetNode["ComboBoxControl"])
			{
				return ComboBoxData::Deserialize(widgetNode["ComboBoxControl"]);
			}
			if (widgetNode["ImageControl"])
			{
				return ImageData::Deserialize(widgetNode["ImageControl"]);
			}
			if (widgetNode["ImageButtonControl"])
			{
				return ImageButtonData::Deserialize(widgetNode["ImageButtonControl"]);
			}
			if (widgetNode["SeparatorControl"])
			{
				return SeparatorData::Deserialize(widgetNode["SeparatorControl"]);
			}
			if (widgetNode["RadioButtonControl"])
			{
				return RadioButtonData::Deserialize(widgetNode["RadioButtonControl"]);
			}
			if (widgetNode["ColorPickerControl"])
			{
				return ColorPickerData::Deserialize(widgetNode["ColorPickerControl"]);
			}
			if (widgetNode["DragFloatControl"])
			{
				return DragFloatData::Deserialize(widgetNode["DragFloatControl"]);
			}
			if (widgetNode["DragIntControl"])
			{
				return DragIntData::Deserialize(widgetNode["DragIntControl"]);
			}
			if (widgetNode["TreeNodeControl"])
			{
				return TreeNodeData::Deserialize(widgetNode["TreeNodeControl"]);
			}
			if (widgetNode["TabBarControl"])
			{
				return TabBarData::Deserialize(widgetNode["TabBarControl"]);
			}
			if (widgetNode["TabItemControl"])
			{
				return TabItemData::Deserialize(widgetNode["TabItemControl"]);
			}
			if (widgetNode["CollapsingHeaderControl"])
			{
				return CollapsingHeaderData::Deserialize(widgetNode["CollapsingHeaderControl"]);
			}
			if (widgetNode["VerticalLayoutGroupControl"])
			{
				return VerticalLayoutGroupData::Deserialize(widgetNode["VerticalLayoutGroupControl"]);
			}

			// Explicit widget type is required here. Falling back to a generic panel
			// causes unrelated widgets to reuse the wrong payload and look identical.
			return std::monostate{};
		}

		auto getNode = [&widgetNode](const char* subkey) -> YAML::Node {
			return widgetNode[subkey] ? widgetNode[subkey] : widgetNode;
		};

		switch (widgetType)
		{
		case WidgetType_Button:
			return ButtonData::Deserialize(getNode("ButtonControl"));
		case WidgetType_Label:
			return LabelData::Deserialize(getNode("LabelControl"));
		case WidgetType_Slider:
			return SliderData::Deserialize(getNode("SliderControl"));
		case WidgetType_Checkbox:
			return CheckboxData::Deserialize(getNode("CheckboxControl"));
		case WidgetType_ProgressBar:
			return ProgressBarData::Deserialize(getNode("ProgressBarControl"));
		case WidgetType_Panel:
			return PanelData::Deserialize(getNode("PanelControl"));
		case WidgetType_Image:
			return ImageData::Deserialize(getNode("ImageControl"));
		case WidgetType_ComboBox:
			return ComboBoxData::Deserialize(getNode("ComboBoxControl"));
		case WidgetType_ImageButton:
			return ImageButtonData::Deserialize(getNode("ImageButtonControl"));
		case WidgetType_InputText:
			return InputTextData::Deserialize(getNode("InputTextControl"));
		case WidgetType_Separator:
			return SeparatorData::Deserialize(getNode("SeparatorControl"));
		case WidgetType_RadioButton:
			return RadioButtonData::Deserialize(getNode("RadioButtonControl"));
		case WidgetType_ColorPicker:
			return ColorPickerData::Deserialize(getNode("ColorPickerControl"));
		case WidgetType_DragFloat:
			return DragFloatData::Deserialize(getNode("DragFloatControl"));
		case WidgetType_DragInt:
			return DragIntData::Deserialize(getNode("DragIntControl"));
		case WidgetType_TreeNode:
			return TreeNodeData::Deserialize(getNode("TreeNodeControl"));
		case WidgetType_TabBar:
			return TabBarData::Deserialize(getNode("TabBarControl"));
		case WidgetType_TabItem:
			return TabItemData::Deserialize(getNode("TabItemControl"));
		case WidgetType_CollapsingHeader:
			return CollapsingHeaderData::Deserialize(getNode("CollapsingHeaderControl"));
		case WidgetType_PlotLines: {
			auto data = PlotData::Deserialize(getNode("PlotControl"));
			data.Mode = PlotMode::Lines;
			return data;
		}
		case WidgetType_PlotHistogram: {
			auto data = PlotData::Deserialize(getNode("PlotControl"));
			data.Mode = PlotMode::Histogram;
			return data;
		}
		case WidgetType_VerticalLayoutGroup:
			return VerticalLayoutGroupData::Deserialize(getNode("VerticalLayoutGroupControl"));
		default:
			return std::monostate{};
		}
	}

	ControlData CreateDefaultWidgetData(WidgetType type)
	{
		switch (type)
		{
		case WidgetType_Button:
			return ButtonData{};
		case WidgetType_Panel:
			return PanelData{};
		case WidgetType_Label:
			return LabelData{};
		case WidgetType_Slider:
			return SliderData{};
		case WidgetType_Checkbox:
			return CheckboxData{};
		case WidgetType_InputText:
			return InputTextData{};
		case WidgetType_ComboBox:
			return ComboBoxData{};
		case WidgetType_ProgressBar:
			return ProgressBarData{};
		case WidgetType_Image:
			return ImageData{};
		case WidgetType_ImageButton:
			return ImageButtonData{};
		case WidgetType_Separator:
			return SeparatorData{};
		case WidgetType_RadioButton:
			return RadioButtonData{};
		case WidgetType_ColorPicker:
			return ColorPickerData{};
		case WidgetType_DragFloat:
			return DragFloatData{};
		case WidgetType_DragInt:
			return DragIntData{};
		case WidgetType_TreeNode:
			return TreeNodeData{};
		case WidgetType_TabBar:
			return TabBarData{};
		case WidgetType_TabItem:
			return TabItemData{};
		case WidgetType_CollapsingHeader:
			return CollapsingHeaderData{};
		case WidgetType_PlotLines: {
			PlotData d;
			d.Mode = PlotMode::Lines;
			return d;
		}
		case WidgetType_PlotHistogram: {
			PlotData d;
			d.Mode = PlotMode::Histogram;
			return d;
		}
		case WidgetType_VerticalLayoutGroup:
			return VerticalLayoutGroupData{};
		case WidgetType_None:
		default:
			return std::monostate{};
		}
	}

	const char* WidgetTypeName(WidgetType type)
	{
		switch (type)
		{
		case WidgetType_Button:
			return "Button";
		case WidgetType_Panel:
			return "Panel";
		case WidgetType_Label:
			return "Label";
		case WidgetType_Slider:
			return "Slider";
		case WidgetType_Checkbox:
			return "Checkbox";
		case WidgetType_InputText:
			return "InputText";
		case WidgetType_ComboBox:
			return "ComboBox";
		case WidgetType_ProgressBar:
			return "ProgressBar";
		case WidgetType_Image:
			return "Image";
		case WidgetType_ImageButton:
			return "ImageButton";
		case WidgetType_Separator:
			return "Separator";
		case WidgetType_RadioButton:
			return "RadioButton";
		case WidgetType_ColorPicker:
			return "ColorPicker";
		case WidgetType_DragFloat:
			return "DragFloat";
		case WidgetType_DragInt:
			return "DragInt";
		case WidgetType_TreeNode:
			return "TreeNode";
		case WidgetType_TabBar:
			return "TabBar";
		case WidgetType_TabItem:
			return "TabItem";
		case WidgetType_CollapsingHeader:
			return "CollapsingHeader";
		case WidgetType_PlotLines:
			return "PlotLines";
		case WidgetType_PlotHistogram:
			return "PlotHistogram";
		case WidgetType_VerticalLayoutGroup:
			return "VerticalLayoutGroup";
		case WidgetType_None:
		default:
			return "None";
		}
	}

} // namespace Chained
