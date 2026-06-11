#include "engine/core/engine_pch.h"
#include "profiler_panel.h"
#include "engine/core/profiler.h"
#include <format>
#include "imgui.h"
#include "rlgl.h"
#include "imgui.h"
#include "rlgl.h"

#define GL_RENDERER 0x1F01
extern "C" const unsigned char* glGetString(unsigned int name);

namespace CHEngine
{
ProfilerPanel::ProfilerPanel()
{
    m_Name = "Profiler";
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

    const auto& stats = Profiler::GetStats();

    if (ImGui::CollapsingHeader("Hardware & System", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("GPU: %s", glGetString(GL_RENDERER));
        //ImGui::Text("Driver: %s", glGetString(GL_VERSION));
    }

    if (ImGui::CollapsingHeader("Scene Statistics", ImGuiTreeNodeFlags_DefaultOpen))
    {
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

        // Format polys (K, M)
        std::string polyStr;
        if (stats.PolyCount > 1000000)
        {
            polyStr = std::format("{:.2f} M", stats.PolyCount / 1000000.0f);
        }
        else if (stats.PolyCount > 1000)
        {
            polyStr = std::format("{:.1f} K", stats.PolyCount / 1000.0f);
        }
        else
        {
            polyStr = std::to_string(stats.PolyCount);
        }

        ImGui::Text("Polygons:");
        ImGui::NextColumn();
        ImGui::Text("%s", polyStr.c_str());
        ImGui::NextColumn();
        ImGui::Text("Colliders:");
        ImGui::NextColumn();
        ImGui::Text("%u", stats.ColliderCount);
        ImGui::NextColumn();
        ImGui::Columns(1);
    }

    const auto& results = Profiler::GetLastFrameResults();
    if (ImGui::CollapsingHeader("Execution Timeline", ImGuiTreeNodeFlags_DefaultOpen))
    {
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
        }

        for (const auto& result : results)
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
    const auto& results = Profiler::GetLastFrameResults();
    float frameMS = 0.0f;

    for (const auto& res : results)
    {
        if (res.Name == "MainThread_Frame")
        {
            frameMS = res.DurationMS;
            break;
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

} // namespace CHEngine
