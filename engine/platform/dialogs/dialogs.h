#ifndef CH_DIALOGS_H
#define CH_DIALOGS_H

#include "engine/common/base.h"
#include <filesystem>
#include <string>
#include <vector>
#include <optional>

namespace Chained
{
	struct CH_API DialogFilter
	{
		std::string Name;
		std::string Spec;
	};

	enum class CH_API MessageBoxChoice
	{
		Ok,
		OkCancel,
		YesNo,
		YesNoCancel,
	};

	enum class CH_API MessageBoxIcon
	{
		Info,
		Warning,
		Error,
		Question,
	};

	enum class CH_API MessageBoxResult
	{
		Ok,
		Cancel,
		Yes,
		No,
	};

	namespace Dialogs
	{
		// Opens a file dialog and returns the selected path.
		CH_API std::optional<std::filesystem::path> OpenFile(const std::vector<DialogFilter>& filters = {});

		// Opens a save file dialog and returns the selected path.
		CH_API std::optional<std::filesystem::path> SaveFile(const std::vector<DialogFilter>& filters = {});

		// Opens a folder picker dialog and returns the selected path.
		CH_API std::optional<std::filesystem::path> PickFolder();

		// Shows a modal message box and returns the button the user clicked.
		CH_API MessageBoxResult ShowMessage(const std::string& title, const std::string& text,
											MessageBoxChoice choice = MessageBoxChoice::Ok,
											MessageBoxIcon icon = MessageBoxIcon::Info);

		// Convenience wrapper: shows a blocking error message box (OK button, error icon).
		inline void ShowError(const std::string& title, const std::string& text)
		{
			ShowMessage(title, text, MessageBoxChoice::Ok, MessageBoxIcon::Error);
		}

		// Shows a non-blocking desktop notification.
		CH_API void Notify(const std::string& title, const std::string& message,
						   MessageBoxIcon icon = MessageBoxIcon::Info);
	} // namespace Dialogs
} // namespace Chained

#endif // CH_DIALOGS_H
