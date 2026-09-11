#include "ui_font_registry.h"
#include "engine/assets/asset_manager.h"
#include "engine/common/asset_path.h"
#include "engine/core/service_locator.h"
#include "engine/project/project.h"
#include "thirdparty/imgui/imstb_truetype.h" // Або шлях, де у вас лежить stb_truetype.h
#include "engine/platform/dialogs/dialogs.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <system_error>
#include <thread>

namespace Chained
{

	// stb_truetype (used by ImGui) ONLY supports TrueType glyph outlines (glyf table).
	// It cannot render OpenType CFF/PostScript outlines ('OTTO' header, CFF/CFF2 table)
	// or Variable Fonts (fvar table).
	static bool IsValidFontMetrics(const std::filesystem::path& path)
	{
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file.is_open())
		{
			return false;
		}

		std::streamsize size = file.tellg();
		if (size <= 0)
		{
			return false;
		}
		file.seekg(0, std::ios::beg);

		std::vector<char> fontFileData(size);
		if (!file.read(fontFileData.data(), size))
		{
			return false;
		}

		stbtt_fontinfo fontInfo;
		if (!stbtt_InitFont(&fontInfo, reinterpret_cast<const unsigned char*>(fontFileData.data()), 0))
		{
			return false;
		}

		for (int cp = 33; cp <= 126; cp++)
		{
			if (cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r')
			{
				continue;
			}

			if (stbtt_FindGlyphIndex(&fontInfo, cp) == 0)
			{
				continue;
			}

			int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
			if (stbtt_GetCodepointBox(&fontInfo, cp, &x0, &y0, &x1, &y1))
			{
				if (x1 < x0 || y1 < y0)
				{
					return false;
				}
			}
		}

		return true;
	}

	static bool IsSupportedFontFormat(const std::filesystem::path& path)
	{
		std::ifstream f(path, std::ios::binary);
		if (!f.is_open())
		{
			return false;
		}

		uint8_t hdr[12];
		if (!f.read(reinterpret_cast<char*>(hdr), sizeof(hdr)))
		{
			return false;
		}

		uint32_t sfntVersion = (static_cast<uint32_t>(hdr[0]) << 24) | (static_cast<uint32_t>(hdr[1]) << 16) |
							   (static_cast<uint32_t>(hdr[2]) << 8) | static_cast<uint32_t>(hdr[3]);

		if (sfntVersion != 0x00010000 && sfntVersion != 0x74727565 /* 'true' */)
		{
			return false;
		}

		const uint16_t numTables = (static_cast<uint16_t>(hdr[4]) << 8) | hdr[5];

		for (uint16_t i = 0; i < numTables; ++i)
		{
			uint8_t rec[16];
			if (!f.read(reinterpret_cast<char*>(rec), sizeof(rec)))
			{
				break;
			}

			if ((rec[0] == 'f' && rec[1] == 'v' && rec[2] == 'a' && rec[3] == 'r') ||
				(rec[0] == 'C' && rec[1] == 'F' && rec[2] == 'F'))
			{
				return false;
			}
		}

		f.close();
		return IsValidFontMetrics(path);
	}

	std::string UIFontRegistry::NormalizeFontName(std::string name)
	{
		return NormalizeAssetPath(name);
	}

	ImFont* UIFontRegistry::CommitFont(const std::string& normalizedName, ImFont* font)
	{
		m_Fonts[normalizedName] = font;
		if (!m_DefaultFont)
		{
			m_DefaultFont = font;
		}
		if (ImGui::GetFrameCount() > 0)
		{
			m_NeedsRebuild = true;
		}
		CH_CORE_INFO("UIFontRegistry: Loaded '{}'", normalizedName);
		return font;
	}

	void UIFontRegistry::RegisterKnownFont(const std::string& relKey, const std::string& path, int& discovered)
	{
		if (m_KnownPaths.emplace(relKey, path).second)
		{
			++discovered;
		}
		// Compatibility alias between legacy "font/" and "fonts/" prefixes.
		if (relKey.rfind("font/", 0) == 0)
		{
			m_KnownPaths.emplace("fonts/" + relKey.substr(5), path);
		}
		else if (relKey.rfind("fonts/", 0) == 0)
		{
			m_KnownPaths.emplace("font/" + relKey.substr(6), path);
		}
	}

