#pragma once

#include "fwk/gfx/font.h"

namespace fwk {

class FontFactory {
  public:
	FontFactory();
	~FontFactory();

	FontFactory(FontFactory const &) = delete;
	void operator=(FontFactory const &) = delete;

	Font makeFont(const string &path, int size_in_pixels, bool lcd_mode = false);

  private:
	void *getFace(const string &);
	using GlyphPair = pair<FontCore::Glyph, Texture>;

	Texture makeTextureAtlas(vector<GlyphPair> &, int2 atlas_size);
	Texture makeTextureAtlas(vector<GlyphPair> &);

	void *m_library;
	FwdMember<HashMap<string, void *>> m_faces;
};
}
