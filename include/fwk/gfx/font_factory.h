#pragma once

#include "fwk/gfx/font.h"

namespace fwk {

class FontFactory {
  public:
	FontFactory();
	~FontFactory();

	FontFactory(FontFactory const &) = delete;
	void operator=(FontFactory const &) = delete;

	static string32 ansiCharset();
	static string32 basicMathCharset();

	Font makeFont(ZStr path, int size_px, bool lcd_mode = false) {
		return makeFont(path, ansiCharset(), size_px, lcd_mode);
	}

	Font makeFont(ZStr path, const string32 &charset, int size_px, bool lcd_mode = false);

  private:
	void *getFace(ZStr);
	using GlyphPair = Pair<FontCore::Glyph, Texture>;

	Texture makeTextureAtlas(vector<GlyphPair> &, int2 atlas_size);
	Texture makeTextureAtlas(vector<GlyphPair> &);

	void *m_library;
	FwdMember<HashMap<string, void *>> m_faces;
};
}
