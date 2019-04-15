#pragma once

#include "fwk/gfx/font.h"
#include "fwk/sys/expected.h"
#include "fwk/sys/unique_ptr.h"

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

	Ex<Font> makeFont(ZStr path, int size_px, bool lcd_mode = false) {
		return makeFont(path, ansiCharset(), size_px, lcd_mode);
	}

	Ex<Font> makeFont(ZStr path, const string32 &charset, int size_px, bool lcd_mode = false);

  private:
	using GlyphPair = Pair<FontCore::Glyph, Texture>;

	Texture makeTextureAtlas(vector<GlyphPair> &, int2 atlas_size);
	Texture makeTextureAtlas(vector<GlyphPair> &);

	struct Impl;
	UniquePtr<Impl> m_impl;
};
}
