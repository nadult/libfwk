// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/triangle_buffer.h"
#include "fwk/hash_map.h"

#include "fwk/gfx/font.h"

#include "fwk/gfx/gl_texture.h"
#include "fwk/gfx/opengl.h"
#include "fwk/gfx/renderer2d.h"
#include "fwk/sys/expected.h"
#include "fwk/sys/xml.h"

namespace fwk {

Ex<FontCore> FontCore::load(ZStr file_name) {
	auto doc = XmlDocument::load(file_name);
	return doc ? load(*doc) : doc.error();
}
Ex<FontCore> FontCore::load(const XmlDocument &doc) { return FontCore::load(doc.child("font")); }

FontCore::FontCore() = default;

Ex<FontCore> FontCore::load(CXmlNode font_node) {
	EXPECT(font_node);

	FontCore out;
	auto info_node = font_node.child("info");
	auto pages_node = font_node.child("pages");
	auto chars_node = font_node.child("chars");
	auto common_node = font_node.child("common");
	EXPECT(info_node && pages_node && chars_node && common_node);

	// TODO: co jak parsowanie się sfailuje? kazde parsowanie to potencjalny blad...
	out.m_face_name = info_node.attrib("face");
	out.m_texture_size = int2(common_node.attrib<int>("scaleW"), common_node.attrib<int>("scaleH"));
	out.m_line_height = common_node.attrib<int>("lineHeight");
	int text_base = common_node.attrib<int>("base");

	int page_count = common_node.attrib<int>("pages");
	EXPECT(page_count == 1);

	auto first_page_node = pages_node.child("page");
	EXPECT(first_page_node);
	EXPECT(first_page_node.attrib<int>("id") == 0);

	out.m_texture_name = first_page_node.attrib("file");
	int chars_count = chars_node.attrib<int>("count");
	auto char_node = chars_node.child("char");

	while(char_node) {
		Glyph chr;
		int id = char_node.attrib<int>("id");
		chr.character = id;
		chr.tex_pos = short2(char_node.attrib<int>("x"), char_node.attrib<int>("y"));
		chr.size = short2(char_node.attrib<int>("width"), char_node.attrib<int>("height"));
		chr.offset = short2(char_node.attrib<int>("xoffset"), char_node.attrib<int>("yoffset"));
		chr.x_advance = char_node.attrib<int>("xadvance");
		out.m_glyphs[id] = chr;

		EXPECT_CATCH();

		chars_count--;
		char_node = char_node.sibling("char");
	}
	EXPECT(chars_count == 0);
	EXPECT(out.m_glyphs.find((int)' ') != out.m_glyphs.end());

	auto kernings_node = font_node.child("kernings");
	if(kernings_node) {
		int kernings_count = kernings_node.attrib<int>("count");

		auto kerning_node = kernings_node.child("kerning");
		while(kerning_node) {
			int first = kerning_node.attrib<int>("first");
			int second = kerning_node.attrib<int>("second");
			int value = kerning_node.attrib<int>("amount");
			out.m_kernings[pair(first, second)] = value;
			kernings_count--;

			kerning_node = kerning_node.sibling("kerning");
		}
		EXPECT(kernings_count == 0);
	}

	out.computeRect();
	return out;
}

// clang-format off
	FontCore::FontCore(CSpan<Glyph> glyphs, CSpan<Kerning> kernings, int2 tex_size, int line_height)
		: m_texture_size(tex_size), m_line_height(line_height) {
		for(auto &glyph : glyphs)
			m_glyphs[glyph.character] = glyph;
		for(auto &kerning : kernings)
			m_kernings[pair(kerning.left, kerning.right)] = kerning.value;
		m_max_rect = {};

		for(auto &glyph : m_glyphs) {
			IRect rect = IRect(glyph.second.size) + glyph.second.offset;
			m_max_rect = enclose(m_max_rect, rect);
		}

		for(auto &glyph : m_glyphs)
			glyph.second.offset.y -= m_max_rect.y();
		computeRect();
	}

	FWK_COPYABLE_CLASS_IMPL(FontCore)

	void FontCore::computeRect() {
		m_max_rect = {};
		for(auto &it : m_glyphs)
			m_max_rect = enclose(m_max_rect, IRect(it.second.size) + it.second.offset);
	}

