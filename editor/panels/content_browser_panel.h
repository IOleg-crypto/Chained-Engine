#ifndef CH_CONTENT_BROWSER_PANEL_H
#define CH_CONTENT_BROWSER_PANEL_H

#include "editor/asset_types.h"
#include "panel.h"
#include <vector>

namespace Chained
{

	class ContentBrowserPanel : public Panel
	{
	public:
		ContentBrowserPanel();
		~ContentBrowserPanel() override;

		void OnImGuiRender(bool readOnly = false) override;
		void OnEvent(Event& e) override;

	private:
		void RenderToolbar();
		void RenderGridView();

		void OnAssetDoubleClicked(const AssetEntry& entry);

		void Scan();
		EditorAssetType DetermineAssetType(const std::filesystem::path& path);

		const std::vector<AssetEntry>& GetAssets() const
		{
			return m_CurrentAssets;
		}
		const std::filesystem::path& GetCurrentDirectory() const
		{
			return m_CurrentDirectory;
		}
		const std::filesystem::path& GetRootDirectory() const
		{
			return m_RootDirectory;
		}

		void SetRoot(const std::filesystem::path& path);
		void SetFilter(const std::string& query, int typeFilter);
		void Refresh();
		void Navigate(const std::filesystem::path& path);
		void GoUp();
		void GoToRoot();

	private:
		std::filesystem::path m_RootDirectory;
		std::filesystem::path m_CurrentDirectory;
		std::vector<AssetEntry> m_CurrentAssets;

		std::string m_FilterQuery;
		int m_ContentFilterType = 0;

		float m_ThumbnailSize = 96.0f;
		float m_Padding = 16.0f;
		float m_IconScale = 1.0f;

		char m_FilterBuffer[128] = "";
		int m_FilterType = 0;

		std::filesystem::path m_RenamingPath;
		char m_RenameBuffer[256] = "";
		std::filesystem::path m_PathToDelete;
		std::filesystem::path m_NextDirectory;
		bool m_OpenRenamePopup = false;
		bool m_OpenDeletePopup = false;
		bool m_PendingRefresh = false;
	};

} // namespace Chained

#endif // CH_CONTENT_BROWSER_PANEL_H
