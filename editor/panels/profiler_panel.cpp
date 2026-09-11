#include "profiler_panel.h"
#include "engine/core/profiler.h"
#include "imgui.h"
#include <format>
#include <glad/gl.h>

namespace Chained
{
	ProfilerPanel::ProfilerPanel()
	{
		m_Name = "Profiler";
		m_IsOpen = false;
		m_FrameTimeHistory.reserve(100);
		for (int i = 0; i < 100; i++)
		{
			m_FrameTimeHistory.push_back(0.0f);
		}
	}

	void ProfilerPanel::OnImGuiRender(bool readOnly)
	{
		if (!m_IsOpen)
		{
			return;
		}

		UpdateHistory();

		ImGui::Begin(m_Name.c_str(), &m_IsOpen);

		const auto* inst = Instrumentor::TryGet();
		const auto& stats = inst ? inst->GetStats() : ProfilerStats{};

		if (ImGui::CollapsingHeader("Hardware & System", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("GPU and driver information");
			}
			ImGui::Text("GPU: %s", glGetString(GL_RENDERER));
			ImGui::Text("Driver: %s", glGetString(GL_VERSION));
		}

		if (ImGui::CollapsingHeader("Scene Statistics", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Current scene entity and draw call counts");
			}
			ImGui::Columns(2);
			ImGui::Text("Entities:");
			ImGui::NextColumn();
			ImGui::Text("%u", stats.EntityCount);
			ImGui::NextColumn();
			ImGui::Text("Draw Calls:");
			ImGui::NextColumn();
			ImGui::Text("%u", stats.DrawCalls);
			ImGui::NextColumn();
			ImGui::Text("Meshes:");
			ImGui::NextColumn();
			ImGui::Text("%u", stats.MeshCount);
			ImGui::NextColumn();

			ImGui::Text("Colliders:");
			ImGui::NextColumn();
			ImGui::Text("%u", stats.ColliderCount);
			ImGui::NextColumn();
			ImGui::Columns(1);
		}

		if (ImGui::CollapsingHeader("Execution Timeline", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Frame time history and per-system timings");
			}
			if (!m_FrameTimeHistory.empty())
			{
				float maxTime = 0.0f;
				for (float f : m_FrameTimeHistory)
				{
					if (f > maxTime)
					{
						maxTime = f;
					}
				}

				ImGui::PushStyleColor(ImGuiCol_PlotLines, ImVec4(0.2f, 0.7f, 1.0f, 1.0f));
				ImGui::PlotLines("##FrameTime", m_FrameTimeHistory.data(), (int)m_FrameTimeHistory.size(), 0,
								 std::format("Max: {:.2f}ms", maxTime).c_str(), 0.0f, 33.3f, ImVec2(0, 80));
				ImGui::PopStyleColor();
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Frame time history (lower is better, 33ms = 30fps target)");
				}
			}

			for (const auto& result : inst->GetLastFrameResults())
			{
				DrawProfileResult(result);
			}
		}

		ImGui::End();
	}

	void ProfilerPanel::DrawProfileResult(const ProfileResult& result)
	{
		std::string label = std::format("{} - {:.3f}ms", result.Name, result.DurationMS);
		ImGui::Text("%s", label.c_str());
	}

	void ProfilerPanel::UpdateHistory()
	{
		const auto* histInst = Instrumentor::TryGet();
		const auto& results = histInst ? histInst->GetLastFrameResults() : std::vector<ProfileResult>{};
		float frameMS = 0.0f;

		for (const auto& res : results)
		{
			if (res.Name == "Run")
			{
				frameMS = res.DurationMS;
				break;
			}
		}

		if (frameMS <= 0)
		{
			for (const auto& res : results)
			{
				if (res.Name == "MainThread_Frame")
				{
					frameMS = res.DurationMS;
					break;
				}
			}
		}

		if (frameMS > 0)
		{
			for (size_t i = 1; i < m_FrameTimeHistory.size(); i++)
			{
				m_FrameTimeHistory[i - 1] = m_FrameTimeHistory[i];
			}
			m_FrameTimeHistory.back() = frameMS;
		}
	}

} // namespace Chained
