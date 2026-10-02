#include "font_manager.h"
#include "gui.h"
#include "engine/app/application.h"
#include "engine/assets/asset_manager.h"
#include "engine/core/service_locator.h"
#include "engine/ui/ui_font_registry.h"
#include "engine/ui/widget_renderer.h"
#include "engine/imgui/imgui_layer.h"
#include "thirdparty/IconsFontAwesome6.h"

namespace Chained
{

	FontManager::FontManager(EditorConfig& config)
		: m_Config(config)
	{
	}

	void FontManager::AddFontsToAtlas()
	{
		auto* imguiLayer = Application::Get().GetImGuiLayer();
		if (!imguiLayer)
		{
			return;
		}

		float fontSize = m_Config.FontSize > 0.0f ? m_Config.FontSize : 16.0f;
		auto* assetManager = ServiceLocator::TryGet<AssetManager>();
		if (!assetManager)
		{
			return;
		}
		std::string relFont =
			!m_Config.FontPath.empty() ? m_Config.FontPath : "engine/resources/font/lato/lato-bold.ttf";
		std::string fontPath = assetManager->ResolvePath(relFont);

		bool baseFontLoaded = false;

		if (std::filesystem::exists(fontPath))
		{
			imguiLayer->AddFontFromFile(fontPath, fontSize, nullptr, ImGui::GetIO().Fonts->GetGlyphRangesCyrillic());
			CH_CORE_INFO("Loaded editor font: {} @ {}px (with Cyrillic)", fontPath, fontSize);
			baseFontLoaded = true;
		}
		else
		{
			CH_CORE_WARN("Editor font not found: {}. Using default ImGui font.", fontPath);
			ImGui::GetIO().Fonts->AddFontDefault();
		}

		// --- Icon Font (FontAwesome) ---
		std::string faPath = assetManager->ResolvePath("engine/resources/font/fa-solid-900.ttf");
		if (baseFontLoaded && std::filesystem::exists(faPath))
		{
			ImFontConfig icons_config;
			icons_config.MergeMode = true;
			icons_config.PixelSnapH = true;

			static const ImWchar* font_awesome_ranges = nullptr;
			if (!font_awesome_ranges)
			{
				static const ImWchar ranges[] = {ICON_MIN_FA, ICON_MAX_16_FA, 0};
				font_awesome_ranges = ranges;
			}

			imguiLayer->AddFontFromFile(faPath, fontSize, &icons_config, font_awesome_ranges);
			CH_CORE_INFO("Loaded and merged FontAwesome for editor: {}", faPath);
		}
	}

	void FontManager::LoadFonts()
	{
		AddFontsToAtlas();
		Application::Get().GetImGuiLayer()->RefreshFontAtlasTexture();
	}

	void FontManager::ReloadFonts()
	{
		auto* imguiLayer = Application::Get().GetImGuiLayer();
		if (!imguiLayer)
		{
			return;
		}

		imguiLayer->ClearFonts();

		if (auto* fontRegistry = ServiceLocator::TryGet<UIFontRegistry>())
		{
			fontRegistry->Clear();
		}

		AddFontsToAtlas();
		EditorGUI::ApplyTheme();

		if (auto* widgetRenderer = ServiceLocator::TryGet<WidgetRenderer>())
		{
			widgetRenderer->LoadProjectFonts();
		}

		imguiLayer->RefreshFontAtlasTexture();
	}

	void FontManager::RequestReload()
	{
		m_PendingReload = true;
	}

} // namespace Chained
