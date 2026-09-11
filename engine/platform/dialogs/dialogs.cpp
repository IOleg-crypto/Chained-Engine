#include "dialogs.h"
#include "engine/core/log.h"
#include "portable-file-dialogs.h"

namespace Chained
{
	namespace
	{
		void CheckLinuxDialogBackend()
		{
#if !defined(_WIN32) && !defined(__APPLE__)
			static bool s_Checked = false;
			if (!s_Checked)
			{
				s_Checked = true;
				if (!pfd::settings::available())
				{
					CH_CORE_WARN("Dialogs: No graphical file dialog backend found (zenity, kdialog, or yad). "
								 "Please install zenity: 'sudo apt install zenity'");
				}
			}
#endif
		}

		// portable-file-dialogs expects each filter as a pair of
		// [human-readable name, space-separated glob patterns], e.g. {"Textures", "*.png *.jpg"}.
		// Our DialogFilter::Spec is authored as a bare, comma-separated extension list
		// ("png,jpg,tga" or "chscene"), so translate it into the glob form PFD understands.
		// Already-globbed specs ("*.png", "*") are passed through untouched.
		std::string NormalizeSpec(const std::string& fileFilterSpec)
		{
			if (fileFilterSpec.empty())
			{
				return "*";
			}

			// If the caller already provided wildcard globs, trust them as-is.
			if (fileFilterSpec.find('*') != std::string::npos)
			{
				return fileFilterSpec;
			}

			std::string result;
			std::stringstream stream(fileFilterSpec);
			std::string ext;
			while (std::getline(stream, ext, ','))
			{
				// Trim surrounding whitespace and a leading dot ("."chscene / ".png").
				size_t start = ext.find_first_not_of(" \t.");
				size_t end = ext.find_last_not_of(" \t");
				if (start == std::string::npos)
				{
					continue;
				}
				ext = ext.substr(start, end - start + 1);
				if (ext.empty())
				{
					continue;
				}

				if (!result.empty())
				{
					result += ' ';
				}
				result += "*." + ext;
			}

			return result.empty() ? "*" : result;
		}

		// Builds the flat [name, spec, name, spec, ...] vector PFD consumes, converting each
		// spec to glob form and always appending an "All Files" escape hatch.
		std::vector<std::string> BuildPfdFilters(const std::vector<DialogFilter>& filters)
		{
			std::vector<std::string> pfdFilters;
			for (const auto& filter : filters)
			{
				pfdFilters.push_back(filter.Name);
				pfdFilters.push_back(NormalizeSpec(filter.Spec));
			}
			pfdFilters.push_back("All Files");
			pfdFilters.push_back("*");
			return pfdFilters;
		}
	} // namespace

	std::optional<std::filesystem::path> Dialogs::OpenFile(const std::vector<DialogFilter>& filters)
	{
		CheckLinuxDialogBackend();
		pfd::open_file dialog("Open File", "", BuildPfdFilters(filters));
		auto result = dialog.result();

		if (result.empty())
		{
			return std::nullopt;
		}

		return std::filesystem::path(result[0]);
	}

	std::optional<std::filesystem::path> Dialogs::SaveFile(const std::vector<DialogFilter>& filters)
	{
		CheckLinuxDialogBackend();
		pfd::save_file dialog("Save File", "", BuildPfdFilters(filters));
		auto result = dialog.result();

		if (result.empty())
		{
			return std::nullopt;
		}

		return std::filesystem::path(result);
	}

	std::optional<std::filesystem::path> Dialogs::PickFolder()
	{
		CheckLinuxDialogBackend();
		pfd::select_folder dialog("Select Folder");
		auto result = dialog.result();

		if (result.empty())
		{
			return std::nullopt;
		}

		return std::filesystem::path(result);
	}

	MessageBoxResult Dialogs::ShowMessage(const std::string& title, const std::string& text, MessageBoxChoice choice,
										  MessageBoxIcon icon)
	{
		pfd::choice pfdChoice;
		switch (choice)
		{
		case MessageBoxChoice::Ok:
			pfdChoice = pfd::choice::ok;
			break;
		case MessageBoxChoice::OkCancel:
			pfdChoice = pfd::choice::ok_cancel;
			break;
		case MessageBoxChoice::YesNo:
			pfdChoice = pfd::choice::yes_no;
			break;
		case MessageBoxChoice::YesNoCancel:
			pfdChoice = pfd::choice::yes_no_cancel;
			break;
		}

		pfd::icon pfdIcon;
		switch (icon)
		{
		case MessageBoxIcon::Info:
			pfdIcon = pfd::icon::info;
			break;
		case MessageBoxIcon::Warning:
			pfdIcon = pfd::icon::warning;
			break;
		case MessageBoxIcon::Error:
			pfdIcon = pfd::icon::error;
			break;
		case MessageBoxIcon::Question:
			pfdIcon = pfd::icon::question;
			break;
		}

		pfd::message dialog(title, text, pfdChoice, pfdIcon);
		auto result = dialog.result();

		switch (result)
		{
		case pfd::button::ok:
			return MessageBoxResult::Ok;
		case pfd::button::cancel:
			return MessageBoxResult::Cancel;
		case pfd::button::yes:
			return MessageBoxResult::Yes;
		case pfd::button::no:
			return MessageBoxResult::No;
		default:
			return MessageBoxResult::Cancel;
		}
	}

	void Dialogs::Notify(const std::string& title, const std::string& message, MessageBoxIcon icon)
	{
		pfd::icon pfdIcon;
		switch (icon)
		{
		case MessageBoxIcon::Info:
			pfdIcon = pfd::icon::info;
			break;
		case MessageBoxIcon::Warning:
			pfdIcon = pfd::icon::warning;
			break;
		case MessageBoxIcon::Error:
			pfdIcon = pfd::icon::error;
			break;
		case MessageBoxIcon::Question:
			pfdIcon = pfd::icon::question;
			break;
		}

		pfd::notify(title, message, pfdIcon);
	}
} // namespace Chained
