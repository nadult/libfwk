// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/font_factory.h"

#include "fwk/format.h"
#include "fwk/gfx/image.h"
#include "fwk/hash_map.h"
#include "fwk/sys/error.h"
#include <cstdarg>
#include <cstring>
#include <cwchar>

#include <ft2build.h>
#ifdef FT_FREETYPE_H
#include FT_FREETYPE_H
#endif

namespace fwk {

static const char *ftError(int err) {
	if(err == FT_Err_Unknown_File_Format)
		return "unknown file format";
	return "unknown";
}

struct FontFactory::Impl {
	FT_Library lib = nullptr;
	HashMap<string, FT_Face> faces;

	Ex<FT_Face> getFace(ZStr path) {
		DASSERT(lib);

		if(auto it = faces.find(path))
			return it->value;

		FT_Face face;
		FT_Error error = FT_New_Face(lib, path.c_str(), 0, &face);

		if(error)
			return FWK_ERROR("Error while loading font face '%': % [%]", path, ftError(error),
							 error);
		faces[path] = face;
		return face;
	}
};

FontFactory::FontFactory() {
	m_impl.emplace();
	if(FT_Init_FreeType(&m_impl->lib) != 0)
		m_impl->lib = nullptr;
}

FontFactory::FontFactory(FontFactory &&) = default;

FontFactory::~FontFactory() {
	if(m_impl->lib) {
		for(auto face : m_impl->faces)
			FT_Done_Face((FT_Face)face.value);
		FT_Done_FreeType((FT_Library)m_impl->lib);
	}
}

string32 FontFactory::ansiCharset() {
	vector<char> chars;
	for(char c = 32; c < 127; c++)
		chars.emplace_back(c);
	auto text = toUTF32(string(begin(chars), end(chars)));
	ASSERT(text);
	return *text;
}

string32 FontFactory::basicMathCharset() {
	string utf8_text = "\u2219\u221A\u221e\u2248\u2260\u2264\u2265";
	auto text = toUTF32(utf8_text);
	ASSERT(text);
	return *text;
}

Image FontFactory::makeTextureAtlas(vector<Pair<FontCore::Glyph, Image>> &glyphs, int2 atlas_size) {
	const int border = 2;

	bool all_fits = true;
	int x_offset = 0, y_offset = 0, max_y = 0;

	for(int g = 0; g < (int)glyphs.size(); g++) {
		auto &glyph = glyphs[g].first;

		if(glyph.size.y + border * 2 + y_offset > atlas_size.y) {
			all_fits = false;
			break;
		}

		if(glyph.size.x + border * 2 + x_offset > atlas_size.x) {
			if(x_offset == 0) {
				all_fits = false;
				break;
			}
			y_offset = max_y;
			x_offset = 0;
			g--;
			continue;
		}

		glyph.tex_pos = short2(x_offset + border, y_offset + border);
		max_y = max(max_y, y_offset + border * 2 + glyph.size.y);
		x_offset += border * 2 + glyph.size.x;
	}

	if(!all_fits) {
		int2 new_atlas_size = atlas_size;
		(atlas_size.y > atlas_size.x ? new_atlas_size.x : new_atlas_size.y) *= 2;
		return makeTextureAtlas(glyphs, new_atlas_size);
	}

	Image atlas(atlas_size, ColorId::transparent);

	for(int g = 0; g < (int)glyphs.size(); g++) {
		auto const &glyph = glyphs[g].first;
		auto &tex = glyphs[g].second;
		atlas.blit(tex, glyph.tex_pos);
	}

	return atlas;
}

Image FontFactory::makeTextureAtlas(vector<GlyphPair> &glyphs) {
	std::sort(begin(glyphs), end(glyphs), [](const GlyphPair &a, const GlyphPair &b) {
		return a.first.size.y < b.first.size.y;
	});
	return makeTextureAtlas(glyphs, {256, 256});
}

auto FontFactory::makeFont(ZStr path, const string32 &charset, int size_px, bool lcd_mode)
	-> Ex<FontData> {
	DASSERT(size_px > 0);
	DASSERT(size_px < 1000 && "Please keep it reasonable");

	if(!m_impl->lib)
		return FWK_ERROR("FreeType not initialized properly or FontFactory moved");

	auto face = EX_PASS(m_impl->getFace(path));
	if(FT_Set_Pixel_Sizes(face, 0, size_px) != 0)
		return FWK_ERROR("Error in FT_Set_Pixel_Sizes while creating font %", path);

	vector<Pair<FontCore::Glyph, Image>> glyphs;
	for(auto character : charset) {
		FT_UInt index = FT_Get_Char_Index(face, character);
		if(FT_Load_Glyph(face, index, FT_LOAD_DEFAULT) != 0)
			continue;

		auto *glyph = face->glyph;
		if(FT_Render_Glyph(glyph, lcd_mode ? FT_RENDER_MODE_LCD : FT_RENDER_MODE_NORMAL) != 0)
			continue;

		auto const &bitmap = glyph->bitmap;

		Image tex(int2(lcd_mode ? bitmap.width / 3 : bitmap.width, bitmap.rows), no_init);

		for(int y = 0; y < tex.height(); y++) {
			auto dst = tex.row<IColor>(y);
			unsigned char *src = bitmap.buffer + bitmap.pitch * y;
			if(lcd_mode) {
				for(int x = 0; x < tex.width(); x++) {
					IColor src_color(src[x * 3 + 0], src[x * 3 + 1], src[x * 3 + 2]);
					dst[x] = src_color;
				}

			} else {
				for(int x = 0; x < tex.width(); x++)
					dst[x] = IColor(255, 255, 255, int(src[x]));
			}
		}

		short2 bearing(glyph->metrics.horiBearingX / 64, -glyph->metrics.horiBearingY / 64);
		short advance = glyph->metrics.horiAdvance / 64;

		glyphs.emplace_back(
			FontCore::Glyph{(int)character, {0, 0}, (short2)tex.size(), bearing, advance},
			std::move(tex));
	}

	auto atlas = makeTextureAtlas(glyphs);

	auto oglyphs = transform(glyphs, [](auto &pair) { return pair.first; });

	vector<FontCore::Kerning> okernings;
	// TODO: optimize
	if(FT_HAS_KERNING(face))
		for(auto left : charset)
			for(auto right : charset) {
				FT_Vector vector;
				FT_Get_Kerning(face, left, right, FT_KERNING_DEFAULT, &vector);

				int2 kerning(vector.x / 64, vector.y / 64);
				if(kerning.x != 0)
					okernings.emplace_back(FontCore::Kerning{int(left), int(right), kerning.x});
			}

	int line_height = (int)face->size->metrics.height / 64;
	FontCore core{std::move(oglyphs), std::move(okernings), atlas.size(), line_height};
	return FontData{std::move(core), std::move(atlas)};
}
}
