// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/hash_map.h"

#include "fwk/gfx/font.h"

#include "fwk/gfx/canvas_2d.h"
#include "fwk/gfx/font_factory.h"
#include "fwk/gfx/font_finder.h"
#include "fwk/io/xml.h"
#include "fwk/sys/assert.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan/vulkan_image.h"
#include "fwk/vulkan/vulkan_window.h"

namespace fwk {

FontStyle::FontStyle(IColor color, IColor shadow_color, HAlign halign, VAlign valign)
	: text_color(color), shadow_color(shadow_color), halign(halign), valign(valign) {}

FontStyle::FontStyle(IColor color, HAlign halign, VAlign valign)
	: text_color(color), shadow_color(ColorId::transparent), halign(halign), valign(valign) {}

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
	out.m_face_name = info_node("face");
	out.m_texture_size = int2(common_node("scaleW"), common_node("scaleH"));
	out.m_line_height = common_node("lineHeight");
	int text_base = common_node("base");

	int page_count = common_node("pages");
	EXPECT(page_count == 1);

	auto first_page_node = pages_node.child("page");
	EXPECT(first_page_node);
	EXPECT((int)first_page_node("id") == 0);

	out.m_texture_name = first_page_node("file");
	int chars_count = chars_node("count");
	auto char_node = chars_node.child("char");

	while(char_node) {
		Glyph chr;
		int id = char_node("id");
		chr.character = id;
		chr.tex_pos = short2(char_node("x"), char_node("y"));
		chr.size = short2(char_node("width"), char_node("height"));
		chr.offset = short2(char_node("xoffset"), char_node("yoffset"));
		chr.x_advance = char_node("xadvance");
		out.m_glyphs[id] = chr;

		EX_CATCH();

		chars_count--;
		char_node = char_node.sibling("char");
	}
	EXPECT(chars_count == 0);
	EXPECT(out.m_glyphs.find((int)' ') != out.m_glyphs.end());

	auto kernings_node = font_node.child("kernings");
	if(kernings_node) {
		int kernings_count = kernings_node("count");

		auto kerning_node = kernings_node.child("kerning");
		while(kerning_node) {
			int first = kerning_node("first");
			int second = kerning_node("second");
			int value = kerning_node("amount");
			out.m_kernings[pair(first, second)] = value;
			kernings_count--;

			kerning_node = kerning_node.sibling("kerning");
		}
		EXPECT(kernings_count == 0);
	}

	out.computeRect();
	return out;
}

i64 FontCore::usedMemory() const { return m_glyphs.usedMemory() + m_kernings.usedMemory(); }

// clang-format off
	FontCore::FontCore(CSpan<Glyph> glyphs, CSpan<Kerning> kernings, int2 tex_size, int line_height)
		: m_texture_size(tex_size), m_line_height(line_height) {
		for(auto &glyph : glyphs)
			m_glyphs[glyph.character] = glyph;
		for(auto &kerning : kernings)
			m_kernings[pair(kerning.left, kerning.right)] = kerning.value;
		m_max_rect = {};

		for(auto &[id, glyph] : m_glyphs) {
			IRect rect = IRect(glyph.size) + glyph.offset;
			m_max_rect = enclose(m_max_rect, rect);
		}

		for(auto &glyph : m_glyphs)
			glyph.value.offset.y -= m_max_rect.y();
		computeRect();
	}

	FWK_COPYABLE_CLASS_IMPL(FontCore)

	void FontCore::computeRect() {
		m_max_rect = {};
		for(auto &it : m_glyphs)
			m_max_rect = enclose(m_max_rect, IRect(it.value.size) + it.value.offset);
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

			const Glyph &glyph = char_it->value;

			IRect new_rect = m_max_rect + pos;
			int new_width = text[n] == ' ' ? glyph.x_advance : glyph.offset.x + glyph.size.x;
			new_rect = IRect({new_width, new_rect.height()}) + new_rect.min();

			rect = n == 0 ? new_rect : enclose(rect, new_rect);
			if(n + 1 < (int)text.size()) {
				pos.x += glyph.x_advance;
				if(auto it = m_kernings.find({(int)text[n], (int)text[n + 1]}))
					pos.x += it->value;
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

			const Glyph &glyph = char_it->value;

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
				if(auto it = m_kernings.find({(int)text[n], (int)text[n + 1]}))
					pos.x += it->value;
			}

			offset += 4;
		}

		return offset / 4;
	}
// clang-format on

Font::Font(FontCore core, PVImageView texture) : m_core(std::move(core)), m_texture(texture) {
	DASSERT(m_texture);
	DASSERT_EQ(int3(core.m_texture_size, 1), m_texture->size());
}

FWK_COPYABLE_CLASS_IMPL(Font)

Ex<Font> Font::makeDefault(VDeviceRef device, VWindowRef window, int font_size) {
	auto font_path = EX_PASS(findDefaultSystemFont()).file_path;
	font_size *= window->dpiScale();
	auto font_data = EX_PASS(FontFactory().makeFont(font_path, font_size));
	auto font_image = EX_PASS(VulkanImage::createAndUpload(device, font_data.image));
	auto font_image_view = VulkanImageView::create(device, font_image);
	return Font(std::move(font_data.core), std::move(font_image_view));
}

float2 Font::drawPos(const string32 &text, const FRect &rect, const FontStyle &style) const {
	float2 pos = rect.min();
	if(style.halign != HAlign::left || style.valign != VAlign::top) {
		FRect extents = (FRect)m_core.evalExtents(text);
		float2 center = rect.center() - extents.center();

		bool hleft = style.halign == HAlign::left, hcenter = style.halign == HAlign::center;
		bool vtop = style.valign == VAlign::top, vcenter = style.valign == VAlign::center;

		pos.x = hleft ? rect.x() : hcenter ? center.x : rect.ex() - extents.ex();
		pos.y = vtop ? rect.y() : vcenter ? center.y : rect.ey() - extents.ey();
	}

	return float2((int)(pos.x + 0.5f), (int)(pos.y + 0.5f));
}

FRect Font::draw(Canvas2D &out, const FRect &rect, const FontStyle &style,
				 const string32 &text) const {
	auto pos = drawPos(text, rect, style);

	PodVector<float2> pos_buf(text.length() * 4), uv_buf(text.length() * 4);
	int num_verts = m_core.genQuads(text, pos_buf, uv_buf, pos) * 4;
	CSpan<float2> positions(pos_buf.data(), num_verts), uvs(uv_buf.data(), num_verts);

	auto prev_mat = out.getMaterial();
	if(style.shadow_color != ColorId::transparent) {
		out.pushViewMatrix();
		out.setMaterial({m_texture, style.shadow_color, SimpleBlendingMode::normal});
		out.mulViewMatrix(translation(float3(1.0f, 1.0f, 0.0f)));
		// TODO: increase out_rect when rendering with shadow?
		// TODO: shadows could use same set of vertex data
		out.addQuads(positions, uvs);
		out.popViewMatrix();
	}
	out.setMaterial({m_texture, style.text_color, SimpleBlendingMode::normal});
	out.addQuads(positions, uvs);
	out.setMaterial(prev_mat);

	return enclose(positions);
}
}
