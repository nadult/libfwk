// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/dtexture.h"
#include "fwk/gfx/font.h"
#include "fwk/gfx/renderer2d.h"
#include "fwk_opengl.h"
#include "fwk/sys/xml.h"
#include <map>

namespace fwk {

struct FontCore::Impl {
	Impl(XMLNode font_node) {
		ASSERT(font_node);

		XMLNode info_node = font_node.child("info");
		XMLNode pages_node = font_node.child("pages");
		XMLNode chars_node = font_node.child("chars");
		XMLNode common_node = font_node.child("common");
		ASSERT(info_node && pages_node && chars_node && common_node);

		m_face_name = info_node.attrib("face");
		m_texture_size = int2(common_node.attrib<int>("scaleW"), common_node.attrib<int>("scaleH"));
		m_line_height = common_node.attrib<int>("lineHeight");
		int text_base = common_node.attrib<int>("base");

		int page_count = common_node.attrib<int>("pages");
		ASSERT(page_count == 1);

		XMLNode first_page_node = pages_node.child("page");
		ASSERT(first_page_node);
		ASSERT(first_page_node.attrib<int>("id") == 0);

		m_texture_name = first_page_node.attrib("file");
		int chars_count = chars_node.attrib<int>("count");
		XMLNode char_node = chars_node.child("char");

		while(char_node) {
			Glyph chr;
			int id = char_node.attrib<int>("id");
			chr.character = id;
			chr.tex_pos = short2(char_node.attrib<int>("x"), char_node.attrib<int>("y"));
			chr.size = short2(char_node.attrib<int>("width"), char_node.attrib<int>("height"));
			chr.offset = short2(char_node.attrib<int>("xoffset"), char_node.attrib<int>("yoffset"));
			chr.x_advance = char_node.attrib<int>("xadvance");
			m_glyphs[id] = chr;

			chars_count--;
			char_node = char_node.sibling("char");
		}
		ASSERT(chars_count == 0);
		ASSERT(m_glyphs.find((int)' ') != m_glyphs.end());

		XMLNode kernings_node = font_node.child("kernings");
		if(kernings_node) {
			int kernings_count = kernings_node.attrib<int>("count");

			XMLNode kerning_node = kernings_node.child("kerning");
			while(kerning_node) {
				int first = kerning_node.attrib<int>("first");
				int second = kerning_node.attrib<int>("second");
				int value = kerning_node.attrib<int>("amount");
				m_kernings[make_pair(first, second)] = value;
				kernings_count--;

				kerning_node = kerning_node.sibling("kerning");
			}
			ASSERT(kernings_count == 0);
		}

		computeRect();
	}
	Impl(CSpan<Glyph> glyphs, CSpan<Kerning> kernings, int2 tex_size, int line_height)
		: m_texture_size(tex_size), m_line_height(line_height) {
		for(auto &glyph : glyphs)
			m_glyphs[glyph.character] = glyph;
		for(auto &kerning : kernings)
			m_kernings[make_pair(kerning.left, kerning.right)] = kerning.value;
		m_max_rect = {};

		for(auto &glyph : m_glyphs) {
			IRect rect = IRect(glyph.second.size) + glyph.second.offset;
			m_max_rect = enclose(m_max_rect, rect);
		}

		for(auto &glyph : m_glyphs)
			glyph.second.offset.y -= m_max_rect.y();
		computeRect();
	}

	void computeRect() {
		m_max_rect = {};
		for(auto &it : m_glyphs)
			m_max_rect = enclose(m_max_rect, IRect(it.second.size) + it.second.offset);
	}

	IRect evalExtents(const string32 &text) const {
		IRect rect;
		int2 pos;

		if(text.empty())
			rect = IRect({0, m_max_rect.height()}) + m_max_rect.min();

		for(int n = 0; n < (int)text.size(); n++) {
			if(text[n] == '\n') {
				pos.x = 0;
				pos.y += m_line_height;
				continue;
			}

			auto char_it = m_glyphs.find(text[n]);
			if(char_it == m_glyphs.end()) {
				char_it = m_glyphs.find((int)' ');
				DASSERT(char_it != m_glyphs.end());
			}

			const Glyph &glyph = char_it->second;

			IRect new_rect = m_max_rect + pos;
			int new_width = text[n] == ' ' ? glyph.x_advance : glyph.offset.x + glyph.size.x;
			new_rect = IRect({new_width, new_rect.height()}) + new_rect.min();

			rect = n == 0 ? new_rect : enclose(rect, new_rect);
			if(n + 1 < (int)text.size()) {
				pos.x += glyph.x_advance;
				auto kerning_it = m_kernings.find(make_pair((int)text[n], (int)text[n + 1]));
				if(kerning_it != m_kernings.end())
					pos.x += kerning_it->second;
			}
		}

		return rect;
	}