	IRect FontCore::evalExtents(const string32 &text) const {
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
				auto kerning_it = m_kernings.find({(int)text[n], (int)text[n + 1]});
				if(kerning_it != m_kernings.end())
					pos.x += kerning_it->second;
			}
		}

		return rect;
	}

	// Returns number of quads generated
	// For every quad it generates: 4 vectors in each buffer
	int FontCore::genQuads(const string32 &text, Span<float2> out_pos, Span<float2> out_uv, float2 pos) const {
		DASSERT(out_pos.size() == out_uv.size());
		DASSERT(out_pos.size() % 4 == 0);

		const float2 tex_scale = vinv(float2(m_texture_size));
		int count = min((int)text.size(), out_pos.size() / 4);
		float min_x = pos.x;

		int offset = 0;
		for(int n = 0; n < count; n++) {
			if(text[n] == '\n') {
				pos.x = min_x;
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
				auto kerning_it = m_kernings.find({(int)text[n], (int)text[n + 1]});
				if(kerning_it != m_kernings.end())
					pos.x += kerning_it->second;
			}

			offset += 4;
		}

		return offset / 4;
	}
// clang-format on

FontStyle::FontStyle(FColor color, FColor shadow_color, HAlign halign, VAlign valign)
	: text_color(color), shadow_color(shadow_color), halign(halign), valign(valign) {}

FontStyle::FontStyle(FColor color, HAlign halign, VAlign valign)
	: text_color(color), shadow_color(ColorId::transparent), halign(halign), valign(valign) {}

Font::Font(PFontCore core, PTexture texture) : m_core(core), m_texture(texture) {
	DASSERT(m_core);
	DASSERT(m_texture);
	DASSERT(m_texture->size() == m_core->m_texture_size);
}
FWK_COPYABLE_CLASS_IMPL(Font)

float2 Font::drawPos(const string32 &text, const FRect &rect, const FontStyle &style) const {
	float2 pos = rect.min();
	if(style.halign != HAlign::left || style.valign != VAlign::top) {
		FRect extents = (FRect)m_core->evalExtents(text);
		float2 center = rect.center() - extents.center();

		bool hleft = style.halign == HAlign::left, hcenter = style.halign == HAlign::center;
		bool vtop = style.valign == VAlign::top, vcenter = style.valign == VAlign::center;

		pos.x = hleft ? rect.x() : hcenter ? center.x : rect.ex() - extents.ex();
		pos.y = vtop ? rect.y() : vcenter ? center.y : rect.ey() - extents.ey();
	}

	return float2((int)(pos.x + 0.5f), (int)(pos.y + 0.5f));
}

FRect Font::draw(TriangleBuffer &out, const FRect &rect, const FontStyle &style,
				 const string32 &text) const {
	auto pos = drawPos(text, rect, style);

	PodVector<float2> pos_buf(text.length() * 4), uv_buf(text.length() * 4);
	int num_verts = m_core->genQuads(text, pos_buf, uv_buf, pos) * 4;
	CSpan<float2> positions(pos_buf.data(), num_verts), uvs(uv_buf.data(), num_verts);

	if(style.shadow_color != ColorId::transparent) {
		out.setTrans(translation(float3(1.0f, 1.0f, 0.0f)));
		out.setMaterial(Material({m_texture}, (IColor)style.shadow_color));
		out.quads(positions, uvs, {});
	}

	out.setTrans(Matrix4::identity());
	out.setMaterial(Material({m_texture}, (IColor)style.text_color));
	out.quads(positions, uvs, {});

	out.setMaterial(Material(ColorId::white));

	return enclose(positions);
}

FRect Font::draw(Renderer2D &out, const FRect &rect, const FontStyle &style,
				 const string32 &text) const {
	auto pos = drawPos(text, rect, style);

	PodVector<float2> pos_buf(text.length() * 4), uv_buf(text.length() * 4);
	int num_verts = m_core->genQuads(text, pos_buf, uv_buf, pos) * 4;
	CSpan<float2> positions(pos_buf.data(), num_verts), uvs(uv_buf.data(), num_verts);

	if(style.shadow_color != ColorId::transparent) {
		out.pushViewMatrix();
		out.mulViewMatrix(translation(float3(1.0f, 1.0f, 0.0f)));
		// TODO: increase out_rect when rendering with shadow?
		// TODO: shadows could use same set of vertex data
		out.addQuads(positions, uvs, {}, {m_texture, style.shadow_color});
		out.popViewMatrix();
	}
	out.addQuads(positions, uvs, {}, {m_texture, style.text_color});

	return enclose(positions);
}
}
