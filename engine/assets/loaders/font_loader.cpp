#include "engine/assets/loaders/font_loader.h"
#include "engine/assets/types/font_asset.h"
#include "engine/graphics/freetype_gl_atlas.h"

#include "engine/core/service_locator.h"
#include "engine/assets/asset_manager.h"
#include <glad/gl.h>

// Provide stb_truetype symbols for ui_font_registry.cpp (font validation).
// ImGui's imgui_draw.cpp normally provides these via imstb_truetype.h, but
// thin LTO can strip them when no intra-TU reference exists.
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace Chained
{
	std::shared_ptr<Asset> FontLoader::Create()
	{
		return std::make_shared<FontAsset>();
	}

	bool FontLoader::Load(std::shared_ptr<Asset> asset, const std::string& resolvedPath, std::string* outError)
	{

		auto fail = [&](const std::string& msg, bool logError = true) {
			std::string fullMsg = "FontLoader: " + msg;
			if (logError)
			{
				CH_CORE_ERROR("{0}", fullMsg);
			}
			if (outError)
			{
				*outError = fullMsg;
			}
			return false;
		};

		if (resolvedPath.empty())
		{
			return fail("empty path", false);
		}

		auto* assetManager = ServiceLocator::TryGet<AssetManager>();
		// Try reading from pack first
		auto data = assetManager ? assetManager->ReadAssetData(resolvedPath) : std::vector<uint8_t>{};
		if (data.empty())
		{
			return fail("font not found '" + resolvedPath + "'");
		}

		Font font;
		font.fontSize = 24.0f;
		font.atlasWidth = 1024;
		font.atlasHeight = 1024;

		auto* ftAtlas = new FreeTypeGLAtlas();
		if (!ftAtlas->InitFromMemory(data.data(), data.size(), font.fontSize, font.atlasWidth, font.atlasHeight))
		{
			delete ftAtlas;
			return fail("FreeTypeGLAtlas::InitFromMemory failed for '" + resolvedPath + "'");
		}

		// Build RGBA8 atlasPixels from the alpha-only atlas
		const uint8_t* alphaData = ftAtlas->GetAlphaData();
		std::vector<uint8_t> rgbaPixels(font.atlasWidth * font.atlasHeight * 4);
		if (alphaData)
		{
			for (size_t i = 0; i < static_cast<size_t>(font.atlasWidth) * font.atlasHeight; ++i)
			{
				rgbaPixels[i * 4 + 0] = 255;
				rgbaPixels[i * 4 + 1] = 255;
				rgbaPixels[i * 4 + 2] = 255;
				rgbaPixels[i * 4 + 3] = alphaData[i];
			}
		}

		font.textureAtlas = Texture::Create(font.atlasWidth, font.atlasHeight, TextureFormat::RGBA8);
		font.textureAtlas->SetData(rgbaPixels.data(), rgbaPixels.size());
		font.atlasPixels = std::move(rgbaPixels);

		// Populate chars map from the atlas wrapper's cached glyphs
		// (PreloadRange already populated them during InitFromMemory)
		const auto& cachedGlyphs = ftAtlas->GetCachedGlyphs();
		for (const auto& [codepoint, glyph] : cachedGlyphs)
		{
			FontChar symbol;
			symbol.x0 = glyph.s0;
			symbol.y0 = glyph.t0;
			symbol.x1 = glyph.s1;
			symbol.y1 = glyph.t1;
			symbol.xoff = static_cast<float>(glyph.offsetX);
			symbol.yoff = static_cast<float>(glyph.offsetY);
			symbol.xadvance = glyph.advanceX;
			font.chars[codepoint] = symbol;
		}

		font.freeTypeAtlas = ftAtlas;

		std::static_pointer_cast<FontAsset>(asset)->SetFont(font);

		CH_CORE_INFO("FontLoader: Imported font atlas for {} (glyphs={})", resolvedPath, font.chars.size());

		return true;
	}
} // namespace Chained
