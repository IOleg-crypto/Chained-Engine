#include "effects_panel.h"
#include "editor/layer.h"
#include "engine/graphics/pipeline/renderer.h"
#include "scene/scene.h"
#include "engine/core/service_locator.h"

namespace Chained
{

	EffectsPanel::EffectsPanel()
	{
		m_Name = "Effects & Debug";
		m_IsOpen = false;
	}

	void EffectsPanel::OnImGuiRender(bool readOnly)
	{
		if (!m_IsOpen)
		{
			return;
		}

		ImGui::Begin(m_Name.c_str(), &m_IsOpen);

		if (!m_Context)
		{
			ImGui::Text("No active scene.");
			ImGui::End();
			return;
		}

		if (ImGui::CollapsingHeader("Viewport Settings", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Configure viewport rendering modes");
			}
			const char* diagnosticModes[] = {"Full Render", "Normals", "Lighting only", "Albedo only"};
			int currentDiag = (int)m_Context->GetSettings().DiagnosticMode;
			if (ImGui::Combo("Diagnostic Mode", &currentDiag, diagnosticModes, 4))
			{
				m_Context->SetDiagnosticMode((float)currentDiag);
				if (auto* renderer = ServiceLocator::TryGet<Renderer>())
				{
					renderer->SetDiagnosticMode((float)currentDiag);
				}
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Switch between diagnostic rendering modes");
			}
		}

		if (ImGui::CollapsingHeader("Debug Visualization", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Toggle debug visualization overlays");
			}
			auto& debugFlags = m_Context->GetSettings().DebugFlags;
			ImGui::Checkbox("Physics (Colliders)", &debugFlags.DrawColliders);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Show/hide physics collider shapes");
			}
			if (debugFlags.DrawColliders)
			{
				const char* wireModes[] = {"Wireframe", "Solid", "Solid + Wireframe"};
				ImGui::Combo("Visual Mode", &debugFlags.SetCollisionWireframeMode, wireModes, 3);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Choose collider display style");
				}
			}
			ImGui::Checkbox("Lights", &debugFlags.DrawLights);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Show/hide light source gizmos");
			}
			ImGui::Checkbox("Spawn Zones", &debugFlags.DrawSpawnZones);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Show/hide spawn zone volumes");
			}
			ImGui::Checkbox("Draw Grid", &debugFlags.DrawGrid);
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Show/hide the world grid");
			}
			if (debugFlags.DrawGrid)
			{
				auto& grid = m_Context->GetSettings().Grid;
				ImGui::Indent(12.0f);
				ImGui::DragFloat("Spacing", &grid.Spacing, 0.1f, 0.01f, 50.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Primary grid line spacing");
				}
				ImGui::DragFloat("Secondary Spacing", &grid.SecondarySpacing, 1.0f, 1.0f, 100.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Secondary grid line spacing");
				}
				ImGui::ColorEdit4("Color", &grid.Color.x);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Grid line color");
				}
				ImGui::DragFloat("Fade Start", &grid.FadeStart, 10.0f, 0.0f, 10000.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Distance where grid starts to fade");
				}
				ImGui::DragFloat("Fade End", &grid.FadeEnd, 10.0f, 100.0f, 50000.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Distance where grid is fully transparent");
				}
				ImGui::DragFloat("Plane Size", &grid.PlaneSize, 100.0f, 100.0f, 50000.0f);
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Total grid plane size");
				}
				ImGui::Unindent(12.0f);
			}
		}

		ImGui::End();
	}

} // namespace Chained
