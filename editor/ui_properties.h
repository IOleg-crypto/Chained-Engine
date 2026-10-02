#ifndef CH_UI_PROPERTIES_H
#define CH_UI_PROPERTIES_H

#include "thirdparty/IconsFontAwesome6.h"
#include "gui.h"
#include "engine/reflection/reflection.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Chained
{
	// Implementation of the Archive concept for ImGui UI
	class UIProperties : public IPropertyArchive
	{
	public:
		UIProperties() = default;

		virtual ReflectionMode GetReflectionMode() const override
		{
			return ReflectionMode::UI;
		}
		virtual bool HasChanged() const override
		{
			return m_Changed;
		}
		virtual void SetChanged(bool changed) override
		{
			m_Changed = changed;
		}

		// IPropertyArchive overrides
		virtual bool Property(const char* name, int& value, const PropertyMeta& meta = {}) override
		{
			return PropertyInternal(name, value, meta);
		}
		virtual bool Property(const char* name, float& value, const PropertyMeta& meta = {}) override
		{
			return PropertyInternal(name, value, meta);
		}
		virtual bool Property(const char* name, bool& value, const PropertyMeta& meta = {}) override
		{
			return PropertyInternal(name, value, meta);
		}
		virtual bool Property(const char* name, std::string& value, const PropertyMeta& meta = {}) override
		{
			return PropertyInternal(name, value, meta);
		}
		virtual bool Property(const char* name, glm::vec2& value, const PropertyMeta& meta = {}) override
		{
			return PropertyInternal(name, value, meta);
		}
		virtual bool Property(const char* name, glm::vec3& value, const PropertyMeta& meta = {}) override
		{
			return PropertyInternal(name, value, meta);
		}
		virtual bool Property(const char* name, glm::vec4& value, const PropertyMeta& meta = {}) override
		{
			return PropertyInternal(name, value, meta);
		}
		virtual bool Property(const char* name, Color& value, const PropertyMeta& meta = {}) override
		{
			return PropertyInternal(name, value, meta);
		}
		virtual bool Enum(const char* name, int& value, const char** names, int count,
						  const PropertyMeta& meta = {}) override
		{
			return EnumPropertyInternal(name, value, names, count, meta);
		}

		virtual bool StringEnum(const char* name, std::string& value, const std::vector<std::string>& options,
								const PropertyMeta& meta = {}) override
		{
			return StringEnumInternal(name, value, options, meta);
		}

		virtual bool Property(const char* name, uint64_t& value, const PropertyMeta& meta = {}) override
		{
			return Handle(name, value, meta);
		}
		virtual bool Handle(const char* name, uint64_t& value, const PropertyMeta& meta = {}) override
		{
			bool changed = EditorGUI::Property(name, value);
			UpdateState(changed);
			return changed;
		}
		virtual bool File(const char* name, std::string& value, const char* extensions = nullptr,
						  const PropertyMeta& meta = {}) override
		{
			ImGui::BeginDisabled(meta.ReadOnly);
			bool changed = EditorGUI::FileProperty(name, value, extensions);
			ImGui::EndDisabled();
			if (!meta.Tooltip.empty() && ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", meta.Tooltip.c_str());
			}
			UpdateState(changed);
			return changed;
		}
		virtual void Header(const char* label) override
		{
			HeaderInternal(label);
		}
		virtual void Separator() override
		{
			SeparatorInternal();
		}
		bool HasFinished() const
		{
			return m_Started && m_Finished;
		}
		bool HasStarted() const
		{
			return m_Started;
		}

		// Template methods for non-virtual calls (still used by Properties<UIProperties>)
		template <typename T> bool Property(const char* name, T& value)
		{
			return PropertyInternal(name, value, {});
		}
		template <typename T_Enum> bool Property(const char* name, T_Enum& value, const char** names, int count)
		{
			return EnumPropertyInternal(name, (int&)value, names, count, {});
		}
		template <typename T> bool Property(const char* name, T& value, const PropertyMeta& meta)
		{
			return PropertyInternal(name, value, meta);
		}
		template <typename T_Enum>
		bool Property(const char* name, T_Enum& value, const char** names, int count, const PropertyMeta& meta)
		{
			return EnumPropertyInternal(name, (int&)value, names, count, meta);
		}

		virtual void BeginSequence(const char* name, size_t& size) override
		{
			if (ImGui::GetCurrentTable() != nullptr)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
			}

			ImGuiTreeNodeFlags flags =
				ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (ImGui::GetCurrentTable() != nullptr)
			{
				flags |= ImGuiTreeNodeFlags_SpanAllColumns;
			}

			m_InSequence = ImGui::TreeNodeEx(name, flags);
			if (m_InSequence && ImGui::GetCurrentTable() != nullptr)
			{
				ImGui::TableSetColumnIndex(1);
			}
		}

		virtual void EndSequence() override
		{
			if (m_InSequence)
			{
				ImGui::TreePop();
				m_InSequence = false;
			}
		}

		virtual bool Nested(const char* name, std::function<void(IPropertyArchiveBase&)> callback) override
		{
			if (ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
			{
				callback(*this);
				ImGui::TreePop();
				return true; // Simplified: assume changed if we opened and called callback
			}
			return false;
		}

		virtual void BeginMap(const char* name, size_t& size) override
		{
			// Not used directly in UI mode — Map() template handles rendering
		}

		virtual void EndMap() override
		{
		}

		virtual bool MapNextKey(std::string& key) override
		{
			return false;
		}

		template <typename V> bool Map(const char* name, std::unordered_map<std::string, V>& map)
		{
			if (ImGui::GetCurrentTable() != nullptr)
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
			}

			bool localChanged = false;
			ImGuiTreeNodeFlags flags =
				ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (ImGui::GetCurrentTable() != nullptr)
			{
				flags |= ImGuiTreeNodeFlags_SpanAllColumns;
			}

			if (ImGui::TreeNodeEx(name, flags))
			{
				auto it = map.begin();
				while (it != map.end())
				{
					ImGui::PushID(it->first.c_str());

					if (ImGui::GetCurrentTable() != nullptr)
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(1);
					}

					if (ImGui::Button(ICON_FA_TRASH))
					{
						it = map.erase(it);
						m_Changed = localChanged = true;
						ImGui::PopID();
						continue;
					}

					ImGui::SameLine();

					// Key input
					char keyBuf[128];
					strncpy(keyBuf, it->first.c_str(), sizeof(keyBuf));
					keyBuf[sizeof(keyBuf) - 1] = '\0';
					ImGui::SetNextItemWidth(120);
					if (ImGui::InputText("##key", keyBuf, sizeof(keyBuf)))
					{
						std::string newKey = keyBuf;
						if (newKey != it->first && map.find(newKey) == map.end())
						{
							float val = it->second;
							auto next = map.erase(it);
							it = map.insert_or_assign(next, newKey, val).first;
							m_Changed = localChanged = true;
						}
						ImGui::PopID();
						continue;
					}

					ImGui::SameLine();
					ImGui::Text("=");
					ImGui::SameLine();

					// Value input
					float val = it->second;
					ImGui::SetNextItemWidth(-1);
					if (ImGui::DragFloat("##val", &val, 0.01f))
					{
						it->second = val;
						m_Changed = localChanged = true;
					}

					++it;
					ImGui::PopID();
					ImGui::Separator();
				}

				ImGui::Spacing();
				if (ImGui::GetCurrentTable() != nullptr)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(1);
				}

				if (EditorGUI::ActionButton(ICON_FA_PLUS, "Add Variable"))
				{
					std::string newKey = "var_" + std::to_string(map.size());
					map[newKey] = 0.0f;
					m_Changed = localChanged = true;
				}
				ImGui::TreePop();
			}
			return localChanged;
		}

	private:
		bool m_InSequence = false;

	public:
		// --- Legacy types or internal helpers if needed ---
		// Note: Most templates are now handled by the base class Properties<T_Archive>.

	private:
		template <typename T> bool PropertyInternal(const char* name, T& value, const PropertyMeta& meta)
		{
			bool changed = false;
			ImGui::BeginDisabled(meta.ReadOnly);
			if constexpr (std::is_same_v<T, float>)
			{
				changed = EditorGUI::Property(name, value, meta.Speed, meta.MinValue, meta.MaxValue);
			}
			else if constexpr (std::is_same_v<T, int>)
			{
				changed = EditorGUI::Property(name, value, (int)meta.MinValue, (int)meta.MaxValue);
			}
			else if constexpr (std::is_same_v<T, std::string>)
			{
				changed = EditorGUI::Property(name, value);
			}
			else if constexpr (is_variant_v<T>)
			{
				changed = std::visit([&](auto&& v) { return EditorGUI::Property(name, v); }, value);
			}
			else if constexpr (std::is_same_v<T, float[4]>)
			{
				changed = EditorGUI::Property(name, *(glm::vec4*)&value);
			}
			else
			{
				if constexpr (requires { EditorGUI::Property(name, value); })
				{
					changed = EditorGUI::Property(name, value);
				}
			}
			ImGui::EndDisabled();

			if (!meta.Tooltip.empty() && ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("%s", meta.Tooltip.c_str());
			}
			UpdateState(changed);
			return changed;
		}

		bool EnumPropertyInternal(const char* name, int& value, const char** names, int count,
								  const PropertyMeta& meta);
		bool StringEnumInternal(const char* name, std::string& value, const std::vector<std::string>& options,
								const PropertyMeta& meta);
		void HeaderInternal(const char* label);
		void SeparatorInternal();
		void UpdateState(bool changed);

		bool m_Changed = false;
		bool m_Started = false;
		bool m_Finished = false;
	};
} // namespace Chained

#endif // CH_UI_PROPERTIES_H
