#include "editor/panels/content_browser_panel.h"
#include "editor/action_commands.h"
#include "editor/events.h"
#include "editor/layer.h"
#include "engine/core/log.h"
#include "engine/project/project.h"
#include "engine/scene/components.h"
#include "engine/scene/prefab_serializer.h"
#include "engine/scene/scene_events.h"
#include "imgui.h"
#include "thirdparty/IconsFontAwesome6.h"
#include <algorithm>
#include <unordered_map>

namespace Chained
{

	ContentBrowserPanel::ContentBrowserPanel()
	{
		m_Name = "Content Browser";

		if (auto project = Project::GetActive())
		{
			SetRoot(project->GetConfig().ProjectDirectory / project->GetConfig().AssetDirectory);
		}
		else
		{
			SetRoot(std::filesystem::current_path() / "assets");
		}

		m_ThumbnailSize = EditorLayer::Get().GetConfig().DefaultThumbnailSize;
	}

	ContentBrowserPanel::~ContentBrowserPanel() = default;

	void ContentBrowserPanel::OnImGuiRender(bool readOnly)
	{
		if (!m_NextDirectory.empty())
		{
			Navigate(m_NextDirectory);
			m_NextDirectory.clear();
		}

		if (!m_IsOpen)
		{
			return;
		}

		ImGui::Begin(m_Name.c_str(), &m_IsOpen);
		ImGui::BeginDisabled(readOnly);

		RenderToolbar();
		ImGui::Separator();
		RenderGridView();

		ImGui::EndDisabled();
		ImGui::End();
	}

	void ContentBrowserPanel::OnEvent(Event& e)
	{
		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<ProjectOpenedEvent>([this](ProjectOpenedEvent& e) {
			if (auto project = Project::GetActive())
			{
				SetRoot(project->GetConfig().ProjectDirectory / project->GetConfig().AssetDirectory);
			}
			return false;
		});
	}