	void UIFontRegistry::LoadProjectFonts()
	{
		m_KnownPaths.clear();

		if (!Project::GetActive())
		{
			CH_CORE_WARN("UIFontRegistry: No active project, skipping font load.");
			return;
		}

		const std::filesystem::path assetDir = Project::GetActive()->GetAssetDirectory();
		const std::array<std::filesystem::path, 2> candidateDirs = {
			assetDir / "fonts",
			assetDir / "font",
		};

		const std::array<std::string, 2> validExtensions = {".ttf", ".otf"};
		int discovered = 0;

		for (const auto& fontsDir : candidateDirs)
		{
			if (!FileExists(fontsDir))
			{
				continue;
			}

			std::error_code iterError;
			std::filesystem::recursive_directory_iterator it(
				fontsDir, std::filesystem::directory_options::skip_permission_denied, iterError);
			std::filesystem::recursive_directory_iterator end;

			if (iterError)
			{
				CH_CORE_WARN("UIFontRegistry: Failed to scan '{}' ({})", fontsDir.string(), iterError.message());
				continue;
			}

			for (; it != end; it.increment(iterError))
			{
				if (iterError)
				{
					CH_CORE_WARN("UIFontRegistry: Iteration failed in '{}' ({})", fontsDir.string(),
								 iterError.message());
					iterError.clear();
					continue;
				}

				const auto& entry = *it;
				if (!entry.is_regular_file(iterError) || iterError)
				{
					iterError.clear();
					continue;
				}

				auto ext = entry.path().extension().string();
				std::transform(ext.begin(), ext.end(), ext.begin(),
							   [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });

				bool validExt = false;
				for (const auto& ve : validExtensions)
				{
					if (ext == ve)
					{
						validExt = true;
						break;
					}
				}
				if (!validExt)
				{
					continue;
				}

				auto relativePath = std::filesystem::relative(entry.path(), assetDir, iterError);
				if (iterError)
				{
					CH_CORE_WARN("UIFontRegistry: Failed to build relative path for '{}' ({})", entry.path().string(),
								 iterError.message());
					iterError.clear();
					continue;
				}

				if (!IsSupportedFontFormat(entry.path()))
				{
					CH_CORE_WARN("UIFontRegistry: Skipping font '{}' (unsupported format or CFF/Variable outlines).",
								 entry.path().string());
					continue;
				}

				std::string relKey = NormalizeFontName(relativePath.generic_string());
				const std::string absPath = entry.path().string();

				if (m_KnownPaths.find(relKey) == m_KnownPaths.end())
				{
					CH_CORE_INFO("UIFontRegistry: Discovered font '{}'", relKey);
				}
				RegisterKnownFont(relKey, absPath, discovered);
			}
		}

		// ── Also discover fonts from pack archive ─────────────────────────────────
		auto* am = ServiceLocator::TryGet<AssetManager>();
		if (am && am->IsPacked())
		{
			const std::array<std::string, 2> validExts = {".ttf", ".otf"};
			am->EnumeratePackedPaths([&](std::string_view packPath) {
				std::filesystem::path p(packPath);
				auto ext = p.extension().string();
				std::transform(ext.begin(), ext.end(), ext.begin(),
							   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

				bool validExt = false;
				for (const auto& ve : validExts)
				{
					if (ext == ve)
					{
						validExt = true;
						break;
					}
				}
				if (!validExt)
				{
					return;
				}

				// Only fonts under assets/fonts/ or assets/font/
				std::string ps(packPath);
				const bool underFonts = ps.find("assets/fonts/") == 0;
				const bool underFont = ps.find("assets/font/") == 0;
				if (!underFonts && !underFont)
				{
					return;
				}

				// Relative key: strip "assets/" prefix → "fonts/..." or "font/..."
				const std::string relKey = NormalizeFontName(ps.substr(7));
				const std::string packVirtualPath = "PACK:" + ps;

				if (m_KnownPaths.find(relKey) == m_KnownPaths.end())
				{
					CH_CORE_INFO("UIFontRegistry: Discovered pack font '{}'", relKey);
				}
				RegisterKnownFont(relKey, packVirtualPath, discovered);
			});
		}

		if (discovered == 0)
		{
			CH_CORE_INFO("UIFontRegistry: No project fonts found in '{}' or '{}' (disk or pack).",
						 (assetDir / "font").string(), (assetDir / "fonts").string());
			return;
		}

		CH_CORE_INFO("UIFontRegistry: Discovered {} font file(s).", discovered);
		// Fonts are loaded on demand via PreloadFonts() / GetFont().
		// Do not bulk-load all discovered fonts here — the atlas has finite space
		// and most fonts will never be used in a given scene.
	}

	ImFont* UIFontRegistry::EnsureDefaultProjectFont(float pixelSize, bool allowRuntimeMutation)
	{
		const float size = (pixelSize > 0.0f) ? pixelSize : 16.0f;

		if (m_KnownPaths.empty())
		{
			CH_CORE_WARN("UIFontRegistry: EnsureDefaultProjectFont — m_KnownPaths is empty, no fonts to use.");
			return nullptr;
		}

		// Pick the first discovered font that is truly a project font (font/ or fonts/ prefix).
		// Prioritize a Bold font variant by default.
		std::string chosen;
		for (const auto& [name, absPath] : m_KnownPaths)
		{
			if (name.rfind("font/", 0) == 0 || name.rfind("fonts/", 0) == 0)
			{
				std::string lowerName = name;
				std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
				if (lowerName.find("bold") != std::string::npos && lowerName.find("italic") == std::string::npos)
				{
					if (IsSupportedFontFormat(std::filesystem::path(absPath)))
					{
						chosen = name;
						break;
					}
				}
			}
		}

		if (chosen.empty())
		{
			for (const auto& [name, absPath] : m_KnownPaths)
			{
				if (name.rfind("font/", 0) == 0 || name.rfind("fonts/", 0) == 0)
				{
					std::string lowerName = name;
					std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
					if (lowerName.find("arial") != std::string::npos)
					{
						if (IsSupportedFontFormat(std::filesystem::path(absPath)))
						{
							chosen = name;
							break;
						}
					}
				}
			}
		}

		if (chosen.empty())
		{
			for (const auto& [name, absPath] : m_KnownPaths)
			{
				if (name.rfind("font/", 0) == 0 || name.rfind("fonts/", 0) == 0)
				{
					std::string lowerName = name;
					std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
					if (lowerName.find("regular") != std::string::npos ||
						lowerName.find("alansans") != std::string::npos)
					{
						if (IsSupportedFontFormat(std::filesystem::path(absPath)))
						{
							chosen = name;
							break;
						}
					}
				}
			}
		}

		if (chosen.empty())
		{
			for (const auto& [name, absPath] : m_KnownPaths)
			{
				if (name.rfind("font/", 0) == 0 || name.rfind("fonts/", 0) == 0)
				{
					if (IsSupportedFontFormat(std::filesystem::path(absPath)))
					{
						chosen = name;
						break;
					}
				}
			}
		}

		if (chosen.empty())
		{
			CH_CORE_WARN("UIFontRegistry: EnsureDefaultProjectFont — no supported font found in {} known path(s).",
						 m_KnownPaths.size());
			return nullptr;
		}

		CH_CORE_INFO("UIFontRegistry: EnsureDefaultProjectFont — chosen '{}' at {}px (runtime={})", chosen, size,
					 allowRuntimeMutation);
		PreloadFonts({{chosen, size}}, allowRuntimeMutation);
		ImFont* result = GetFont(chosen, size);
		if (!result)
		{
			CH_CORE_WARN(
				"UIFontRegistry: EnsureDefaultProjectFont — GetFont('{}', {}) returned nullptr after PreloadFonts.",
				chosen, size);
		}
		if (result && !m_DefaultFont)
		{
			m_DefaultFont = result;
		}
		return result;
	}

	ImFont* UIFontRegistry::GetFont(const std::string& relativeName, float pixelSize)
	{
		const std::string normalizedName = NormalizeFontName(relativeName);
		if (normalizedName.empty() || normalizedName == "Default" || normalizedName == "default")
		{
			if (m_DefaultFont)
			{
				return m_DefaultFont;
			}
			return EnsureDefaultProjectFont(pixelSize, true);
		}

		// Fonts are keyed by name only — with ImGui 1.92 dynamic fonts, one ImFont*
		// renders at any size (pass the size to PushFont()/ImDrawList::AddText()).
		auto it = m_Fonts.find(normalizedName);
		if (it != m_Fonts.end())
		{
			if (it->second)
			{
				return it->second;
			}
			m_Fonts.erase(it);
		}

		m_FailedFonts.erase(normalizedName);

		// Lazy registration. Safe at any time with the dynamic font atlas
		// (ImGuiBackendFlags_RendererHasTextures) — glyphs bake on demand.
		auto pathIt = m_KnownPaths.find(normalizedName);
		if (pathIt != m_KnownPaths.end())
		{
			return RegisterFont(normalizedName, pathIt->second, pixelSize);
		}

		std::filesystem::path directPath(normalizedName);
		if (directPath.is_absolute() && FileExists(directPath))
		{
			return RegisterFont(normalizedName, directPath.string(), pixelSize);
		}

		CH_CORE_WARN("UIFontRegistry: Font '{}' not discovered. Check assets/font or assets/fonts.", normalizedName);
		m_FailedFonts.insert(normalizedName);
		return nullptr;
	}

	const ImFont* UIFontRegistry::GetFont(const std::string& relativeName, float pixelSize) const
	{
		const std::string normalizedName = NormalizeFontName(relativeName);
		if (normalizedName.empty() || normalizedName == "Default")
		{
			return nullptr;
		}

		auto it = m_Fonts.find(normalizedName);
		if (it != m_Fonts.end())
		{
			return it->second;
		}
		return nullptr;
	}

	int UIFontRegistry::PreloadFonts(const std::vector<std::pair<std::string, float>>& requests,
									 bool allowRuntimeMutation)
	{
		if (requests.empty())
		{
			return 0;
		}

		if (ImGui::GetFrameCount() > 0 && !allowRuntimeMutation)
		{
			CH_CORE_WARN("UIFontRegistry: Ignoring preload request after first frame (runtime mutation disabled).");
			return 0;
		}

		int loaded = 0;
		std::unordered_set<std::string> seenKeys;
		std::unordered_set<std::string> missingFonts;

		for (const auto& [fontNameRaw, pixelSizeRaw] : requests)
		{
			const std::string fontName = NormalizeFontName(fontNameRaw);
			if (fontName.empty() || fontName == "Default")
			{
				continue;
			}

			const float pixelSize = (pixelSizeRaw > 0.0f) ? pixelSizeRaw : 16.0f;
			if (!seenKeys.insert(fontName).second)
			{
				continue;
			}

			if (m_Fonts.find(fontName) != m_Fonts.end())
			{
				continue;
			}

			std::string absolutePath;
			auto knownPathIt = m_KnownPaths.find(fontName);
			if (knownPathIt != m_KnownPaths.end())
			{
				absolutePath = knownPathIt->second;
			}
			else
			{
				std::filesystem::path directPath(fontName);
				if (directPath.is_absolute() && FileExists(directPath))
				{
					absolutePath = directPath.string();
				}
			}

			if (absolutePath.empty())
			{
				missingFonts.insert(fontName);
				continue;
			}

			if (RegisterFont(fontName, absolutePath, pixelSize))
			{
				loaded++;
			}
		}

		for (const auto& fontName : missingFonts)
		{
			CH_CORE_WARN("UIFontRegistry: Missing font '{}' requested by runtime scene.", fontName);
		}

		return loaded;
	}

	ImFont* UIFontRegistry::RegisterFont(const std::string& relativeName, const std::string& absolutePath,
										 float pixelSize)
	{
		const std::string normalizedName = NormalizeFontName(relativeName);
		if (normalizedName.empty())
		{
			return nullptr;
		}

		// Already registered (or a cached failure) — don't touch the atlas again.
		{
			auto it = m_Fonts.find(normalizedName);
			if (it != m_Fonts.end())
			{
				return it->second;
			}
		}

		// ── Pack font: load raw bytes via AssetManager, use AddFontFromMemoryTTF ──
		// absolutePath starts with "PACK:" when the font was discovered inside the
		// pack archive (set by LoadProjectFonts). IsSupportedFontFormat is skipped
		// because it needs a real file path; ImGui returns nullptr on unsupported data.
		static constexpr std::string_view kPackPrefix = "PACK:";
		if (absolutePath.size() > kPackPrefix.size() && absolutePath.compare(0, kPackPrefix.size(), kPackPrefix) == 0)
		{
			auto* am = ServiceLocator::TryGet<AssetManager>();
			if (am)
			{
				const std::string packKey = absolutePath.substr(kPackPrefix.size());
				auto data = am->ReadAssetData(packKey);
				if (!data.empty())
				{
					// Keep the buffer alive — ImGui does NOT own it (FontDataOwnedByAtlas=false)
					auto& buf = m_PackFontData[normalizedName];
					buf = std::move(data);

					ImFontConfig cfg;
					cfg.FontDataOwnedByAtlas = false;
					const float defaultSize = (pixelSize > 0.0f) ? pixelSize : 16.0f;
					ImGuiIO& io = ImGui::GetIO();
					ImFont* font = io.Fonts->AddFontFromMemoryTTF(buf.data(), static_cast<int>(buf.size()), defaultSize,
																  &cfg, io.Fonts->GetGlyphRangesCyrillic());
					if (font)
					{
						m_FailedFonts.erase(normalizedName);
						return CommitFont(normalizedName, font);
					}
				}
			}
			CH_CORE_WARN("UIFontRegistry: Failed to load pack font '{}'", absolutePath);
			m_FailedFonts.insert(normalizedName);
			return nullptr;
		}

		if (!IsSupportedFontFormat(std::filesystem::path(absolutePath)))
		{
			CH_CORE_WARN("UIFontRegistry: Skipped font '{}' (unsupported format or CFF/Variable outlines).",
						 absolutePath);
			m_FailedFonts.insert(normalizedName);
			return nullptr;
		}

		// The size passed here is only the legacy default — with dynamic fonts the
		// actual render size comes from PushFont()/AddText() at each call site.
		const float defaultSize = (pixelSize > 0.0f) ? pixelSize : 16.0f;

		ImGuiIO& io = ImGui::GetIO();
		ImFont* font = io.Fonts->AddFontFromFileTTF(absolutePath.c_str(), defaultSize, nullptr,
													io.Fonts->GetGlyphRangesCyrillic());
		if (!font)
		{
			CH_CORE_ERROR("UIFontRegistry: Failed to load font '{}'", absolutePath);
			m_FailedFonts.insert(normalizedName);
			return nullptr;
		}

		m_FailedFonts.erase(normalizedName);
		return CommitFont(normalizedName, font);
	}

	void UIFontRegistry::Clear()
	{
		m_Fonts.clear();
		m_FailedFonts.clear();
		m_KnownPaths.clear();
		m_PackFontData.clear();
		m_DefaultFont = nullptr;
	}

	std::vector<std::string> UIFontRegistry::GetKnownFontNames() const
	{
		std::vector<std::string> names;
		names.reserve(m_KnownPaths.size());
		for (const auto& [name, _] : m_KnownPaths)
		{
			names.push_back(name);
		}
		std::sort(names.begin(), names.end());
		return names;
	}
} // namespace Chained