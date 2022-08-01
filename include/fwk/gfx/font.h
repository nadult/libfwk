// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/fwd_member.h"
#include "fwk/gfx/color.h"
#include "fwk/gfx/image.h"
#include "fwk/gfx_base.h"
#include "fwk/math/box.h"
#include "fwk/str.h"

#include "fwk/vulkan/vulkan_storage.h" // TODO: only Ptr<> is needed here
#include "fwk/vulkan_base.h"

namespace fwk {

struct FontStyle {
	FontStyle(IColor color, IColor shadow_color, HAlign halign = HAlign::left,
			  VAlign valign = VAlign::top);
	FontStyle(IColor color, HAlign halign = HAlign::left, VAlign valign = VAlign::top);

	IColor text_color;
	IColor shadow_color;
	HAlign halign;
	VAlign valign;
};

class FontCore {
  public:
	struct Glyph {
		int character;
		short2 tex_pos;
		short2 size;
		short2 offset;
		short x_advance;
	};

	struct Kerning {
		int left, right;
		int value;
	};

	FontCore(CSpan<Glyph>, CSpan<Kerning>, int2 tex_size, int line_height);
	FWK_COPYABLE_CLASS(FontCore)

	static Ex<FontCore> load(ZStr file_name);
	static Ex<FontCore> load(const XmlDocument &);
	static Ex<FontCore> load(CXmlNode);

	int genQuads(const string32 &text, Span<float2> out_pos, Span<float2> out_uv,
				 float2 offset) const;
	IRect evalExtents(const string32 &) const;
	int lineHeight() const { return m_line_height; }
	const string &textureName() const { return m_texture_name; }

	const auto &glyphs() const { return *&m_glyphs; }
	const auto &kernings() const { return *&m_kernings; }
	i64 usedMemory() const;

  private:
	FontCore();
	void computeRect();

	FwdMember<HashMap<int, Glyph>> m_glyphs;
	FwdMember<HashMap<Pair<int>, int>> m_kernings;
	string m_texture_name;
	int2 m_texture_size;

	string m_face_name;
	IRect m_max_rect;
	int m_line_height;

	friend class FontFactory;
	friend class Font;
};

struct FontData {
	FontCore core;
	Image image;
};

class Font {
  public:
	using Style = FontStyle;
	Font(FontCore, PVImageView);
	FWK_COPYABLE_CLASS(Font)

	static Ex<Font> makeDefault(VDeviceRef, VWindowRef, int font_size = 14);
	FRect draw(Canvas2D &, const FRect &, const Style &, const string32 &text) const;

	template <class Output>
	FRect draw(Output &out, const float2 &pos, const Style &style, const string32 &text) const {
		return draw(out, FRect(pos, pos), style, text);
	}

	template <class Output>
	FRect draw(Output &out, const FRect &rect, const Style &style, Str text_utf8) const {
		if(auto text = toUTF32(text_utf8))
			return draw(out, rect, style, *text);
		return {};
	}

	template <class Output>
	FRect draw(Output &out, const float2 &pos, const Style &style, Str text_utf8) const {
		if(auto text = toUTF32(text_utf8))
			return draw(out, FRect(pos, pos), style, *text);
		return {};
	}

	auto core() const { return m_core; }
	auto texture() const { return m_texture; }

	IRect evalExtents(const string32 &text) const { return m_core.evalExtents(text); }
	IRect evalExtents(Str text_utf8) const {
		if(auto text = toUTF32(text_utf8))
			return m_core.evalExtents(*text);
		return {};
	}
	int lineHeight() const { return m_core.lineHeight(); }

  private:
	float2 drawPos(const string32 &text, const FRect &, const FontStyle &) const;

	FontCore m_core;
	PVImageView m_texture;
};
}
