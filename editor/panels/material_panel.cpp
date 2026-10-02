#include "material_panel.h"
#include "engine/scene/components/render/model_component.h"
#include "engine/scene/scene_events.h"
#include "imgui.h"
#include "property_editor.h"
#include "ui_properties.h"
#include "engine/assets/types/texture_asset.h"
#include "engine/core/service_locator.h"
#include "engine/assets/asset_manager.h"
#include "engine/assets/types/model_asset.h"
#include "engine/assets/types/material_asset.h"
#include "editor/layer.h"
#include <filesystem>

namespace Chained
{

	MaterialPanel::MaterialPanel()
	{
		m_Name = "Material Editor";
	}

	static uint32_t GetTextureID(const std::shared_ptr<Texture>& map, const std::string& path)
	{
		if (map)
		{
			return map->GetNativeHandle();
		}
		if (path.empty())
		{
			return 0;
		}
		auto* am = ServiceLocator::TryGet<AssetManager>();
		if (am)
		{
			auto texAsset = am->Get<TextureAsset>(path);
			if (texAsset && texAsset->IsReady())
			{
				auto gpuTex = texAsset->GetTexture();
				if (gpuTex)
				{
					return gpuTex->GetNativeHandle();
				}
			}
		}
		return 0;
	}

	static void UpdateTextureFromPath(std::shared_ptr<Texture>& outMap, const std::string& path)
	{
		auto* am = ServiceLocator::TryGet<AssetManager>();
		if (!am || path.empty())
		{
			return;
		}
		auto texAsset = am->Get<TextureAsset>(path);
		if (texAsset && texAsset->IsReady())
		{
			outMap = texAsset->GetTexture();
		}
	}

