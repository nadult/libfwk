/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_gfx.h"

namespace fwk {

FontCore::FontCore() {
	m_texture_size = int2(2, 2);
	m_max_rect = IRect(int2(8, 8));
	m_line_height = 8;
	m_glyphs[' '] = FontCore::Glyph{' ', short2(0, 0), short2(2, 2), short2(0, 0), 2};
}
}

#ifdef FWK_TARGET_HTML5

namespace fwk {

class FontFactory::Impl {};
FontFactory::FontFactory() = default;
FontFactory::~FontFactory() = default;

Font FontFactory::makeFont(const string &path, int size, bool lcd_mode) {
	// TODO: add support for freetype / somehow use html fonts
	FontCore core;
	return Font(PFontCore(move(core)), make_immutable<DTexture>(Texture(2, 2)));
}
}

#else

#include <ft2build.h>
#ifdef FT_FREETYPE_H
#include FT_FREETYPE_H
#endif

#include <cstring>
#include <cwchar>
#include <cstdarg>
#include <algorithm>

namespace fwk {

class FontFactory::Impl {
  public:
	FT_Library library;
	std::map<string, FT_Face> faces;

	FT_Face getFace(const string &path) {
		auto it = faces.find(path);
		if(it != faces.end())
			return it->second;

		FT_Face face;
		FT_Error error = FT_New_Face(library, path.c_str(), 0, &face);

		if(error == FT_Err_Unknown_File_Format)
			THROW("Error while loading font face '%s': unknown file format", path.c_str());
		else if(error != 0)
			THROW("Error while loading font face '%s'", path.c_str());
		faces[path] = face;
		return face;
	}
};

FontFactory::FontFactory() : m_impl(make_unique<Impl>()) {
	if(FT_Init_FreeType(&m_impl->library) != 0)
		THROW("Error while initializing FreeType");
}

FontFactory::~FontFactory() {
	for(auto face : m_impl->faces)
		FT_Done_Face(face.second);
	FT_Done_FreeType(m_impl->library);
}

static Texture makeTextureAtlas(vector<pair<FontCore::Glyph, Texture>> &glyphs, int2 atlas_size) {
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

	Texture atlas(atlas_size.x, atlas_size.y);
	atlas.fill(Color::transparent);

	for(int g = 0; g < (int)glyphs.size(); g++) {
		auto const &glyph = glyphs[g].first;
		auto &tex = glyphs[g].second;
		atlas.blit(tex, glyph.tex_pos);
	}

	return atlas;
}

using GlyphPair = pair<FontCore::Glyph, Texture>;

Texture makeTextureAtlas(vector<GlyphPair> &glyphs) {
	std::sort(begin(glyphs), end(glyphs), [](const GlyphPair &a, const GlyphPair &b) {
		return a.first.size.y < b.first.size.y;
	});
	return makeTextureAtlas(glyphs, {256, 256});
}

Font FontFactory::makeFont(const string &path, int size, bool lcd_mode) {
	FT_Face face = m_impl->getFace(path);
	if(FT_Set_Pixel_Sizes(face, 0, size) != 0)
		THROW("Error while creating font %s: failed on FT_Set_Pixel_Sizes", path.c_str());

	wstring ansi_charset;
	{
		vector<char> chars;
		for(char c = 32; c < 127; c++)
			chars.emplace_back(c);
		ansi_charset = toWideString(string(begin(chars), end(chars)));
	}

	vector<pair<FontCore::Glyph, Texture>> glyphs;
	for(wchar_t character : ansi_charset) {
		FT_UInt index = FT_Get_Char_Index(face, character);
		if(FT_Load_Glyph(face, index, FT_LOAD_DEFAULT) != 0)
			continue;

		auto *glyph = face->glyph;
		if(FT_Render_Glyph(glyph, lcd_mode ? FT_RENDER_MODE_LCD : FT_RENDER_MODE_NORMAL) != 0)
			continue;

		auto const &bitmap = glyph->bitmap;

		Texture tex(lcd_mode ? bitmap.width / 3 : bitmap.width, bitmap.rows);

		for(int y = 0; y < tex.height(); y++) {
			Color *dst = tex.line(y);
			unsigned char *src = bitmap.buffer + bitmap.pitch * y;
			if(lcd_mode) {
				for(int x = 0; x < tex.width(); x++) {
					Color src_color(src[x * 3 + 0], src[x * 3 + 1], src[x * 3 + 2]);
					dst[x] = src_color;
				}

			} else {
				for(int x = 0; x < tex.width(); x++)
					dst[x] = Color(255, 255, 255, int(src[x]));
			}
		}

		short2 bearing(glyph->metrics.horiBearingX / 64, -glyph->metrics.horiBearingY / 64);
		short advance = glyph->metrics.horiAdvance / 64;

		glyphs.emplace_back(FontCore::Glyph{character, {0, 0}, tex.size(), bearing, advance},
							move(tex));
	}

	FontCore out;
	auto atlas = makeTextureAtlas(glyphs);
	out.m_texture_size = atlas.size();

	for(auto &glyph : glyphs)
		out.m_glyphs[glyph.first.character] = glyph.first;

	out.m_max_rect = IRect(0, 0, 0, 0);

	for(auto &glyph : glyphs) {
		IRect rect = IRect((int2)glyph.first.size) + int2(glyph.first.offset);
		out.m_max_rect = sum(out.m_max_rect, rect);
	}
	out.m_line_height = face->size->metrics.height / 64;

	for(auto &glyph : out.m_glyphs)
		glyph.second.offset.y -= out.m_max_rect.min.y;

	// TODO: optimize
	if(FT_HAS_KERNING(face))
		for(auto left : ansi_charset)
			for(auto right : ansi_charset) {
				FT_Vector vector;
				FT_Get_Kerning(face, left, right, FT_KERNING_DEFAULT, &vector);

				int2 kerning(vector.x / 64, vector.y / 64);
				if(kerning.x != 0)
					out.m_kernings[make_pair(int(left), int(right))] = kerning.x;
			}

	return {PFontCore(move(out)), make_immutable<DTexture>(atlas)};
}
}

#endif
