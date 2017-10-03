// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/color.h"
#include "fwk/gfx_base.h"

namespace fwk {

struct FontStyle {
	FontStyle(FColor color, FColor shadow_color, HAlign halign = HAlign::left,
			  VAlign valign = VAlign::top);
	FontStyle(FColor color, HAlign halign = HAlign::left, VAlign valign = VAlign::top);

	FColor text_color;
	FColor shadow_color;
	HAlign halign;
	VAlign valign;
};

class FontCore : public immutable_base<FontCore> {
  public:
	FontCore(const string &name, Stream &);
	FontCore(const XMLDocument &);
	FontCore(const XMLNode &);
	FWK_COPYABLE_CLASS(FontCore)

	struct Glyph {
		int character;
		short2 tex_pos;
		short2 size;
		short2 offset;
		short x_advance;
	};

	int genQuads(const string32 &text, Span<float2> out_pos, Span<float2> out_uv) const;
	IRect evalExtents(const string32 &) const;
	int lineHeight() const;
	const string &textureName() const;

  private:
	struct Kerning {
		int left, right;
		int value;
	};
	FontCore(CSpan<Glyph>, CSpan<Kerning>, int2, int);

	struct Impl;
	StaticPimpl<Impl, 192> m_impl;

	friend class FontFactory;
	friend class Font;
};

class Font {
  public:
	using Style = FontStyle;
	Font(PFontCore font, PTexture texture);
	FWK_COPYABLE_CLASS(Font)

	FRect draw(Renderer2D &out, const FRect &rect, const Style &style, const string32 &text) const;
	FRect draw(Renderer2D &out, const float2 &pos, const Style &style, const string32 &text) const {
		return draw(out, FRect(pos, pos), style, text);
	}

	FRect draw(Renderer2D &out, const FRect &rect, const Style &style, StringRef text_utf8) const {
		if(auto text = toUTF32(text_utf8))
			return draw(out, rect, style, *text);
		return {};
	}
	FRect draw(Renderer2D &out, const float2 &pos, const Style &style, StringRef text_utf8) const {
		if(auto text = toUTF32(text_utf8))
			return draw(out, FRect(pos, pos), style, *text);
		return {};
	}

	auto core() const { return m_core; }
	auto texture() const { return m_texture; }

	IRect evalExtents(const string32 &text) const { return m_core->evalExtents(text); }
	IRect evalExtents(StringRef text_utf8) const {
		if(auto text = toUTF32(text_utf8))
			return m_core->evalExtents(*text);
		return {};
	}
	int lineHeight() const { return m_core->lineHeight(); }

  private:
	PFontCore m_core;
	PTexture m_texture;
};

class FontFactory {
  public:
	FontFactory();
	~FontFactory();

	FontFactory(FontFactory const &) = delete;
	void operator=(FontFactory const &) = delete;

	Font makeFont(string const &path, int size_in_pixels, bool lcd_mode = false);

  private:
	class Impl;
	unique_ptr<Impl> m_impl;
};
}
