#ifndef CH_EDITOR_GUI_H
#define CH_EDITOR_GUI_H

#include <functional>
#include <string>
#include "editor/layer.h"
#include "editor/panels.h"
#include "engine/common/color.h"

namespace Chained
{

	// Immediate-mode GUI helpers shared by editor panels and property inspectors.
	namespace EditorGUI
	{
		/// @brief Begins a 2-column property grid.
		void BeginPropertyGrid();
		void EndPropertyGrid();
		void DrawPropertyLabel(const char* label);

		// Simple property widgets that do not use columns.
		bool Property(const char* label, bool& value);
		bool Property(const char* label, int& value, int min = 0, int max = 0);
		bool Property(const char* label, float& value, float speed = 0.1f, float min = 0.0f, float max = 0.0f);
		bool Property(const char* label, std::string& value, bool multiline = false);
		bool Property(const char* label, Color& value);
		bool Property(const char* label, glm::vec2& value);
		bool Property(const char* label, glm::vec3& value);
		bool Property(const char* label, glm::vec4& value);
		bool Property(const char* label, uint64_t& value);

		// Renders a glm::vec4 as an RGBA color swatch/picker instead of raw X/Y/Z/W drag fields.
		bool PropertyColor(const char* label, glm::vec4& value, bool hdr = false);

		bool Property(const char* label, int& value, const char** items, int itemCount);

		// Action widgets.
		bool ActionButton(const char* icon, const char* label);

		// File property widgets.
		bool FileProperty(const char* label, std::string& value, const char* filter = nullptr);
		bool FileProperty(const char* label, std::string& path, uint32_t textureId, const char* filter = nullptr);

		bool DrawVec2(const char* label, glm::vec2& values, float resetValue = 0.0f);
		bool DrawVec3(const char* label, glm::vec3& values, float resetValue = 0.0f);
		bool DrawVec4(const char* label, glm::vec4& values, float resetValue = 0.0f);
		// Applies the editor-wide ImGui style.
		void ApplyTheme();

		template <typename F> bool PropertyWidget(const char* label, F&& widgetFn);

		bool FilePropertyImpl(const char* label, std::string& value, const char* filter,
							  std::function<void()> thumbnailFn, const char* placeholder = nullptr);

		template <int N>
		bool DrawVecImpl(const char* label, float* values, float resetValue, const ImVec4* colors,
						 const char* componentLabels[N]);
	} // namespace EditorGUI

} // namespace Chained

#endif // CH_EDITOR_GUI_H
