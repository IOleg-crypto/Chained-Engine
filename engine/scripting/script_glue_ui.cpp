#include "script_glue_ui.h"

namespace Chained
{
	uint8_t ButtonControl_IsClicked(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (widget.PressedThisFrame)
			{
				widget.PressedThisFrame = false; // consume on read
				return true;
			}
		}
		return false;
	}

	uint8_t ButtonControl_IsDown(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		return entity && entity.HasComponent<UIControlComponent>() ? entity.GetComponent<UIControlComponent>().IsDown
																   : false;
	}

	const Coral::UCChar* ButtonControl_GetLabel(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<ButtonData>(widget.Data))
			{
				return GlueStringPool::ReturnString(std::get<ButtonData>(widget.Data).Label);
			}
		}
		return nullptr;
	}
	void ButtonControl_SetLabel(uint64_t entityID, const Coral::UCChar* label)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>() && label)
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<ButtonData>(widget.Data))
			{
				std::get<ButtonData>(widget.Data).Label = ch_u16_to_string(label);
			}
		}
	}

	void CheckboxControl_SetChecked(uint64_t entityID, uint8_t checked)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<CheckboxData>(widget.Data))
			{
				std::get<CheckboxData>(widget.Data).Checked = checked;
			}
		}
	}

	const Coral::UCChar* LabelControl_GetText(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<LabelData>(widget.Data))
			{
				return GlueStringPool::ReturnString(std::get<LabelData>(widget.Data).Text);
			}
		}
		return nullptr;
	}
	void LabelControl_SetText(uint64_t entityID, const Coral::UCChar* text)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>() && text)
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<LabelData>(widget.Data))
			{
				std::get<LabelData>(widget.Data).Text = ch_u16_to_string(text);
			}
		}
	}

	const Coral::UCChar* SliderControl_GetLabel(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<SliderData>(widget.Data))
			{
				return GlueStringPool::ReturnString(std::get<SliderData>(widget.Data).Label);
			}
		}
		return nullptr;
	}
	void SliderControl_SetLabel(uint64_t entityID, const Coral::UCChar* label)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>() && label)
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<SliderData>(widget.Data))
			{
				std::get<SliderData>(widget.Data).Label = ch_u16_to_string(label);
			}
		}
	}
	float SliderControl_GetValue(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<SliderData>(widget.Data))
			{
				return std::get<SliderData>(widget.Data).Value;
			}
		}
		return 0.0f;
	}
	void SliderControl_SetValue(uint64_t entityID, float value)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<SliderData>(widget.Data))
			{
				std::get<SliderData>(widget.Data).Value = value;
			}
		}
	}
	float SliderControl_GetMin(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<SliderData>(widget.Data))
			{
				return std::get<SliderData>(widget.Data).Min;
			}
		}
		return 0.0f;
	}
	void SliderControl_SetMin(uint64_t entityID, float minVal)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<SliderData>(widget.Data))
			{
				std::get<SliderData>(widget.Data).Min = minVal;
			}
		}
	}
	float SliderControl_GetMax(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<SliderData>(widget.Data))
			{
				return std::get<SliderData>(widget.Data).Max;
			}
		}
		return 0.0f;
	}
	void SliderControl_SetMax(uint64_t entityID, float maxVal)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<SliderData>(widget.Data))
			{
				std::get<SliderData>(widget.Data).Max = maxVal;
			}
		}
	}

	float ProgressBarControl_GetProgress(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<ProgressBarData>(widget.Data))
			{
				return std::get<ProgressBarData>(widget.Data).Progress;
			}
		}
		return 0.0f;
	}
	void ProgressBarControl_SetProgress(uint64_t entityID, float progress)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<ProgressBarData>(widget.Data))
			{
				std::get<ProgressBarData>(widget.Data).Progress = progress;
			}
		}
	}
	const Coral::UCChar* ProgressBarControl_GetOverlayText(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<ProgressBarData>(widget.Data))
			{
				return GlueStringPool::ReturnString(std::get<ProgressBarData>(widget.Data).OverlayText);
			}
		}
		return nullptr;
	}
	void ProgressBarControl_SetOverlayText(uint64_t entityID, const Coral::UCChar* text)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>() && text)
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<ProgressBarData>(widget.Data))
			{
				std::get<ProgressBarData>(widget.Data).OverlayText = ch_u16_to_string(text);
			}
		}
	}
	uint8_t ProgressBarControl_GetShowPercentage(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<ProgressBarData>(widget.Data))
			{
				return std::get<ProgressBarData>(widget.Data).ShowPercentage;
			}
		}
		return false;
	}
	void ProgressBarControl_SetShowPercentage(uint64_t entityID, uint8_t show)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<ProgressBarData>(widget.Data))
			{
				std::get<ProgressBarData>(widget.Data).ShowPercentage = show;
			}
		}
	}

	uint8_t WidgetControl_GetActive(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		return entity && entity.HasComponent<ControlComponent>() ? entity.GetComponent<ControlComponent>().IsActive
																 : false;
	}
	void WidgetControl_SetActive(uint64_t entityID, uint8_t active)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<ControlComponent>())
		{
			entity.GetComponent<ControlComponent>().IsActive = active;
		}
	}

	const Coral::UCChar* WidgetControl_GetTextColor(uint64_t entityID)
	{
		// Returns "R G B A" as space-separated integers
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& c = entity.GetComponent<UIControlComponent>().TextStyle.TextColor;
			std::string s =
				std::to_string(c.r) + " " + std::to_string(c.g) + " " + std::to_string(c.b) + " " + std::to_string(c.a);
			return GlueStringPool::ReturnString(s);
		}
		return nullptr;
	}
	void WidgetControl_SetTextColorRGBA(uint64_t entityID, int r, int g, int b, int a)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& col = entity.GetComponent<UIControlComponent>().TextStyle.TextColor;
			col.r = (uint8_t)std::clamp(r, 0, 255);
			col.g = (uint8_t)std::clamp(g, 0, 255);
			col.b = (uint8_t)std::clamp(b, 0, 255);
			col.a = (uint8_t)std::clamp(a, 0, 255);
		}
	}

	uint8_t CheckboxControl_GetChecked(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<CheckboxData>(widget.Data))
			{
				return std::get<CheckboxData>(widget.Data).Checked;
			}
		}
		return false;
	}
	int ComboBoxControl_GetSelectedIndex(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<ComboBoxData>(widget.Data))
			{
				return std::get<ComboBoxData>(widget.Data).SelectedIndex;
			}
		}
		return 0;
	}
	void ComboBoxControl_SetSelectedIndex(uint64_t entityID, int index)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<ComboBoxData>(widget.Data))
			{
				std::get<ComboBoxData>(widget.Data).SelectedIndex = index;
			}
		}
	}
	void ComboBoxControl_AddItem(uint64_t entityID, const Coral::UCChar* item)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>() && item)
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<ComboBoxData>(widget.Data))
			{
				std::get<ComboBoxData>(widget.Data).Items.push_back(ch_u16_to_string(item));
			}
		}
	}
	void ComboBoxControl_ClearItems(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<ComboBoxData>(widget.Data))
			{
				auto& combo = std::get<ComboBoxData>(widget.Data);
				combo.Items.clear();
				combo.SelectedIndex = 0;
			}
		}
	}
	int ComboBoxControl_GetItemCount(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<ComboBoxData>(widget.Data))
			{
				return (int)std::get<ComboBoxData>(widget.Data).Items.size();
			}
		}
		return 0;
	}
	const Coral::UCChar* ComboBoxControl_GetItem(uint64_t entityID, int index)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<ComboBoxData>(widget.Data))
			{
				auto& combo = std::get<ComboBoxData>(widget.Data);
				if (index >= 0 && index < (int)combo.Items.size())
				{
					return GlueStringPool::ReturnString(combo.Items[index]);
				}
			}
		}
		return nullptr;
	}
	const Coral::UCChar* InputTextControl_GetText(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<InputTextData>(widget.Data))
			{
				auto& input = std::get<InputTextData>(widget.Data);
				// Sync text from InputBuffer if it has content
				if (!input.InputBuffer.empty())
				{
					input.Text = input.InputBuffer.data();
				}
				return GlueStringPool::ReturnString(input.Text);
			}
		}
		return nullptr;
	}

	void InputTextControl_SetText(uint64_t entityID, const Coral::UCChar* text)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>() && text)
		{
			auto& widget = entity.GetComponent<UIControlComponent>();
			if (std::holds_alternative<InputTextData>(widget.Data))
			{
				auto& input = std::get<InputTextData>(widget.Data);
				input.Text = ch_u16_to_string(text);
				// Reset buffer so it picks up the new text on next render
				input.InputBuffer.clear();
			}
		}
	}

	uint8_t InputTextControl_HasChanged(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			return entity.GetComponent<UIControlComponent>().ValueChanged;
		}
		return false;
	}

	void InputTextControl_ClearChanged(uint64_t entityID)
	{
		Entity entity = GetEntity(entityID);
		if (entity && entity.HasComponent<UIControlComponent>())
		{
			entity.GetComponent<UIControlComponent>().ValueChanged = false;
		}
	}

	static int s_LastOverlayFrame = -1;
	static float s_OverlayCursorY = 10.0f;

	void UI_Text(const Coral::UCChar* text)
	{
		if (ImGui::GetCurrentContext() == nullptr || !ImGui::GetCurrentContext()->WithinFrameScope || !text)
		{
			return;
		}

		std::string strText = ch_u16_to_string(text);

		auto window = ImGui::GetCurrentContext()->CurrentWindow;
		if (window && !window->SkipItems)
		{
			ImGui::Text("%s", strText.c_str());
		}
		else
		{
			int currentFrame = ImGui::GetFrameCount();
			if (currentFrame != s_LastOverlayFrame)
			{
				s_LastOverlayFrame = currentFrame;
				s_OverlayCursorY = 10.0f;
			}

			ImGui::GetForegroundDrawList()->AddText({10.0f, s_OverlayCursorY}, IM_COL32(255, 255, 255, 255),
													strText.c_str());
			s_OverlayCursorY += ImGui::GetFontSize() + 4.0f;
		}
	}

	void UI_TextColored(const Coral::UCChar* text, float r, float g, float b, float a)
	{
		if (ImGui::GetCurrentContext() == nullptr || !ImGui::GetCurrentContext()->WithinFrameScope || !text)
		{
			return;
		}

		std::string strText = ch_u16_to_string(text);
		auto window = ImGui::GetCurrentContext()->CurrentWindow;
		if (window && !window->SkipItems)
		{
			ImGui::TextColored(ImVec4(r, g, b, a), "%s", strText.c_str());
		}
		else
		{
			int currentFrame = ImGui::GetFrameCount();
			if (currentFrame != s_LastOverlayFrame)
			{
				s_LastOverlayFrame = currentFrame;
				s_OverlayCursorY = 10.0f;
			}

			ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a));
			ImGui::GetForegroundDrawList()->AddText({10.0f, s_OverlayCursorY}, col, strText.c_str());
			s_OverlayCursorY += ImGui::GetFontSize() + 4.0f;
		}
	}

	uint8_t UI_Button(const Coral::UCChar* label)
	{
		if (ImGui::GetCurrentContext() == nullptr || !ImGui::GetCurrentContext()->WithinFrameScope || !label)
		{
			return false;
		}

		std::string strLabel = ch_u16_to_string(label);
		return ImGui::Button(strLabel.c_str());
	}

	void UI_BeginWindow(const Coral::UCChar* title, float x, float y, float w, float h, float bgAlpha)
	{
		if (ImGui::GetCurrentContext() == nullptr || !title)
		{
			return;
		}

		std::string strTitle = ch_u16_to_string(title);

		// Offset position by the main viewport origin so coords are relative to game window
		ImGuiViewport* vp = ImGui::GetMainViewport();
		float vpX = vp ? vp->Pos.x : 0.0f;
		float vpY = vp ? vp->Pos.y : 0.0f;

		if (x >= 0.0f && y >= 0.0f)
		{
			ImGui::SetNextWindowPos(ImVec2(vpX + x, vpY + y), ImGuiCond_Always);
		}
		if (w > 0.0f && h > 0.0f)
		{
			ImGui::SetNextWindowSize(ImVec2(w, h), ImGuiCond_Always);
		}
		ImGui::SetNextWindowBgAlpha(bgAlpha);

		ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
								 ImGuiWindowFlags_NoSavedSettings;
		if (bgAlpha <= 0.0f)
		{
			flags |= ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav;
		}
		ImGui::Begin(strTitle.c_str(), nullptr, flags);
	}

	void UI_EndWindow()
	{
		if (ImGui::GetCurrentContext())
		{
			ImGui::End();
		}
	}

	void UI_BeginChild(const Coral::UCChar* strId, float w, float h, uint8_t border)
	{
		if (ImGui::GetCurrentContext() == nullptr || !strId)
		{
			return;
		}
		std::string id = ch_u16_to_string(strId);
		ImGui::BeginChild(id.c_str(), ImVec2(w, h), border != 0);
	}

	void UI_EndChild()
	{
		if (ImGui::GetCurrentContext())
		{
			ImGui::EndChild();
		}
	}

	void UI_Separator()
	{
		if (ImGui::GetCurrentContext())
		{
			ImGui::Separator();
		}
	}

	void UI_SameLine(float offsetFromStartX, float spacing)
	{
		if (ImGui::GetCurrentContext())
		{
			ImGui::SameLine(offsetFromStartX, spacing);
		}
	}

	void UI_SetNextItemWidth(float itemWidth)
	{
		if (ImGui::GetCurrentContext())
		{
			ImGui::SetNextItemWidth(itemWidth);
		}
	}

	uint8_t UI_InputText(const Coral::UCChar* label, Coral::UCChar* buffer, int maxLen)
	{
		if (ImGui::GetCurrentContext() == nullptr || !label || !buffer || maxLen <= 0)
		{
			return false;
		}

		std::string strLabel = ch_u16_to_string(label);
		std::string strBuf = ch_u16_to_string(buffer);

		char localBuf[512] = {0};
		std::strncpy(localBuf, strBuf.c_str(), sizeof(localBuf) - 1);

		bool changed =
			ImGui::InputText(strLabel.c_str(), localBuf, sizeof(localBuf), ImGuiInputTextFlags_EnterReturnsTrue);

		auto wide = ToWide(localBuf);
		int copyLen = std::min((int)wide.size(), maxLen - 1);
		for (int i = 0; i < copyLen; ++i)
		{
			buffer[i] = wide[i];
		}
		buffer[copyLen] = 0;

		return changed ? 1 : 0;
	}

	void UI_SetKeyboardFocusHere()
	{
		if (ImGui::GetCurrentContext())
		{
			ImGui::SetKeyboardFocusHere();
		}
	}

	void UI_SetScrollHereY(float centerYRatio)
	{
		if (ImGui::GetCurrentContext())
		{
			ImGui::SetScrollHereY(centerYRatio);
		}
	}

	void UI_GetDisplaySize(float* outW, float* outH)
	{
		if (!outW || !outH)
		{
			return;
		}
		ImGuiViewport* vp = ImGui::GetMainViewport();
		if (vp)
		{
			*outW = vp->Size.x;
			*outH = vp->Size.y;
		}
		else if (ImGui::GetCurrentContext())
		{
			*outW = ImGui::GetIO().DisplaySize.x;
			*outH = ImGui::GetIO().DisplaySize.y;
		}
		else
		{
			*outW = 1280.0f;
			*outH = 720.0f;
		}
	}

	void UI_PushStyleColor(int32_t colIdx, float r, float g, float b, float a)
	{
		if (ImGui::GetCurrentContext())
		{
			ImGui::PushStyleColor(static_cast<ImGuiCol>(colIdx), ImVec4(r, g, b, a));
		}
	}

	void UI_PopStyleColor(int32_t count)
	{
		if (ImGui::GetCurrentContext() && count > 0)
		{
			ImGui::PopStyleColor(count);
		}
	}

	void UI_PushStyleVarFloat(int32_t varIdx, float val)
	{
		if (ImGui::GetCurrentContext())
		{
			ImGui::PushStyleVar(static_cast<ImGuiStyleVar>(varIdx), val);
		}
	}

	void UI_PushStyleVarVec2(int32_t varIdx, float x, float y)
	{
		if (ImGui::GetCurrentContext())
		{
			ImGui::PushStyleVar(static_cast<ImGuiStyleVar>(varIdx), ImVec2(x, y));
		}
	}

	void UI_PopStyleVar(int32_t count)
	{
		if (ImGui::GetCurrentContext() && count > 0)
		{
			ImGui::PopStyleVar(count);
		}
	}

	void UI_Dummy(float w, float h)
	{
		if (ImGui::GetCurrentContext())
		{
			ImGui::Dummy(ImVec2(w, h));
		}
	}

	uint8_t UI_IsItemHovered()
	{
		if (ImGui::GetCurrentContext())
		{
			return ImGui::IsItemHovered() ? 1 : 0;
		}
		return 0;
	}

	void UI_SetWindowFontScale(float scale)
	{
		if (ImGui::GetCurrentContext())
		{
			ImGui::SetWindowFontScale(scale);
		}
	}

	uint8_t UI_SliderFloat(const Coral::UCChar* label, float* v, float v_min, float v_max)
	{
		if (ImGui::GetCurrentContext() == nullptr || !ImGui::GetCurrentContext()->WithinFrameScope || !label || !v)
		{
			return 0;
		}
		std::string strLabel = ch_u16_to_string(label);
		return ImGui::SliderFloat(strLabel.c_str(), v, v_min, v_max, "%.1f") ? 1 : 0;
	}

	uint8_t UI_Checkbox(const Coral::UCChar* label, uint8_t* v)
	{
		if (ImGui::GetCurrentContext() == nullptr || !ImGui::GetCurrentContext()->WithinFrameScope || !label || !v)
		{
			return 0;
		}
		std::string strLabel = ch_u16_to_string(label);
		bool val = (*v != 0);
		bool changed = ImGui::Checkbox(strLabel.c_str(), &val);
		if (changed)
		{
			*v = val ? 1 : 0;
		}
		return changed ? 1 : 0;
	}

} // namespace Chained