	void ContentBrowserPanel::RenderToolbar()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 2));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(0, 0));

		if (GetCurrentDirectory() != GetRootDirectory())
		{
			if (ImGui::Button(ICON_FA_ARROW_LEFT))
			{
				GoUp();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Go to parent directory");
			}
		}
		else
		{
			ImGui::BeginDisabled(true);
			ImGui::Button(ICON_FA_ARROW_LEFT);
			ImGui::EndDisabled();
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(200);

		if (ImGui::InputTextWithHint("##Search", ICON_FA_MAGNIFYING_GLASS " Search...", m_FilterBuffer,
									 sizeof(m_FilterBuffer)))
		{
			SetFilter(m_FilterBuffer, m_FilterType);
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Filter assets by name");
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(150);
		const char* filterNames[] = {"All Types", "Scenes", "Prefabs", "Models", "Textures", "Scripts", "Audio"};
		if (ImGui::BeginCombo("##TypeFilter", filterNames[m_FilterType]))
		{
			for (int i = 0; i < IM_ARRAYSIZE(filterNames); i++)
			{
				if (ImGui::Selectable(filterNames[i], m_FilterType == i))
				{
					m_FilterType = i;
					SetFilter(m_FilterBuffer, m_FilterType);
				}
			}
			ImGui::EndCombo();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Filter assets by type");
		}

		ImGui::SameLine();
		if (ImGui::Button("Assets"))
		{
			GoToRoot();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Navigate to project assets root");
		}

		// Breadcrumbs
		std::error_code ec;
		auto relPath = std::filesystem::relative(GetCurrentDirectory(), GetRootDirectory(), ec);
		if (!ec && !relPath.empty() && relPath != ".")
		{
			std::filesystem::path accumulated = GetRootDirectory();
			for (const auto& part : relPath)
			{
				ImGui::SameLine();
				ImGui::Text("/");
				ImGui::SameLine();
				accumulated /= part;
				if (ImGui::Button(part.string().c_str()))
				{
					Navigate(accumulated);
					break;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Open folder: %s", accumulated.string().c_str());
				}
			}
		}

		ImGui::SameLine(ImGui::GetWindowWidth() - 160.0f);
		ImGui::SetNextItemWidth(150.0f);
		ImGui::SliderFloat("##IconScale", &m_IconScale, 0.5f, 2.0f, ICON_FA_IMAGE);
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Adjust thumbnail icon size");
		}

		ImGui::PopStyleVar(2);
	}

	void ContentBrowserPanel::RenderGridView()
	{
		float cellSize = (m_ThumbnailSize * m_IconScale) + m_Padding;
		float panelWidth = ImGui::GetContentRegionAvail().x;
		int columnCount = std::max(1, (int)(panelWidth / cellSize));

		ImGui::Columns(columnCount, nullptr, false);

		const auto& assets = GetAssets();
		if (assets.empty())
		{
			ImGui::TextDisabled("Empty directory or No assets found matching filters.");
			ImGui::Columns(1);
		}
		else
		{
			int i = 0;
			for (const auto& asset : assets)
			{
				ImGui::PushID(i++);
				const char* icon = ICON_FA_FOLDER;
				if (!asset.isDirectory)
				{
					switch (asset.type)
					{
					case EditorAssetType::Scene:
						icon = ICON_FA_CUBES;
						break;
					case EditorAssetType::Script:
						icon = ICON_FA_FILE_CODE;
						break;
					case EditorAssetType::Model:
						icon = ICON_FA_SHAPES;
						break;
					case EditorAssetType::Texture:
						icon = ICON_FA_IMAGE;
						break;
					case EditorAssetType::Audio:
						icon = ICON_FA_MUSIC;
						break;
					case EditorAssetType::Prefab:
						icon = ICON_FA_CUBE;
						break;
					case EditorAssetType::Shader:
						icon = ICON_FA_CODE;
						break;
					default:
						icon = ICON_FA_FILE;
						break;
					}
				}

				ImGui::BeginGroup();
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
				ImGui::Button(icon, {cellSize - m_Padding, m_ThumbnailSize * m_IconScale});

				if (ImGui::BeginPopupContextItem())
				{
					if (ImGui::MenuItem(ICON_FA_PEN " Rename"))
					{
						m_RenamingPath = asset.path;
						strncpy(m_RenameBuffer, asset.name.c_str(), sizeof(m_RenameBuffer) - 1);
						m_RenameBuffer[sizeof(m_RenameBuffer) - 1] = '\0';
						m_OpenRenamePopup = true;
					}
					if (ImGui::MenuItem(ICON_FA_TRASH " Delete"))
					{
						m_PathToDelete = asset.path;
						m_OpenDeletePopup = true;
					}
					ImGui::EndPopup();
				}

				if (ImGui::IsItemHovered())
				{
					if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
					{
						OnAssetDoubleClicked(asset);
					}
					ImGui::SetTooltip("%s%s", asset.isDirectory ? "Folder: " : "File: ", asset.name.c_str());
				}

				if (ImGui::BeginDragDropSource())
				{
					std::string pathStr = asset.path.string();
					ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", pathStr.c_str(), pathStr.size() + 1);
					ImGui::Text("%s %s", icon, asset.name.c_str());
					ImGui::EndDragDropSource();
				}

				ImGui::PopStyleColor();
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
				ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + cellSize - m_Padding);
				ImGui::TextUnformatted(asset.name.c_str());
				ImGui::PopTextWrapPos();
				ImGui::PopStyleVar();
				ImGui::EndGroup();

				ImGui::NextColumn();
				ImGui::PopID();
			}
			ImGui::Columns(1);
		}

		if (m_OpenRenamePopup)
		{
			ImGui::OpenPopup("RenameAsset");
			m_OpenRenamePopup = false;
		}
		if (m_OpenDeletePopup)
		{
			ImGui::OpenPopup("DeleteAsset?");
			m_OpenDeletePopup = false;
		}

		if (ImGui::BeginPopupModal("RenameAsset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Enter new name:");
			ImGui::InputText("##NewName", m_RenameBuffer, sizeof(m_RenameBuffer));
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Enter the new asset name");
			}
			if (ImGui::Button("OK", {120, 0}))
			{
				EditorActionCommands::RenameAsset(m_RenamingPath, m_RenameBuffer);
				m_PendingRefresh = true;
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Confirm rename");
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", {120, 0}))
			{
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Cancel rename");
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginPopupModal("DeleteAsset?", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Delete %s?", m_PathToDelete.filename().string().c_str());
			if (ImGui::Button("Delete", {120, 0}))
			{
				EditorActionCommands::DeleteAsset(m_PathToDelete);
				m_PendingRefresh = true;
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Permanently delete this asset");
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel", {120, 0}))
			{
				ImGui::CloseCurrentPopup();
			}
			if (ImGui::IsItemHovered())
			{
				ImGui::SetTooltip("Cancel deletion");
			}
			ImGui::EndPopup();
		}

		if (ImGui::BeginPopupContextWindow(0, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			if (ImGui::BeginMenu(ICON_FA_PLUS " Create"))
			{
				if (ImGui::MenuItem(ICON_FA_FOLDER " New Folder"))
				{
					EditorActionCommands::CreateFolder(GetCurrentDirectory());
					m_PendingRefresh = true;
				}
				if (ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Create a new folder in the current directory");
				}
				ImGui::EndMenu();
			}
			ImGui::EndPopup();
		}

		if (m_PendingRefresh)
		{
			m_PendingRefresh = false;
			Refresh();
		}
	}

	void ContentBrowserPanel::OnAssetDoubleClicked(const AssetEntry& entry)
	{
		if (entry.isDirectory)
		{
			m_NextDirectory = entry.path;
			return;
		}

		if (entry.type == EditorAssetType::Scene)
		{
			EditorLayer::Get().GetSceneManager().OpenScene(entry.path);
			return;
		}

		auto scene = EditorLayer::Get().GetSceneManager().GetActiveScene();
		if (!scene)
		{
			return;
		}

		if (entry.type == EditorAssetType::Prefab)
		{
			PrefabSerializer::Deserialize(scene.get(), entry.path.string());
		}
		if (entry.type == EditorAssetType::Model)
		{
			Entity entity = scene->CreateEntity(entry.name);
			auto& modelcomp = entity.AddComponent<ModelComponent>();
			modelcomp.ModelPath = Project::GetActive()->GetRelativePath(entry.path);
			SelectEntity(entity, scene.get());
		}
		if (entry.type == EditorAssetType::Texture)
		{
			Entity entity = scene->CreateEntity(entry.name);
			auto& sprite = entity.AddComponent<SpriteComponent>();
			sprite.TexturePath = Project::GetActive()->GetRelativePath(entry.path);
			SelectEntity(entity, scene.get());
		}
		if (entry.type == EditorAssetType::Audio)
		{
			Entity entity = scene->CreateEntity(entry.name);
			auto& audiocomp = entity.AddComponent<AudioComponent>();
			audiocomp.SoundPath = entry.path.string();
			SelectEntity(entity, scene.get());
		}
		if (entry.type == EditorAssetType::Shader)
		{
			Entity entity = scene->CreateEntity(entry.name);
			auto& shader = entity.AddComponent<ShaderComponent>();
			shader.ShaderPath = Project::GetActive()->GetRelativePath(entry.path);
			SelectEntity(entity, scene.get());
		}
	}

	void ContentBrowserPanel::SetRoot(const std::filesystem::path& path)
	{
		m_RootDirectory = path;
		m_CurrentDirectory = path;
		Scan();
	}

	void ContentBrowserPanel::SetFilter(const std::string& query, int typeFilter)
	{
		m_FilterQuery = query;
		std::transform(m_FilterQuery.begin(), m_FilterQuery.end(), m_FilterQuery.begin(), ::tolower);
		m_ContentFilterType = typeFilter;
		Scan();
	}

	void ContentBrowserPanel::Refresh()
	{
		Scan();
	}

	void ContentBrowserPanel::Navigate(const std::filesystem::path& path)
	{
		m_CurrentDirectory = path;
		Scan();
	}

	void ContentBrowserPanel::GoUp()
	{
		if (m_CurrentDirectory != m_RootDirectory)
		{
			m_CurrentDirectory = m_CurrentDirectory.parent_path();
			Scan();
		}
	}

	void ContentBrowserPanel::GoToRoot()
	{
		m_CurrentDirectory = m_RootDirectory;
		Scan();
	}

	void ContentBrowserPanel::Scan()
	{
		m_CurrentAssets.clear();
		std::error_code ec;

		if (!std::filesystem::exists(m_CurrentDirectory, ec))
		{
			return;
		}

		for (auto& p : std::filesystem::directory_iterator(m_CurrentDirectory, ec))
		{
			AssetEntry entry;
			entry.name = p.path().filename().string();
			entry.path = p.path();
			entry.isDirectory = p.is_directory();
			entry.type = DetermineAssetType(p.path());

			if (!m_FilterQuery.empty())
			{
				std::string nameLower = entry.name;
				std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
				if (nameLower.find(m_FilterQuery) == std::string::npos)
				{
					continue;
				}
			}

			if (!entry.isDirectory && m_ContentFilterType > 0)
			{
				static constexpr EditorAssetType kFilterTypes[] = {EditorAssetType::Scene,	EditorAssetType::Prefab,
																   EditorAssetType::Model,	EditorAssetType::Texture,
																   EditorAssetType::Script, EditorAssetType::Audio};
				static constexpr int kFilterTypeCount = sizeof(kFilterTypes) / sizeof(kFilterTypes[0]);

				bool match = false;
				if (m_ContentFilterType - 1 < kFilterTypeCount)
				{
					match = (entry.type == kFilterTypes[m_ContentFilterType - 1]);
				}
				if (!match)
				{
					continue;
				}
			}

			m_CurrentAssets.push_back(entry);
		}

		std::sort(m_CurrentAssets.begin(), m_CurrentAssets.end(), [](const AssetEntry& a, const AssetEntry& b) {
			if (a.isDirectory != b.isDirectory)
			{
				return a.isDirectory > b.isDirectory;
			}
			return a.name < b.name;
		});
	}

	EditorAssetType ContentBrowserPanel::DetermineAssetType(const std::filesystem::path& path)
	{
		if (std::filesystem::is_directory(path))
		{
			return EditorAssetType::Directory;
		}

		static const std::unordered_map<std::string, EditorAssetType> s_ExtensionMap = {
			{".chscene", EditorAssetType::Scene},	{".chmap", EditorAssetType::Scene},
			{".chprefab", EditorAssetType::Prefab}, {".h", EditorAssetType::Script},
			{".cpp", EditorAssetType::Script},		{".cs", EditorAssetType::Script},
			{".obj", EditorAssetType::Model},		{".gltf", EditorAssetType::Model},
			{".glb", EditorAssetType::Model},		{".png", EditorAssetType::Texture},
			{".jpg", EditorAssetType::Texture},		{".tga", EditorAssetType::Texture},
			{".bmp", EditorAssetType::Texture},		{".wav", EditorAssetType::Audio},
			{".ogg", EditorAssetType::Audio},		{".mp3", EditorAssetType::Audio},
			{".glsl", EditorAssetType::Shader},		{".vs", EditorAssetType::Shader},
			{".fs", EditorAssetType::Shader},		{".vert", EditorAssetType::Shader},
			{".frag", EditorAssetType::Shader}};

		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		auto it = s_ExtensionMap.find(ext);
		return (it != s_ExtensionMap.end()) ? it->second : EditorAssetType::Other;
	}

} // namespace Chained