	static bool DrawSectionHeader(const char* icon, const char* label)
	{
		ImGui::PushStyleColor(ImGuiCol_Header, {0.2f, 0.25f, 0.35f, 0.8f});
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, {0.3f, 0.4f, 0.6f, 1.0f});
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, {0.25f, 0.35f, 0.5f, 1.0f});
		bool open = ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed);
		ImGui::PopStyleColor(3);
		return open;
	}

	void MaterialPanel::DrawMaterialSlot(Material& mat)
	{
		if (DrawSectionHeader(ICON_FA_IMAGE, ICON_FA_IMAGE " Albedo"))
		{
			ImGui::Indent();
			EditorGUI::BeginPropertyGrid();
			EditorGUI::PropertyColor("Color", mat.AlbedoColor);
			if (EditorGUI::FileProperty("Texture", mat.AlbedoPath, GetTextureID(mat.AlbedoMap, mat.AlbedoPath),
										"png,jpg,tga"))
			{
				UpdateTextureFromPath(mat.AlbedoMap, mat.AlbedoPath);
			}
			EditorGUI::EndPropertyGrid();
			ImGui::Unindent();
		}

		if (DrawSectionHeader(ICON_FA_WATER, ICON_FA_WATER " Normals"))
		{
			ImGui::Indent();
			EditorGUI::BeginPropertyGrid();
			if (EditorGUI::FileProperty("Normal Map", mat.NormalPath, GetTextureID(mat.NormalMap, mat.NormalPath),
										"png,jpg,tga"))
			{
				UpdateTextureFromPath(mat.NormalMap, mat.NormalPath);
			}
			EditorGUI::EndPropertyGrid();
			ImGui::Unindent();
		}

		if (DrawSectionHeader(ICON_FA_CIRCLE_HALF_STROKE, ICON_FA_CIRCLE_HALF_STROKE " PBR (Metal/Rough)"))
		{
			ImGui::Indent();
			EditorGUI::BeginPropertyGrid();
			EditorGUI::Property("Metalness", mat.Metalness, 0.01f, 0.0f, 1.0f);
			EditorGUI::Property("Roughness", mat.Roughness, 0.01f, 0.0f, 1.0f);
			if (EditorGUI::FileProperty("PBR Map", mat.MetallicRoughnessPath,
										GetTextureID(mat.MetallicRoughnessMap, mat.MetallicRoughnessPath),
										"png,jpg,tga"))
			{
				UpdateTextureFromPath(mat.MetallicRoughnessMap, mat.MetallicRoughnessPath);
			}
			EditorGUI::EndPropertyGrid();
			ImGui::TextDisabled("PBR map: G = Roughness, B = Metalness (glTF convention)");
			ImGui::Unindent();
		}

		if (DrawSectionHeader(ICON_FA_SUN, ICON_FA_SUN " Emissive"))
		{
			ImGui::Indent();
			EditorGUI::BeginPropertyGrid();
			EditorGUI::PropertyColor("Color", mat.EmissiveColor, /*hdr*/ true);
			EditorGUI::Property("Intensity", mat.EmissiveIntensity, 0.05f, 0.0f, 1000.0f);
			if (EditorGUI::FileProperty("Emissive Map", mat.EmissivePath,
										GetTextureID(mat.EmissiveMap, mat.EmissivePath), "png,jpg,tga"))
			{
				UpdateTextureFromPath(mat.EmissiveMap, mat.EmissivePath);
			}
			EditorGUI::EndPropertyGrid();
			ImGui::Unindent();
		}

		if (DrawSectionHeader(ICON_FA_GEARS, ICON_FA_GEARS " Settings"))
		{
			ImGui::Indent();
			EditorGUI::BeginPropertyGrid();
			EditorGUI::Property("Transparent", mat.Transparent);
			ImGui::BeginDisabled(!mat.Transparent);
			EditorGUI::Property("Alpha", mat.Alpha, 0.01f, 0.0f, 1.0f);
			ImGui::EndDisabled();
			EditorGUI::EndPropertyGrid();
			ImGui::Unindent();
		}
	}

	void MaterialPanel::OnImGuiRender(bool readOnly)
	{
		if (!m_IsOpen)
		{
			return;
		}

		ImGui::Begin(m_Name.c_str(), &m_IsOpen);

		if (m_SelectedEntity && (!m_Context || m_SelectedEntity.GetRegistryPtr() != m_Context->GetRegistryPtr() ||
								 !m_SelectedEntity.IsValid()))
		{
			m_SelectedEntity = {};
		}

		if (m_SelectedEntity)
		{
			ImGui::BeginDisabled(readOnly);

			std::vector<Material>* materials = nullptr;

			if (m_SelectedEntity.HasComponent<ModelComponent>())
			{
				auto& mc = m_SelectedEntity.GetComponent<ModelComponent>();

				// Reload materials if: path changed, or materials not yet loaded while asset is ready.
				bool pathChanged = (mc.ModelPath != m_LoadedModelPath);
				bool shouldLoad = !mc.ModelPath.empty() && (pathChanged || m_Materials.empty());
				if (shouldLoad)
				{
					auto* assetMgr = ServiceLocator::TryGet<AssetManager>();
					if (assetMgr)
					{
						auto asset = assetMgr->Get<ModelAsset>(mc.ModelPath);
						if (asset && asset->IsReady())
						{
							m_Materials = asset->GetMaterials();
							m_LoadedModelPath = mc.ModelPath;

							std::filesystem::path modelPath(mc.ModelPath);
							std::string modelName = modelPath.stem().string();
							std::filesystem::path modelDir = modelPath.parent_path();

							for (size_t i = 0; i < m_Materials.size(); ++i)
							{
								if (i < mc.MaterialPaths.size() && !mc.MaterialPaths[i].empty())
								{
									auto matAsset = assetMgr->Get<MaterialAsset>(mc.MaterialPaths[i]);
									if (matAsset && matAsset->IsReady())
									{
										m_Materials[i] = matAsset->GetMaterial();
									}
								}
								else
								{
									std::string matFileName = modelName + "_material_" + std::to_string(i) + ".chmat";
									std::string autoMatRel = (modelDir / matFileName).generic_string();
									if (assetMgr->FileExists(autoMatRel))
									{
										auto matAsset = assetMgr->Get<MaterialAsset>(autoMatRel);
										if (matAsset && matAsset->IsReady())
										{
											m_Materials[i] = matAsset->GetMaterial();
											if (i >= mc.MaterialPaths.size())
											{
												mc.MaterialPaths.resize(i + 1);
											}
											mc.MaterialPaths[i] = autoMatRel;
										}
									}
								}
							}
						}
					}
				}

				materials = &m_Materials;
			}

			if (materials && !materials->empty())
			{
				// Material Selection Sidebar / List
				ImGui::BeginChild("MaterialList", ImVec2(180, 0), true);
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::InputTextWithHint("##MatFilter", ICON_FA_MAGNIFYING_GLASS " Search...", m_FilterBuffer,
										 sizeof(m_FilterBuffer));
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Filter materials by name");
				}
				ImGui::Separator();

				std::string filterStr = m_FilterBuffer;
				std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

				for (int i = 0; i < (int)materials->size(); i++)
				{
					const Material& m = (*materials)[i];
					std::string label = m.Name;
					if (label.empty())
					{
						label = "Material " + std::to_string(i);
					}

					if (!filterStr.empty())
					{
						std::string lowerLabel = label;
						std::transform(lowerLabel.begin(), lowerLabel.end(), lowerLabel.begin(), ::tolower);
						if (lowerLabel.find(filterStr) == std::string::npos)
						{
							continue;
						}
					}

					ImGui::PushID(i);
					uint32_t texHandle = GetTextureID(m.AlbedoMap, m.AlbedoPath);
					if (texHandle != 0)
					{
						ImGui::Image((ImTextureID)(uintptr_t)texHandle, ImVec2(14, 14));
					}
					else
					{
						ImVec4 swatch = {m.AlbedoColor.r, m.AlbedoColor.g, m.AlbedoColor.b, 1.0f};
						ImGui::ColorButton("##swatch", swatch,
										   ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker |
											   ImGuiColorEditFlags_NoBorder,
										   {14, 14});
					}
					ImGui::SameLine();
					if (ImGui::Selectable(label.c_str(), m_SelectedMaterialIndex == i))
					{
						m_SelectedMaterialIndex = i;
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::SetTooltip("Select material: %s", label.c_str());
					}
					ImGui::PopID();
				}
				ImGui::EndChild();

				ImGui::SameLine();

				// Material Properties — leave room for Save button
				float availH = ImGui::GetContentRegionAvail().y;
				float buttonArea = 50.0f;
				ImGui::BeginChild("MaterialProperties", ImVec2(0, availH - buttonArea));
				if (m_SelectedMaterialIndex < (int)materials->size())
				{
					Material& selected = (*materials)[m_SelectedMaterialIndex];
					std::string title =
						selected.Name.empty() ? ("Material " + std::to_string(m_SelectedMaterialIndex)) : selected.Name;
					ImGui::TextColored({0.2f, 0.8f, 1.0f, 1.0f}, ICON_FA_PALETTE " Editing: %s", title.c_str());
					ImGui::Separator();
					ImGui::Spacing();
					DrawMaterialSlot(selected);

					// Live update in memory so the viewport reflects changes immediately
					if (auto* assetMgr = ServiceLocator::TryGet<AssetManager>())
					{
						if (m_SelectedEntity.HasComponent<ModelComponent>())
						{
							auto& mc = m_SelectedEntity.GetComponent<ModelComponent>();
							auto modelAsset = assetMgr->Get<ModelAsset>(mc.ModelPath);
							if (modelAsset)
							{
								modelAsset->GetMaterials() = m_Materials;
							}
							if (m_SelectedMaterialIndex < (int)mc.MaterialPaths.size() &&
								!mc.MaterialPaths[m_SelectedMaterialIndex].empty())
							{
								auto matAsset = assetMgr->Get<MaterialAsset>(mc.MaterialPaths[m_SelectedMaterialIndex]);
								if (matAsset)
								{
									matAsset->SetMaterial(selected);
								}
							}
						}
					}
				}
				else
				{
					m_SelectedMaterialIndex = 0;
				}
				ImGui::EndChild();

				// Save & Delete buttons
				ImGui::Separator();
				float buttonWidth = ImGui::GetContentRegionAvail().x * 0.5f - 4.0f;
				if (ImGui::Button(ICON_FA_FLOPPY_DISK " Save Materials", ImVec2(buttonWidth, 0)))
				{
					SaveMaterials();
					ImGui::OpenPopup("Materials Saved");
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Save all material changes to disk");
				}

				if (ImGui::BeginPopup("Materials Saved"))
				{
					ImGui::Text("Materials saved successfully!");
					ImGui::EndPopup();
				}

				ImGui::SameLine();
				ImGui::PushStyleColor(ImGuiCol_Button, {0.7f, 0.2f, 0.2f, 1.0f});
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, {0.85f, 0.3f, 0.3f, 1.0f});
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, {0.6f, 0.15f, 0.15f, 1.0f});
				if (ImGui::Button(ICON_FA_TRASH " Delete .chmat", ImVec2(-1, 0)))
				{
					DeleteMaterials();
					ImGui::OpenPopup("Materials Deleted");
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Delete .chmat files and restore model defaults");
				}
				ImGui::PopStyleColor(3);

				if (ImGui::BeginPopup("Materials Deleted"))
				{
					ImGui::Text("Deleted .chmat files and restored model defaults!");
					ImGui::EndPopup();
				}
			}
			else
			{
				ImGui::TextColored({0.8f, 0.8f, 0.2f, 1.0f}, ICON_FA_CIRCLE_INFO " No Materials found for this entity");
			}

			ImGui::EndDisabled();
		}
		else
		{
			ImGui::Text("No entity selected.");
			ImGui::TextDisabled("Select an entity in the Hierarchy to edit its materials.");
		}

		ImGui::End();
	}

	void MaterialPanel::SaveMaterials()
	{
		if (!m_SelectedEntity || !m_SelectedEntity.IsValid())
		{
			return;
		}

		if (!m_SelectedEntity.HasComponent<ModelComponent>())
		{
			return;
		}
		if (m_Materials.empty())
		{
			return;
		}

		auto& mc = m_SelectedEntity.GetComponent<ModelComponent>();
		std::filesystem::path modelPath(mc.ModelPath);
		std::string modelName = modelPath.stem().string();
		std::filesystem::path modelDir = modelPath.parent_path();

		auto* assets = ServiceLocator::TryGet<AssetManager>();
		if (!assets)
		{
			return;
		}

		for (int i = 0; i < (int)m_Materials.size(); i++)
		{
			std::string matFileName = modelName + "_material_" + std::to_string(i) + ".chmat";
			std::string matPath;

			if (i < (int)mc.MaterialPaths.size() && !mc.MaterialPaths[i].empty())
			{
				matPath = mc.MaterialPaths[i];
			}
			else
			{
				matPath = (modelDir / matFileName).generic_string();
			}

			auto matAsset = std::make_shared<MaterialAsset>();
			matAsset->SetMaterial(m_Materials[i]);
			matAsset->SaveToFile(assets->ResolvePath(matPath));

			if (i >= (int)mc.MaterialPaths.size())
			{
				mc.MaterialPaths.resize(i + 1);
			}
			mc.MaterialPaths[i] = matPath;

			// Invalidate asset cache and update loaded instance
			assets->Invalidate(matPath);
			auto loadedMat = assets->Get<MaterialAsset>(matPath);
			if (loadedMat)
			{
				loadedMat->SetMaterial(m_Materials[i]);
			}
		}

		// Write back to ModelAsset so renderer picks up changes immediately
		auto modelAsset = assets->Get<ModelAsset>(mc.ModelPath);
		if (modelAsset)
		{
			modelAsset->GetMaterials() = m_Materials;
		}

		m_SelectedEntity.GetRegistry().patch<ModelComponent>(m_SelectedEntity, [](ModelComponent&) {});
		EditorLayer::Get().GetSceneManager().MarkSceneDirty();
	}

	void MaterialPanel::DeleteMaterials()
	{
		if (!m_SelectedEntity || !m_SelectedEntity.IsValid())
		{
			return;
		}

		if (!m_SelectedEntity.HasComponent<ModelComponent>())
		{
			return;
		}

		auto& mc = m_SelectedEntity.GetComponent<ModelComponent>();
		auto* assets = ServiceLocator::TryGet<AssetManager>();
		if (!assets)
		{
			return;
		}

		// Delete all .chmat and .meta files associated with this component
		for (const auto& matPath : mc.MaterialPaths)
		{
			if (!matPath.empty())
			{
				std::string resolved = assets->ResolvePath(matPath);
				std::error_code ec;
				std::filesystem::remove(resolved, ec);
				std::filesystem::remove(resolved + ".meta", ec);
				assets->Invalidate(matPath);
			}
		}

		// Also clean standard naming pattern <modelName>_material_*.chmat
		std::filesystem::path modelPath(mc.ModelPath);
		std::string modelName = modelPath.stem().string();
		std::filesystem::path modelDir = modelPath.parent_path();
		for (int i = 0; i < 64; ++i)
		{
			std::string matFileName = modelName + "_material_" + std::to_string(i) + ".chmat";
			std::string matRel = (modelDir / matFileName).generic_string();
			std::string resolved = assets->ResolvePath(matRel);
			if (std::filesystem::exists(resolved))
			{
				std::error_code ec;
				std::filesystem::remove(resolved, ec);
				std::filesystem::remove(resolved + ".meta", ec);
				assets->Invalidate(matRel);
			}
		}

		mc.MaterialPaths.clear();

		// Invalidate model asset and reload its original native materials
		assets->Invalidate(mc.ModelPath);
		auto modelAsset = assets->Get<ModelAsset>(mc.ModelPath);
		if (modelAsset && modelAsset->IsReady())
		{
			m_Materials = modelAsset->GetMaterials();
		}
		else
		{
			m_Materials.clear();
		}

		m_SelectedEntity.GetRegistry().patch<ModelComponent>(m_SelectedEntity, [](ModelComponent&) {});
		EditorLayer::Get().GetSceneManager().MarkSceneDirty();
	}

	void MaterialPanel::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<EntitySelectedEvent>([this](EntitySelectedEvent& ev) {
			if (Scene* scene = ev.GetScene())
			{
				m_SelectedEntity = Entity(ev.GetEntity(), &scene->GetRegistry());
			}
			m_SelectedMeshIndex = ev.GetMeshIndex();
			m_SelectedMaterialIndex = 0;
			m_Materials.clear();
			m_LoadedModelPath.clear();
			return false;
		});
	}

	void MaterialPanel::SetContext(const std::shared_ptr<Scene>& context)
	{
		if (m_Context != context)
		{
			Panel::SetContext(context);
			m_SelectedEntity = {};
			m_Materials.clear();
			m_LoadedModelPath.clear();
		}
	}

} // namespace Chained
