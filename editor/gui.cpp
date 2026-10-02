#include "gui.h"
#include "editor/layer.h"
#include "editor/panels/panel.h"
#include "editor/panels/viewport_panel.h"
#include "editor/project/project_exporter.h"
#include "engine/app/application.h"
#include "engine/core/service_locator.h"
#include "engine/platform/dialogs/dialogs.h"
#include "engine/project/project.h"
#include "events.h"
#include "thirdparty/IconsFontAwesome6.h"
#include "misc/cpp/imgui_stdlib.h"

#define IMGUI_DEFINE_MATH_OPERATORS
#include "engine/common/thread_pool.h"
#include "thirdparty/imgui/imgui_internal.h"
#include "engine/scripting/scriptengine.h"
#include "editor/font_choice_gui.h"
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace Chained
{

	static float GetButtonSize()
	{
		return ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
	}

	static float GetThumbnailSize(float buttonSize)
	{
		return buttonSize * 1.5f;
	}

	void EditorGUI::DrawPropertyLabel(const char* label)
	{
		const char* displayLabel = label ? label : "Unknown";
		if (ImGui::GetCurrentTable() != nullptr)
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::Text("%s", displayLabel);
			ImGui::TableSetColumnIndex(1);
		}
		else
		{
			ImGui::Text("%s", displayLabel);
			ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.4f);
		}
	}

	void EditorGUI::BeginPropertyGrid()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 6));
		ImGui::BeginTable("PropertyGrid", 2,
						  ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
							  ImGuiTableFlags_SizingStretchSame);

		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 120.0f);
		ImGui::TableSetupColumn("Control", ImGuiTableColumnFlags_WidthStretch);
	}

	void EditorGUI::EndPropertyGrid()
	{
		ImGui::EndTable();
		ImGui::PopStyleVar();
	}

	// --- Property Widgets Implementation ---

	template <typename F> bool EditorGUI::PropertyWidget(const char* label, F&& widgetFn)
	{
		if (!label)
		{
			return false;
		}
		DrawPropertyLabel(label);
		ImGui::PushID(label);
		bool changed = widgetFn();
		ImGui::PopID();
		return changed;
	}

	bool EditorGUI::Property(const char* label, bool& value)
	{
		return PropertyWidget(label, [&]() { return ImGui::Checkbox("##prop", &value); });
	}

	bool EditorGUI::Property(const char* label, float& value, float speed, float min, float max)
	{
		return PropertyWidget(label, [&]() { return ImGui::DragFloat("##prop", &value, speed, min, max); });
	}

	bool EditorGUI::Property(const char* label, int& value, int min, int max)
	{
		return PropertyWidget(label, [&]() { return ImGui::DragInt("##prop", &value, 1.0f, min, max); });
	}

	bool EditorGUI::Property(const char* label, uint64_t& value)
	{
		return PropertyWidget(label, [&]() { return ImGui::InputScalar("##prop", ImGuiDataType_U64, &value); });
	}

	bool EditorGUI::Property(const char* label, std::string& value, bool multiline)
	{
		if (!label)
		{
			return false;
		}
		DrawPropertyLabel(label);
		ImGui::PushID(label);
		bool changed;
		if (multiline)
		{
			changed = ImGui::InputTextMultiline("##prop", &value, ImVec2(0, ImGui::GetTextLineHeightWithSpacing() * 3));
		}
		else
		{
			changed = ImGui::InputText("##prop", &value);
		}
		ImGui::PopID();
		return changed;
	}

	bool EditorGUI::Property(const char* label, Color& value)
	{
		return PropertyWidget(label, [&]() {
			float c[4] = {value.r / 255.0f, value.g / 255.0f, value.b / 255.0f, value.a / 255.0f};
			ImGuiColorEditFlags flags =
				ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_DisplayRGB;
			bool changed = ImGui::ColorEdit4("##prop", c, flags);
			if (changed)
			{
				value = {(unsigned char)(c[0] * 255), (unsigned char)(c[1] * 255), (unsigned char)(c[2] * 255),
						 (unsigned char)(c[3] * 255)};
			}
			return changed;
		});
	}

	bool EditorGUI::Property(const char* label, glm::vec2& value)
	{
		return DrawVec2(label, value, 0.0f);
	}

	bool EditorGUI::Property(const char* label, glm::vec3& value)
	{
		return DrawVec3(label, value, 0.0f);
	}

	bool EditorGUI::Property(const char* label, glm::vec4& value)
	{
		return DrawVec4(label, value, 0.0f);
	}

	bool EditorGUI::PropertyColor(const char* label, glm::vec4& value, bool hdr)
	{
		return PropertyWidget(label, [&]() {
			ImGuiColorEditFlags flags = ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_DisplayRGB;
			if (hdr)
			{
				flags |= ImGuiColorEditFlags_HDR | ImGuiColorEditFlags_Float;
			}
			return ImGui::ColorEdit4("##prop", &value.x, flags);
		});
	}

	bool EditorGUI::Property(const char* label, int& value, const char** items, int itemCount)
	{
		if (!label)
		{
			return false;
		}
		return PropertyWidget(label, [&]() {
			if (!items || itemCount <= 0)
			{
				return false;
			}
			return ImGui::Combo("##prop", &value, items, itemCount);
		});
	}

	bool EditorGUI::FilePropertyImpl(const char* label, std::string& value, const char* filter,
									 std::function<void()> thumbnailFn, const char* placeholder)
	{
		if (!label)
		{
			return false;
		}
		DrawPropertyLabel(label);
		ImGui::PushID(label);

		float width = ImGui::GetContentRegionAvail().x;
		float buttonSize = GetButtonSize();

		float thumbnailSize = 0.0f;
		if (thumbnailFn)
		{
			thumbnailSize = GetThumbnailSize(buttonSize);
			thumbnailFn();
			ImGui::SameLine();
		}

		ImGui::PushItemWidth(width - buttonSize - thumbnailSize - (thumbnailFn ? 10.0f : 5.0f));

		auto project = Project::GetActive();
		std::string displayPath = project ? project->GetRelativePath(value) : value;
		char inputTextBuf[256];
		memset(inputTextBuf, 0, sizeof(inputTextBuf));
		strncpy(inputTextBuf, displayPath.c_str(), sizeof(inputTextBuf) - 1);

		bool changed = false;
		const char* hint = placeholder ? placeholder : "";
		if (ImGui::InputTextWithHint("##prop", hint, inputTextBuf, sizeof(inputTextBuf)))
		{
			value = project ? project->GetRelativePath(inputTextBuf) : std::string(inputTextBuf);
			changed = true;
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				const char* dropPath = static_cast<const char*>(payload->Data);
				if (dropPath)
				{
					value = project ? project->GetRelativePath(dropPath) : std::string(dropPath);
					changed = true;
				}
			}
			ImGui::EndDragDropTarget();
		}
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button(ICON_FA_FOLDER_OPEN, {buttonSize, buttonSize}))
		{
			std::vector<DialogFilter> filters;
			if (filter != nullptr && filter[0] != '\0')
			{
				filters.push_back({"Files", filter});
			}
			auto result = Chained::Dialogs::OpenFile(filters);
			if (result)
			{
				value = project ? project->GetRelativePath(*result) : result->string();
				changed = true;
			}
		}

		ImGui::PopID();
		return changed;
	}

	bool EditorGUI::FileProperty(const char* label, std::string& value, const char* filter)
	{
		return FilePropertyImpl(label, value, filter, nullptr, nullptr);
	}

	bool EditorGUI::FileProperty(const char* label, std::string& path, uint32_t textureId, const char* filter)
	{
		const char* placeholder = (textureId > 0 && path.empty()) ? "<embedded>" : nullptr;
		return FilePropertyImpl(
			label, path, filter,
			[textureId]() {
				float buttonSize = GetButtonSize();
				float thumbnailSize = GetThumbnailSize(buttonSize);
				if (textureId > 0)
				{
					ImGui::Image((void*)(intptr_t)textureId, {thumbnailSize, thumbnailSize}, {0, 1}, {1, 0});
				}
				else
				{
					ImGui::Button("##empty", {thumbnailSize, thumbnailSize});
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("No texture loaded");
					}
				}
			},
			placeholder);
	}

	bool EditorGUI::ActionButton(const char* icon, const char* label)
	{
		std::string text;
		if (icon && icon[0] != '\0')
		{
			text = std::string(icon) + " " + (label ? label : "");
		}
		else
		{
			text = (label ? label : "");
		}
		return ImGui::Button(text.c_str());
	}

	static void DrawPropertyControl(const char* id, float& val, ImVec4 color, const char* label, float resetValue,
									float width, bool& changed)
	{
		if (!label || !id)
		{
			return;
		}
		ImGui::PushID(label);

		float lineHeight = GetButtonSize();
		ImVec2 buttonSize = {lineHeight, lineHeight};

		ImGui::PushStyleColor(ImGuiCol_Button, color);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

		if (ImGui::Button(label, buttonSize))
		{
			val = resetValue;
			changed = true;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Click to reset to %.2f", resetValue);
		}

		ImGui::PopStyleVar();
		ImGui::PopStyleColor(3);

		ImGui::SameLine(0, 0);

		ImGui::SetNextItemWidth(width - buttonSize.x);
		char buf[32];
		snprintf(buf, sizeof(buf), "##%.8s_%.8s", label, id);
		if (ImGui::DragFloat(buf, &val, 0.1f, 0.0f, 0.0f, "%.2f"))
		{
			changed = true;
		}

		ImGui::PopID();
	}

	template <int N>
	bool EditorGUI::DrawVecImpl(const char* label, float* values, float resetValue, const ImVec4* colors,
								const char* componentLabels[N])
	{
		if (!label)
		{
			return false;
		}
		DrawPropertyLabel(label);
		ImGui::PushID(label);

		bool changed = false;

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{4, 0});
		float width = ImGui::GetContentRegionAvail().x;
		float spacing = 4.0f * (N - 1);
		float itemWidth = (width - spacing) / N;

		ImGui::BeginGroup();

		for (int i = 0; i < N; ++i)
		{
			if (i > 0)
			{
				ImGui::SameLine();
			}
			ImGui::SetNextItemWidth(itemWidth);
			DrawPropertyControl(componentLabels[i], values[i], colors[i], componentLabels[i], resetValue, itemWidth,
								changed);
		}

		ImGui::EndGroup();

		ImGui::PopStyleVar();
		ImGui::PopID();
		return changed;
	}

	bool EditorGUI::DrawVec2(const char* label, glm::vec2& values, float resetValue)
	{
		float arr[2] = {values.x, values.y};
		ImVec4 colors[2] = {{0.8f, 0.1f, 0.15f, 1.0f}, {0.2f, 0.7f, 0.2f, 1.0f}};
		const char* labels[2] = {"X", "Y"};
		bool changed = DrawVecImpl<2>(label, arr, resetValue, colors, labels);
		if (changed)
		{
			values.x = arr[0];
			values.y = arr[1];
		}
		return changed;
	}

	bool EditorGUI::DrawVec3(const char* label, glm::vec3& values, float resetValue)
	{
		float arr[3] = {values.x, values.y, values.z};
		ImVec4 colors[3] = {{0.8f, 0.1f, 0.15f, 1.0f}, {0.2f, 0.7f, 0.2f, 1.0f}, {0.1f, 0.25f, 0.8f, 1.0f}};
		const char* labels[3] = {"X", "Y", "Z"};
		bool changed = DrawVecImpl<3>(label, arr, resetValue, colors, labels);
		if (changed)
		{
			values.x = arr[0];
			values.y = arr[1];
			values.z = arr[2];
		}
		return changed;
	}

	bool EditorGUI::DrawVec4(const char* label, glm::vec4& values, float resetValue)
	{
		float arr[4] = {values.x, values.y, values.z, values.w};
		ImVec4 colors[4] = {
			{0.8f, 0.1f, 0.15f, 1.0f}, {0.2f, 0.7f, 0.2f, 1.0f}, {0.1f, 0.25f, 0.8f, 1.0f}, {0.5f, 0.5f, 0.5f, 1.0f}};
		const char* labels[4] = {"X", "Y", "Z", "W"};
		bool changed = DrawVecImpl<4>(label, arr, resetValue, colors, labels);
		if (changed)
		{
			values.x = arr[0];
			values.y = arr[1];
			values.z = arr[2];
			values.w = arr[3];
		}
		return changed;
	}

	void EditorGUI::ApplyTheme()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		style = ImGuiStyle(); // Reset to clean defaults to prevent ScaleAllSizes from accumulating

		style.WindowRounding = 5.0f;
		style.FrameRounding = 4.0f;
		style.PopupRounding = 4.0f;
		style.ScrollbarRounding = 12.0f;
		style.GrabRounding = 4.0f;
		style.TabRounding = 4.0f;

		ImVec4* colors = style.Colors;
		colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.36f, 0.42f, 0.47f, 1.00f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.12f, 0.14f, 1.00f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.14f, 0.16f, 1.00f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.10f, 0.12f, 0.94f);
		colors[ImGuiCol_Border] = ImVec4(0.20f, 0.22f, 0.25f, 0.50f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.20f, 0.22f, 1.00f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.28f, 0.32f, 1.00f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.22f, 0.24f, 0.26f, 1.00f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.06f, 0.08f, 0.10f, 1.00f);

		colors[ImGuiCol_Header] = ImVec4(0.20f, 0.25f, 0.35f, 0.60f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.35f, 0.50f, 0.80f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.30f, 0.40f, 0.60f, 1.00f);

		colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.22f, 0.25f, 1.00f);
		colors[ImGuiCol_CheckMark] = ImVec4(0.40f, 0.60f, 0.90f, 1.00f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.40f, 0.60f, 0.90f, 1.00f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.50f, 0.70f, 1.00f, 1.00f);
		colors[ImGuiCol_Button] = ImVec4(0.18f, 0.20f, 0.22f, 1.00f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.35f, 0.50f, 1.00f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.30f, 0.45f, 0.70f, 1.00f);

		colors[ImGuiCol_Tab] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.35f, 0.50f, 0.80f);
		colors[ImGuiCol_TabActive] = ImVec4(0.12f, 0.14f, 0.16f, 1.00f);
		colors[ImGuiCol_TabUnfocused] = ImVec4(0.08f, 0.10f, 0.12f, 1.00f);
		colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.10f, 0.12f, 0.14f, 1.00f);
		colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
		colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
		colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
		colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
		colors[ImGuiCol_TextSelectedBg] = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
		colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
		colors[ImGuiCol_NavHighlight] = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
		colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.10f, 0.12f, 0.14f, 0.73f);

		float fontSize = EditorLayer::Get().GetConfig().FontSize;
		float scale = fontSize > 0.0f ? (fontSize / 13.0f) : 1.0f;
		style.ScaleAllSizes(scale);
	}

} // namespace Chained
