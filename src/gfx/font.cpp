/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_gfx.h"
#include "fwk_opengl.h"
#include "fwk_xml.h"
#include <cstring>
#include <cwchar>
#include <cstdarg>

namespace fwk {

int convertToWChar(StringRef text, PodArray<wchar_t> &out) {
	mbstate_t ps;
	memset(&ps, 0, sizeof(ps));

	const char *str = text.c_str();
	size_t len = mbsrtowcs(out.data(), &str, out.size(), &ps);
	return (int)(len == (size_t)-1 ? 0 : len);
}

namespace {
	XMLDocument loadDoc(Stream &sr) {
		DASSERT(sr.isLoading());
		XMLDocument doc;
		sr >> doc;
		return std::move(doc);
	}
}

FontStyle::FontStyle(Color color, Color shadow_color, HAlign halign, VAlign valign)
	: text_color(color), shadow_color(shadow_color), halign(halign), valign(valign) {}

FontStyle::FontStyle(Color color, HAlign halign, VAlign valign)
	: text_color(color), shadow_color(Color::transparent), halign(halign), valign(valign) {}

FontCore::FontCore(const string &name, Stream &stream) : FontCore(loadDoc(stream)) {}
FontCore::FontCore(const XMLDocument &doc) : FontCore(doc.child("font")) {}
FontCore::FontCore(const XMLNode &font_node) {
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
		chr.tex_pos = int2(char_node.attrib<int>("x"), char_node.attrib<int>("y"));
		chr.size = int2(char_node.attrib<int>("width"), char_node.attrib<int>("height"));
		chr.offset = int2(char_node.attrib<int>("xoffset"), char_node.attrib<int>("yoffset"));
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

	m_max_rect = IRect(0, 0, 0, 0);
	for(auto &it : m_glyphs) {
		IRect rect = IRect(int2(it.second.size)) + it.second.offset;
		m_max_rect = sum(m_max_rect, rect);
	}
}

IRect FontCore::evalExtents(StringRef text, bool exact) const {
	PodArray<wchar_t> wstr(text.size());
	int len = convertToWChar(text, wstr);

	IRect rect(0, 0, 0, 0);
	int2 pos(0, 0);

	if(!len && !exact) {
		rect = m_max_rect;
		rect.setWidth(0);
	}

	for(int n = 0; n < len; n++) {
		if(wstr[n] == '\n') {
			pos.x = 0;
			pos.y += m_line_height;
			continue;
		}

		auto char_it = m_glyphs.find(wstr[n]);
		if(char_it == m_glyphs.end()) {
			char_it = m_glyphs.find((int)' ');
			DASSERT(char_it != m_glyphs.end());
		}

		const Glyph &glyph = char_it->second;

		IRect new_rect;
		if(exact) {
			new_rect = IRect(pos + glyph.offset, pos + glyph.offset + glyph.size);
		} else {
			new_rect = m_max_rect + pos;
			new_rect.setWidth(wstr[n] == ' ' ? glyph.x_advance : glyph.offset.x + glyph.size.x);
		}

		rect = n == 0 ? new_rect : rect + new_rect;
		if(n + 1 < len) {
			pos.x += glyph.x_advance;
			auto kerning_it = m_kernings.find(make_pair((int)wstr[n], (int)wstr[n + 1]));
			if(kerning_it != m_kernings.end())
				pos.x += kerning_it->second;
		}
	}

	return rect;
}

int FontCore::genQuads(StringRef text, Range<float2> out_pos, Range<float2> out_uv) const {
	DASSERT(out_pos.size() == out_uv.size());
	DASSERT(out_pos.size() % 4 == 0);

	PodArray<wchar_t> wstr(text.size());
	int len = convertToWChar(text, wstr);

	const float2 tex_scale = inverse(float2(m_texture_size));
	int count = min(len, out_pos.size() / 4);
	float2 pos(0, 0);

	int offset = 0;
	for(int n = 0; n < count; n++) {
		if(wstr[n] == '\n') {
			pos.x = 0;
			pos.y += m_line_height;
			continue;
		}

		auto char_it = m_glyphs.find(wstr[n]);
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
			auto kerning_it = m_kernings.find(make_pair((int)wstr[n], (int)wstr[n + 1]));
			if(kerning_it != m_kernings.end())
				pos.x += kerning_it->second;
		}

		offset += 4;
	}

	return offset / 4;
}

Font::Font(PFontCore core, PTexture texture) : m_core(core), m_texture(texture) {
	DASSERT(m_core);
	DASSERT(m_texture);
	DASSERT(m_texture->size() == m_core->m_texture_size);
}

FRect Font::draw(Renderer2D &out, const FRect &rect, const FontStyle &style, StringRef text) const {
	float2 pos = rect.min;
	if(style.halign != HAlign::left || style.valign != VAlign::top) {
		FRect extents = (FRect)m_core->evalExtents(text);
		float2 center = rect.center() - extents.center();
		// TODO: Better vertical alignment

		pos.x = style.halign == HAlign::left ? rect.min.x : style.halign == HAlign::center
																? center.x
																: rect.max.x - extents.max.x;
		pos.y = style.valign == VAlign::top ? rect.min.y : style.valign == VAlign::center
															   ? center.y
															   : rect.max.y - extents.max.y;
	}

	pos = float2((int)(pos.x + 0.5f), (int)(pos.y + 0.5f));

	vector<float2> pos_buf(text.length() * 4);
	vector<float2> uv_buf(text.length() * 4);
	int quad_count = m_core->genQuads(text, pos_buf, uv_buf);
	pos_buf.resize(quad_count * 4);
	uv_buf.resize(quad_count * 4);

	FRect out_rect;
	for(int n = 0; n < quad_count * 4; n++) {
		out_rect.min = min(out_rect.min, pos_buf[n]);
		out_rect.max = max(out_rect.max, pos_buf[n]);
	}
	out_rect += pos;
	// TODO: increase out_rect when rendering with shadow?

	out.pushViewMatrix();
	out.mulViewMatrix(translation(float3(pos + float2(1.0f, 1.0f), 0.0f)));

	if(style.shadow_color != Color::transparent)
		out.addQuads(pos_buf, uv_buf, {}, {m_texture, style.shadow_color});

	out.mulViewMatrix(translation(float3(-1.0f, -1.0f, 0.0f)));
	out.addQuads(pos_buf, uv_buf, {}, {m_texture, style.text_color});
	out.popViewMatrix();

	return out_rect;
}
}
