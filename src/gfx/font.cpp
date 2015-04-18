/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_gfx.h"
#include "fwk_opengl.h"
#include "fwk_xml.h"
#include <cstring>
#include <cwchar>
#include <cstdarg>

namespace fwk {

int convertToWChar(const char *str, wchar_t *wstr, int buffer_size) {
	mbstate_t ps;
	memset(&ps, 0, sizeof(ps));

	size_t len = mbsrtowcs(wstr, &str, buffer_size, &ps);
	return (int)(len == (size_t)-1 ? 0 : len);
}

FontStyle::FontStyle(Color color, Color shadow_color, HAlign halign, VAlign valign)
	: text_color(color), shadow_color(shadow_color), halign(halign), valign(valign) {}

FontStyle::FontStyle(Color color, HAlign halign, VAlign valign)
	: text_color(color), shadow_color(Color::transparent), halign(halign), valign(valign) {}

Font::Font() {}

void Font::load(Stream &sr) {
	ASSERT(sr.isLoading());

	XMLDocument doc;
	sr >> doc;
	loadFromXML(doc);
}

void Font::loadFromXML(const XMLDocument &doc) {
	m_glyphs.clear();
	m_kernings.clear();
	m_face_name.clear();

	XMLNode font_node = doc.child("font");
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

IRect Font::evalExtents(const char *str, bool exact) const {
	int len = strlen(str);
	PodArray<wchar_t> wstr(len);
	len = convertToWChar(str, wstr.data(), len);

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

int Font::genQuads(const char *str, float3 *out_pos, float2 *out_uv, int buffer_size) const {
	DASSERT(buffer_size >= 0 && str);

	wchar_t wstr[1024];
	int len = convertToWChar(str, wstr, sizeof(wstr) / sizeof(wchar_t));

	const float2 tex_scale(1.0f / float(m_texture_size.x), 1.0f / float(m_texture_size.y));
	int count = min(len, buffer_size);
	float2 pos(0, 0);

	int gen_count = 0;
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
		out_pos[0] = float3(spos + float2(0.0f, 0.0f), 0.0f);
		out_pos[1] = float3(spos + float2(glyph.size.x, 0.0f), 0.0f);
		out_pos[2] = float3(spos + float2(glyph.size.x, glyph.size.y), 0.0f);
		out_pos[3] = float3(spos + float2(0.0f, glyph.size.y), 0.0f);

		float2 tpos = (float2)glyph.tex_pos;
		out_uv[0] = tpos + float2(0.0f, 0.0f);
		out_uv[1] = tpos + float2(glyph.size.x, 0.0f);
		out_uv[2] = tpos + float2(glyph.size.x, glyph.size.y);
		out_uv[3] = tpos + float2(0.0f, glyph.size.y);
		for(int i = 0; i < 4; i++) {
			out_uv[i].x *= tex_scale.x;
			out_uv[i].y *= tex_scale.y;
		}

		if(n + 1 < count) {
			pos.x += glyph.x_advance;
			auto kerning_it = m_kernings.find(make_pair((int)wstr[n], (int)wstr[n + 1]));
			if(kerning_it != m_kernings.end())
				pos.x += kerning_it->second;
		}

		out_pos += 4;
		out_uv += 4;
		gen_count++;
	}

	return gen_count;
}

FontRenderer::FontRenderer(PFont font, PTexture texture, Renderer &out)
	: m_font(font), m_texture(texture), m_renderer(out) {
	DASSERT(m_font);
	DASSERT(m_texture);
	DASSERT(m_texture->size() == m_font->m_texture_size);
}

FRect FontRenderer::draw(const FRect &rect, const FontStyle &style, const char *text) const {
	float2 pos = rect.min;
	if(style.halign != HAlign::left || style.valign != VAlign::top) {
		FRect extents = (FRect)m_font->evalExtents(text);
		float2 center = rect.center() - extents.center();
		// TODO: Better veritcal alignment

		pos.x = style.halign == HAlign::left ? rect.min.x : style.halign == HAlign::center
																? center.x
																: rect.max.x - extents.max.x;
		pos.y = style.valign == VAlign::top ? rect.min.y : style.valign == VAlign::center
															   ? center.y
															   : rect.max.y - extents.max.y;
	}

	pos = float2((int)(pos.x + 0.5f), (int)(pos.y + 0.5f));

	int len = strlen(text);
	PodArray<float3> pos_buf(len * 4);
	PodArray<float2> uv_buf(len * 4);
	PodArray<Color> colors(len * 4);
	int quad_count = m_font->genQuads(text, pos_buf.data(), uv_buf.data(), len * 4);

	FRect out_rect;
	for(int n = 0; n < quad_count * 4; n++) {
		out_rect.min = min(out_rect.min, pos_buf[n].xy());
		out_rect.max = max(out_rect.max, pos_buf[n].xy());
	}
	out_rect += pos;
	// TODO: increase out_rect when rendering with shadow?
			
	m_renderer.pushViewMatrix();
	m_renderer.mulViewMatrix(translation(float3(pos + float2(1.0f, 1.0f), 0.0f)));

	if(style.shadow_color != Color::transparent) {	
		for(int n = 0; n < quad_count * 4; n++)
			colors[n] = style.shadow_color;
		m_renderer.addQuads(pos_buf.data(), uv_buf.data(), colors.data(), quad_count, m_texture);
	}
	
	m_renderer.mulViewMatrix(translation(float3(-1.0f, -1.0f, 0.0f)));
	for(int n = 0; n < quad_count * 4; n++)
		colors[n] = style.text_color;
	m_renderer.addQuads(pos_buf.data(), uv_buf.data(), colors.data(), quad_count, m_texture);
	m_renderer.popViewMatrix();
	
	return out_rect;
}
}
