#ifndef CH_EDITOR_MENU_H
#define CH_EDITOR_MENU_H

#include "engine/project/project.h"
#include <string>
#include <mutex>
#include <atomic>
#include <cstdint>

namespace Chained
{

	class EditorPanels;
	class EditorLayer;

	/// @brief Handles the top-level editor menu bar and associated overlays/settings.
	class EditorMenu
	{
	public:
		EditorMenu() = default;
		~EditorMenu() = default;

		/// @brief Draws the main menu bar.
		/// @param panels The editor panels to potentially toggle via the menu.
		void DrawMenuBar(EditorPanels& panels);

		/// @brief Draws the standalone Editor Settings window (if active).
		void DrawEditorSettings();

		/// @brief Draws the export progress overlay (if an export is running).
		void DrawExportProgressOverlay();

		/// @brief Draws the export settings dialog.
		void DrawExportDialog();

	private:
		void DrawFileMenu();
		void DrawViewMenu(EditorPanels& panels);
		void DrawProjectMenu();
		void DrawEditorMenu();
		void DrawPlaybackControls();
		void DrawExportResultPopup();
		void DrawUnsavedChangesPopup();

		// State for the Export Project feature
		struct ExportState
		{
			bool Open = false;
			bool Success = false;
			std::string Message;
			std::string OutDir;
			std::mutex Mutex;
			bool IsExporting = false;

			// Progress tracking (updated from background thread under Mutex)
			uint64_t PackedFiles = 0;
			uint64_t TotalFiles = 0;
			std::string CurrentFile;
			std::chrono::steady_clock::time_point StartTime;

			// Cancel flag (written by GUI, read by worker thread)
			std::atomic<bool> CancelRequested{false};
		};

		// Export dialog state
		struct ExportDialogState
		{
			bool Open = false;
			PackMode SelectedMode = PackMode::Balanced;
			float ZipThreshold = 0.05f;
			uint32_t DataVersion = 0;
			uint32_t SplitSizeMB = 0;
			bool SplitCustom = false; // true when "Custom Size..." is explicitly selected
			std::string PackName = "resources";
			bool ForceRepack = false;
			bool SkipKtx2 = false;
			std::string OutputDir;
		};

		ExportState m_ExportState;
		ExportDialogState m_ExportDialog;
		bool m_ShowEditorSettings = false;

		// Export result popup state
		bool m_ExportResultSuccess = false;
		std::string m_ExportResultMessage;
		std::string m_ExportResultOutDir;
	};

} // namespace Chained

#endif // CH_EDITOR_MENU_H