	// Returns number of quads generated
	// For every quad it generates: 4 vectors in each buffer
	int genQuads(const string32 &text, Span<float2> out_pos, Span<float2> out_uv) const {
		DASSERT(out_pos.size() == out_uv.size());
		DASSERT(out_pos.size() % 4 == 0);

		const float2 tex_scale = vinv(float2(m_texture_size));
		int count = min((int)text.size(), out_pos.size() / 4);
		float2 pos(0, 0);

		int offset = 0;
		for(int n = 0; n < count; n++) {
			if(text[n] == '\n') {
				pos.x = 0;
				pos.y += m_line_height;
				continue;
			}

			auto char_it = m_glyphs.find(text[n]);
			if(char_it == m_glyphs.end()) {
				char_it = m_glyphs.find((int)' ');
				DASSERT(char_it != m_glyphs.end());
			}

			const Glyph &glyph = char_it->second;

			float2 spos = pos + (float2)glyph.offset;
			out_pos[offset + 0] = spos + float2(0.0f, 0.0f);
			out_pos[offset + 1] = spos + float2(glyph.size.x, 0.0f);
			out_pos[offset + 2] = spos + float2(glyph.size.x, glyph.size.y);
			out_pos[offset + 3] = spos + float2(0.0f, glyph.size.y);

			float2 tpos = (float2)glyph.tex_pos;
			out_uv[offset + 0] = tpos + float2(0.0f, 0.0f);
			out_uv[offset + 1] = tpos + float2(glyph.size.x, 0.0f);
			out_uv[offset + 2] = tpos + float2(glyph.size.x, glyph.size.y);
			out_uv[offset + 3] = tpos + float2(0.0f, glyph.size.y);
			for(int i = 0; i < 4; i++) {
				out_uv[offset + i].x *= tex_scale.x;
				out_uv[offset + i].y *= tex_scale.y;
			}

			if(n + 1 < count) {
				pos.x += glyph.x_advance;
				auto kerning_it = m_kernings.find(make_pair((int)text[n], (int)text[n + 1]));
				if(kerning_it != m_kernings.end())
					pos.x += kerning_it->second;
			}

			offset += 4;
		}

		return offset / 4;
	}

	// TODO: better representation? hash table maybe?
	std::map<int, Glyph> m_glyphs;
	std::map<pair<int, int>, int> m_kernings;
	string m_texture_name;
	int2 m_texture_size;

	string m_face_name;
	IRect m_max_rect;
	int m_line_height;
};

FontStyle::FontStyle(FColor color, FColor shadow_color, HAlign halign, VAlign valign)
	: text_color(color), shadow_color(shadow_color), halign(halign), valign(valign) {}

FontStyle::FontStyle(FColor color, HAlign halign, VAlign valign)
	: text_color(color), shadow_color(ColorId::transparent), halign(halign), valign(valign) {}

FontCore::FontCore(const string &name, Stream &stream) : FontCore(XMLDocument(stream)) {}
FontCore::FontCore(const XMLDocument &doc) : FontCore(doc.child("font")) {}
FontCore::FontCore(const XMLNode &font_node) : m_impl(font_node) {}
FontCore::FontCore(CSpan<Glyph> glyphs, CSpan<Kerning> kernings, int2 tex_size, int line_height)
	: m_impl(move(glyphs), move(kernings), tex_size, line_height) {}
FWK_COPYABLE_CLASS_IMPL(FontCore)

IRect FontCore::evalExtents(const string32 &text) const { return m_impl->evalExtents(text); }
int FontCore::lineHeight() const { return m_impl->m_line_height; }
const string &FontCore::textureName() const { return m_impl->m_texture_name; }
int FontCore::genQuads(const string32 &text, Span<float2> out_pos, Span<float2> out_uv) const {
	return m_impl->genQuads(text, out_pos, out_uv);
}

Font::Font(PFontCore core, PTexture texture) : m_core(core), m_texture(texture) {
	DASSERT(m_core);
	DASSERT(m_texture);
	DASSERT(m_texture->size() == m_core->m_impl->m_texture_size);
}
FWK_COPYABLE_CLASS_IMPL(Font)

FRect Font::draw(Renderer2D &out, const FRect &rect, const FontStyle &style,
				 const string32 &text) const {
	float2 pos = rect.min();
	if(style.halign != HAlign::left || style.valign != VAlign::top) {
		FRect extents = (FRect)m_core->evalExtents(text);
		float2 center = rect.center() - extents.center();

		bool hleft = style.halign == HAlign::left, hcenter = style.halign == HAlign::center;
		bool vtop = style.valign == VAlign::top, vcenter = style.valign == VAlign::center;

		pos.x = hleft ? rect.x() : hcenter ? center.x : rect.ex() - extents.ex();
		pos.y = vtop ? rect.y() : vcenter ? center.y : rect.ey() - extents.ey();
	}

	pos = float2((int)(pos.x + 0.5f), (int)(pos.y + 0.5f));

	vector<float2> pos_buf(text.length() * 4);
	vector<float2> uv_buf(text.length() * 4);
	int quad_count = m_core->genQuads(text, pos_buf, uv_buf);
	pos_buf.resize(quad_count * 4);
	uv_buf.resize(quad_count * 4);

	FRect out_rect = enclose(CSpan<float2>(pos_buf.data(), pos_buf.data() + quad_count * 4));

	out_rect += pos;
	// TODO: increase out_rect when rendering with shadow?

	out.pushViewMatrix();
	out.mulViewMatrix(translation(float3(pos + float2(1.0f, 1.0f), 0.0f)));

	if(style.shadow_color != ColorId::transparent)
		out.addQuads(pos_buf, uv_buf, {}, {m_texture, style.shadow_color});

	out.mulViewMatrix(translation(float3(-1.0f, -1.0f, 0.0f)));
	out.addQuads(pos_buf, uv_buf, {}, {m_texture, style.text_color});
	out.popViewMatrix();

	return out_rect;
}
}
