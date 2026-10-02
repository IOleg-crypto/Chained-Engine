#ifndef SCRIPT_GLUE_UI_H
#define SCRIPT_GLUE_UI_H
#include "script_glue_internal.h"
#include "engine/scene/components/ui/control_component.h"
#include <variant>
#include <imgui_internal.h>

namespace Chained
{

	// ── UI Controls ───────────────────────────────────────────────────────
	CH_SCRIPT_FUNC uint8_t ButtonControl_IsClicked(uint64_t entityID);
	CH_SCRIPT_FUNC uint8_t ButtonControl_IsDown(uint64_t entityID);
	CH_SCRIPT_FUNC const Coral::UCChar* ButtonControl_GetLabel(uint64_t entityID);
	CH_SCRIPT_FUNC void ButtonControl_SetLabel(uint64_t entityID, const Coral::UCChar* label);

	CH_SCRIPT_FUNC uint8_t CheckboxControl_GetChecked(uint64_t entityID);
	CH_SCRIPT_FUNC void CheckboxControl_SetChecked(uint64_t entityID, uint8_t checked);

	CH_SCRIPT_FUNC const Coral::UCChar* LabelControl_GetText(uint64_t entityID);
	CH_SCRIPT_FUNC void LabelControl_SetText(uint64_t entityID, const Coral::UCChar* text);

	CH_SCRIPT_FUNC const Coral::UCChar* SliderControl_GetLabel(uint64_t entityID);
	CH_SCRIPT_FUNC void SliderControl_SetLabel(uint64_t entityID, const Coral::UCChar* label);
	CH_SCRIPT_FUNC float SliderControl_GetValue(uint64_t entityID);
	CH_SCRIPT_FUNC void SliderControl_SetValue(uint64_t entityID, float value);
	CH_SCRIPT_FUNC float SliderControl_GetMin(uint64_t entityID);
	CH_SCRIPT_FUNC void SliderControl_SetMin(uint64_t entityID, float minVal);
	CH_SCRIPT_FUNC float SliderControl_GetMax(uint64_t entityID);
	CH_SCRIPT_FUNC void SliderControl_SetMax(uint64_t entityID, float maxVal);

	CH_SCRIPT_FUNC float ProgressBarControl_GetProgress(uint64_t entityID);
	CH_SCRIPT_FUNC void ProgressBarControl_SetProgress(uint64_t entityID, float progress);
	CH_SCRIPT_FUNC const Coral::UCChar* ProgressBarControl_GetOverlayText(uint64_t entityID);
	CH_SCRIPT_FUNC void ProgressBarControl_SetOverlayText(uint64_t entityID, const Coral::UCChar* text);
	CH_SCRIPT_FUNC uint8_t ProgressBarControl_GetShowPercentage(uint64_t entityID);
	CH_SCRIPT_FUNC void ProgressBarControl_SetShowPercentage(uint64_t entityID, uint8_t show);

	CH_SCRIPT_FUNC uint8_t WidgetControl_GetActive(uint64_t entityID);
	CH_SCRIPT_FUNC void WidgetControl_SetActive(uint64_t entityID, uint8_t active);
	CH_SCRIPT_FUNC const Coral::UCChar* WidgetControl_GetTextColor(uint64_t entityID);
	CH_SCRIPT_FUNC void WidgetControl_SetTextColorRGBA(uint64_t entityID, int r, int g, int b, int a);

	CH_SCRIPT_FUNC int ComboBoxControl_GetSelectedIndex(uint64_t entityID);

	CH_SCRIPT_FUNC void ComboBoxControl_SetSelectedIndex(uint64_t entityID, int index);

	CH_SCRIPT_FUNC void ComboBoxControl_AddItem(uint64_t entityID, const Coral::UCChar* item);

	CH_SCRIPT_FUNC void ComboBoxControl_ClearItems(uint64_t entityID);

	CH_SCRIPT_FUNC int ComboBoxControl_GetItemCount(uint64_t entityID);

	CH_SCRIPT_FUNC const Coral::UCChar* ComboBoxControl_GetItem(uint64_t entityID, int index);

	CH_SCRIPT_FUNC const Coral::UCChar* InputTextControl_GetText(uint64_t entityID);
	CH_SCRIPT_FUNC void InputTextControl_SetText(uint64_t entityID, const Coral::UCChar* text);
	CH_SCRIPT_FUNC uint8_t InputTextControl_HasChanged(uint64_t entityID);
	CH_SCRIPT_FUNC void InputTextControl_ClearChanged(uint64_t entityID);

	CH_SCRIPT_FUNC void UI_Text(const Coral::UCChar* text);
	CH_SCRIPT_FUNC void UI_TextColored(const Coral::UCChar* text, float r, float g, float b, float a);
	CH_SCRIPT_FUNC uint8_t UI_Button(const Coral::UCChar* label);
	CH_SCRIPT_FUNC void UI_BeginWindow(const Coral::UCChar* title, float x, float y, float w, float h, float bgAlpha);
	CH_SCRIPT_FUNC void UI_EndWindow();
	CH_SCRIPT_FUNC void UI_BeginChild(const Coral::UCChar* strId, float w, float h, uint8_t border);
	CH_SCRIPT_FUNC void UI_EndChild();
	CH_SCRIPT_FUNC void UI_Separator();
	CH_SCRIPT_FUNC void UI_SameLine(float offsetFromStartX, float spacing);
	CH_SCRIPT_FUNC void UI_SetNextItemWidth(float itemWidth);
	CH_SCRIPT_FUNC uint8_t UI_InputText(const Coral::UCChar* label, Coral::UCChar* buffer, int maxLen);
	CH_SCRIPT_FUNC void UI_SetKeyboardFocusHere();
	CH_SCRIPT_FUNC void UI_SetScrollHereY(float centerYRatio);
	CH_SCRIPT_FUNC void UI_GetDisplaySize(float* outW, float* outH);
	CH_SCRIPT_FUNC void UI_PushStyleColor(int32_t colIdx, float r, float g, float b, float a);
	CH_SCRIPT_FUNC void UI_PopStyleColor(int32_t count);
	CH_SCRIPT_FUNC void UI_PushStyleVarFloat(int32_t varIdx, float val);
	CH_SCRIPT_FUNC void UI_PushStyleVarVec2(int32_t varIdx, float x, float y);
	CH_SCRIPT_FUNC void UI_PopStyleVar(int32_t count);
	CH_SCRIPT_FUNC void UI_Dummy(float w, float h);
	CH_SCRIPT_FUNC uint8_t UI_IsItemHovered();
	CH_SCRIPT_FUNC void UI_SetWindowFontScale(float scale);
	CH_SCRIPT_FUNC uint8_t UI_SliderFloat(const Coral::UCChar* label, float* v, float v_min, float v_max);
	CH_SCRIPT_FUNC uint8_t UI_Checkbox(const Coral::UCChar* label, uint8_t* v);

} // namespace Chained
#endif