#pragma once

#include "fwk/dynamic.h"
#include "fwk/gfx/font.h"
#include "fwk/sys/expected.h"

namespace fwk {
class FontFactory {
  public:
	FontFactory();
	FontFactory(FontFactory &&);
	~FontFactory();

	FontFactory(FontFactory const &) = delete;
	void operator=(FontFactory const &) = delete;

	static string32 ansiCharset();
	static string32 basicMathCharset();

	Ex<FontData> makeFont(ZStr path, int size_px, bool lcd_mode = false) {
		return makeFont(path, ansiCharset(), size_px, lcd_mode);
	}

	Ex<FontData> makeFont(ZStr path, const string32 &charset, int size_px, bool lcd_mode = false);

  private:
	using GlyphPair = Pair<FontCore::Glyph, Image>;

	Image makeTextureAtlas(vector<GlyphPair> &, int2 atlas_size);
	Image makeTextureAtlas(vector<GlyphPair> &);

	struct Impl;
	Dynamic<Impl> m_impl;
};
}
