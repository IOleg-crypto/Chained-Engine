#include "script_glue_entity.h"
#include "engine/scene/component_registry.h"
#include "engine/scene/components.h"
#include "engine/scene/entity.h"

namespace Chained
{

	// ── Entity Registry ──────────────────────────────────────────────────

	void Entity_AddComponent(uint64_t entityID, const Coral::UCChar* componentName)
	{
		Entity entity = GetEntity(entityID);
		if (!entity)
		{
			return;
		}
		std::string name = ch_u16_to_string(componentName);
		for (const auto& [id, metadata] : ComponentRegistry::GetRegistry())
		{
			if (metadata.Name == name || metadata.SerializationKey == name)
			{
				if (metadata.Add)
				{
					metadata.Add(entity);
				}
				return;
			}
		}
	}
	int Entity_FindAllWithComponent(const Coral::UCChar* componentName, uint64_t* outBuf, int bufSize)
	{
		Scene* scene = GetActiveScene();
		if (!scene)
		{
			return 0;
		}
		std::string name = ch_u16_to_string(componentName);
		for (const auto& [id, metadata] : ComponentRegistry::GetRegistry())
		{
			if (metadata.Name == name || metadata.SerializationKey == name)
			{
				if (metadata.GetAll)
				{
					auto ids = metadata.GetAll(scene);
					int totalCount = static_cast<int>(ids.size());
					if (outBuf && bufSize > 0)
					{
						int toCopy = std::min(totalCount, bufSize);
						for (int i = 0; i < toCopy; ++i)
						{
							outBuf[i] = ids[i];
						}
					}
					return totalCount;
				}
				return 0;
			}
		}
		return 0;
	}
	uint8_t Entity_HasComponent(uint64_t entityID, const Coral::UCChar* componentName)
	{
		Entity entity = GetEntity(entityID);
		if (!entity)
		{
			return false;
		}
		std::string name = ch_u16_to_string(componentName);

		if (name.find("Control") != std::string::npos || name.find("Group") != std::string::npos)
		{
			if (!entity.HasComponent<UIControlComponent>())
			{
				return false;
			}
			auto& widget = entity.GetComponent<UIControlComponent>();

			static const std::pair<const char*, std::function<bool(const ControlData&)>> widgetChecks[] = {
				{"ButtonControl", [](const ControlData& d) { return std::holds_alternative<ButtonData>(d); }},
				{"PanelControl", [](const ControlData& d) { return std::holds_alternative<PanelData>(d); }},
				{"LabelControl", [](const ControlData& d) { return std::holds_alternative<LabelData>(d); }},
				{"ImageControl", [](const ControlData& d) { return std::holds_alternative<ImageData>(d); }},
				{"CheckboxControl", [](const ControlData& d) { return std::holds_alternative<CheckboxData>(d); }},
				{"ComboBoxControl", [](const ControlData& d) { return std::holds_alternative<ComboBoxData>(d); }},
				{"SliderControl", [](const ControlData& d) { return std::holds_alternative<SliderData>(d); }},
				{"ProgressBarControl", [](const ControlData& d) { return std::holds_alternative<ProgressBarData>(d); }},
				{"InputTextControl", [](const ControlData& d) { return std::holds_alternative<InputTextData>(d); }},
				{"ImageButtonControl", [](const ControlData& d) { return std::holds_alternative<ImageButtonData>(d); }},
				{"SeparatorControl", [](const ControlData& d) { return std::holds_alternative<SeparatorData>(d); }},
				{"RadioButtonControl", [](const ControlData& d) { return std::holds_alternative<RadioButtonData>(d); }},
				{"ColorPickerControl", [](const ControlData& d) { return std::holds_alternative<ColorPickerData>(d); }},
				{"DragFloatControl", [](const ControlData& d) { return std::holds_alternative<DragFloatData>(d); }},
				{"DragIntControl", [](const ControlData& d) { return std::holds_alternative<DragIntData>(d); }},
				{"VerticalLayoutGroup",
				 [](const ControlData& d) { return std::holds_alternative<VerticalLayoutGroupData>(d); }},
			};

			for (const auto& [widgetName, check] : widgetChecks)
			{
				if (name == widgetName && check(widget.Data))
				{
					return true;
				}
			}
			return false;
		}

		for (const auto& [id, metadata] : ComponentRegistry::GetRegistry())
		{
			if (metadata.Name == name || metadata.SerializationKey == name)
			{
				if (metadata.Has)
				{
					return metadata.Has(entity);
				}
			}
		}
		return false;
	}

} // namespace Chained
