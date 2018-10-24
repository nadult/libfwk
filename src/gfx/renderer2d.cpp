// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_buffer.h"
#include "fwk/gfx/gl_device.h"
#include "fwk/gfx/gl_program.h"
#include "fwk/gfx/gl_shader.h"
#include "fwk/gfx/gl_texture.h"
#include "fwk/gfx/gl_vertex_array.h"
#include "fwk/gfx/opengl.h"
#include "fwk/gfx/renderer2d.h"
#include "fwk/hash_map.h"
#include "fwk/sys/xml.h"

namespace fwk {

static const char *fragment_shader_2d_tex_src =
	"#version 100\n"
	"uniform sampler2D tex; 											\n"
	"varying lowp vec4 color;											\n"
	"varying mediump vec2 tex_coord;									\n"
	"void main() {														\n"
	"	gl_FragColor = color * texture2D(tex, tex_coord);				\n"
	"}																	\n";

static const char *fragment_shader_2d_flat_src =
	"#version 100\n"
	"varying lowp vec4 color;											\n"
	"void main() {														\n"
	"	gl_FragColor = color;											\n"
	"}																	\n";

static const char *vertex_shader_2d_src =
	"#version 100\n"
	"uniform mat4 proj_view_matrix;										\n"
	"attribute vec2 in_pos;												\n"
	"attribute vec4 in_color;											\n"
	"attribute vec2 in_tex_coord;										\n"
	"varying vec2 tex_coord;  											\n"
	"varying vec4 color;  												\n"
	"void main() {														\n"
	"	gl_Position = proj_view_matrix * vec4(in_pos, 0.0, 1.0);		\n"
	"	tex_coord = in_tex_coord;										\n"
	"	color = in_color;												\n"
	"} 																	\n";

static PProgram getProgram(Str name) {
	if(auto out = GlDevice::instance().cacheFindProgram(name))
		return out;

	const char *src =
		name == "2d_with_texture" ? fragment_shader_2d_tex_src : fragment_shader_2d_flat_src;
	auto vsh = GlShader::make(ShaderType::vertex, vertex_shader_2d_src, "", name);
	auto fsh = GlShader::make(ShaderType::fragment, src, "", name);

	auto out = GlProgram::make(vsh, fsh, {"in_pos", "in_color", "in_tex_coord"});
	GlDevice::instance().cacheAddProgram(name, out);
	return out;
}

Renderer2D::Renderer2D(const IRect &viewport)
	: MatrixStack(simpleProjectionMatrix(viewport), simpleViewMatrix(viewport, float2(0, 0))),
	  m_viewport(viewport), m_current_scissor_rect(-1) {
	m_tex_program = getProgram("2d_with_texture");
	m_tex_program["tex"] = 0;
	m_flat_program = getProgram("2d_without_texture");
}

Renderer2D::~Renderer2D() = default;

void Renderer2D::setViewPos(const float2 &view_pos) {
	MatrixStack::setViewMatrix(simpleViewMatrix(m_viewport, view_pos));
}

Matrix4 Renderer2D::simpleProjectionMatrix(const IRect &viewport) {
	return ortho(viewport.x(), viewport.ex(), viewport.y(), viewport.ey(), -1.0f, 1.0f);
}

Matrix4 Renderer2D::simpleViewMatrix(const IRect &viewport, const float2 &look_at) {
	return translation(0, viewport.height(), 0) * scaling(1, -1, 1) *
		   translation(-look_at.x, -look_at.y, 0.0f);
}

Renderer2D::DrawChunk &Renderer2D::allocChunk(int num_verts) {
	if(!m_chunks || m_chunks.back().positions.size() + num_verts > 65535)
		m_chunks.emplace_back();
	return m_chunks.back();
}

Renderer2D::Element &Renderer2D::makeElement(DrawChunk &chunk, PrimitiveType primitive_type,
											 PTexture texture, BlendingMode bm) {
	// TODO: merging won't work for triangle strip (have to add some more indices)
	auto &elems = chunk.elements;

	if(!elems || elems.back().primitive_type != primitive_type || elems.back().texture != texture ||
	   elems.back().blending_mode != bm || fullMatrix() != elems.back().matrix ||
	   m_current_scissor_rect != elems.back().scissor_rect_id)
		elems.emplace_back(Element{fullMatrix(), move(texture), chunk.indices.size(), 0,
								   m_current_scissor_rect, primitive_type, bm});
	return elems.back();
}

void Renderer2D::addFilledRect(const FRect &rect, const FRect &tex_rect, CSpan<FColor, 4> colors,
							   const SimpleMaterial &material) {
	addQuads(rect.corners(), tex_rect.corners(), colors, material);
}

void Renderer2D::addFilledRect(const FRect &rect, const FRect &tex_rect,
							   const SimpleMaterial &material) {
	addQuads(rect.corners(), tex_rect.corners(), {}, material);
}

void Renderer2D::addRect(const FRect &rect, FColor color) {
	auto &chunk = allocChunk(4);
	Element &elem = makeElement(chunk, PrimitiveType::lines, PTexture());
	int vertex_offset = chunk.positions.size();
	chunk.appendVertices(rect.corners(), {}, {}, color);

	const int num_indices = 8;
	int indices[num_indices] = {0, 1, 1, 2, 2, 3, 3, 0};
	for(int i = 0; i < num_indices; i++)
		chunk.indices.emplace_back(vertex_offset + indices[i]);
	elem.num_indices += num_indices;
}

void Renderer2D::addLine(const float2 &p1, const float2 &p2, FColor color) {
	auto &chunk = allocChunk(2);
	Element &elem = makeElement(chunk, PrimitiveType::lines, PTexture());
	int vertex_offset = chunk.positions.size();
	chunk.appendVertices({p1, p2}, {}, {}, color);

	insertBack(chunk.indices, {vertex_offset, vertex_offset + 1});
	elem.num_indices += 2;
}

void Renderer2D::DrawChunk::appendVertices(CSpan<float2> positions_, CSpan<float2> tex_coords_,
										   CSpan<FColor> colors_, FColor mat_color) {
	DASSERT(colors.size() == positions.size() || !colors);
	DASSERT(tex_coords.size() == positions.size() || !tex_coords);

	insertBack(positions, positions_);
	if(colors_)
		for(auto col : colors_)
			colors.emplace_back(col * mat_color);
	else
		colors.resize(positions.size(), IColor(mat_color));
	if(tex_coords_)
		insertBack(tex_coords, tex_coords_);
	else
		tex_coords.resize(positions.size(), float2());
}

void Renderer2D::addLines(CSpan<float2> pos, CSpan<FColor> color, FColor mat_color) {
	DASSERT(pos.size() % 2 == 0);

	auto &chunk = allocChunk(pos.size());
	Element &elem = makeElement(chunk, PrimitiveType::lines, PTexture());

	int vertex_offset = chunk.positions.size();
	chunk.appendVertices(pos, {}, color, mat_color);
	for(int n = 0; n < pos.size(); n++)
		chunk.indices.emplace_back(vertex_offset + n);
	elem.num_indices += pos.size();
}

void Renderer2D::addQuads(CSpan<float2> pos, CSpan<float2> tex_coord, CSpan<FColor> color,
						  const SimpleMaterial &material) {
	DASSERT(pos.size() % 4 == 0);

	auto &chunk = allocChunk(pos.size());
	Element &elem =
		makeElement(chunk, PrimitiveType::triangles, material.texture(), material.blendingMode());
	int vertex_offset = chunk.positions.size();
	int num_quads = pos.size() / 4;

	chunk.appendVertices(pos, tex_coord, color, material.color());
	for(int n = 0; n < num_quads; n++) {
		int inds[6] = {0, 1, 2, 0, 2, 3};
		for(int i = 0; i < 6; i++)
			chunk.indices.emplace_back(vertex_offset + n * 4 + inds[i]);
	}
	elem.num_indices += num_quads * 6;
}

void Renderer2D::addTris(CSpan<float2> pos, CSpan<float2> tex_coord, CSpan<FColor> color,
						 const SimpleMaterial &material) {
	DASSERT(pos.size() % 3 == 0);

	int num_tris = pos.size() / 3;

	auto &chunk = allocChunk(pos.size());
	Element &elem =
		makeElement(chunk, PrimitiveType::triangles, material.texture(), material.blendingMode());
	int vertex_offset = chunk.positions.size();
	chunk.appendVertices(pos, tex_coord, color, material.color());

	for(int n = 0; n < num_tris; n++) {
		int inds[3] = {0, 1, 2};
		for(int i = 0; i < 3; i++)
			chunk.indices.emplace_back(vertex_offset + n * 3 + inds[i]);
	}
	elem.num_indices += num_tris * 3;
}

Maybe<IRect> Renderer2D::scissorRect() const {
	if(m_current_scissor_rect != -1)
		return m_scissor_rects[m_current_scissor_rect];
	return none;
}

void Renderer2D::setScissorRect(Maybe<IRect> rect) {
	if(!rect) {
		m_current_scissor_rect = -1;
		return;
	}
	if(!m_scissor_rects || m_current_scissor_rect == -1 ||
	   m_scissor_rects[m_current_scissor_rect] != *rect) {
		m_scissor_rects.emplace_back(*rect);
		m_current_scissor_rect = m_scissor_rects.size() - 1;
	}
}

void Renderer2D::clear() {
	m_chunks.clear();
	m_scissor_rects.clear();
	m_current_scissor_rect = -1;
}

void Renderer2D::render() {
	glViewport(m_viewport.x(), m_viewport.y(), m_viewport.width(), m_viewport.height());
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);
	glDisable(GL_SCISSOR_TEST);

	int prev_scissor_rect = -1;
	auto bm = BlendingMode::normal;

	for(const auto &chunk : m_chunks) {
		auto pbuffer = GlBuffer::make(BufferType::array, chunk.positions);
		auto cbuffer = GlBuffer::make(BufferType::array, chunk.colors);
		auto tbuffer = GlBuffer::make(BufferType::array, chunk.tex_coords);
		auto ibuffer = GlBuffer::make(BufferType::element_array, chunk.indices);

		auto vao = GlVertexArray::make();
		vao->set({move(pbuffer), move(cbuffer), move(tbuffer)},
				 defaultVertexAttribs<float2, IColor, float2>(), move(ibuffer), IndexType::uint32);

		for(const auto &element : chunk.elements) {
			auto &program = (element.texture ? m_tex_program : m_flat_program);
			program["proj_view_matrix"] = element.matrix;

			if(element.texture)
				element.texture->bind();
			if(bm != element.blending_mode) {
				bm = element.blending_mode;
				if(bm == BlendingMode::normal)
					glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				else if(bm == BlendingMode::additive)
					glBlendFunc(GL_ONE, GL_ONE);
			}

			if(element.scissor_rect_id != prev_scissor_rect) {
				if(element.scissor_rect_id == -1)
					glDisable(GL_SCISSOR_TEST);
				else if(prev_scissor_rect == -1)
					glEnable(GL_SCISSOR_TEST);

				if(element.scissor_rect_id != -1) {
					IRect rect = m_scissor_rects[element.scissor_rect_id];
					int min_y = m_viewport.height() - rect.ey();
					rect = {{rect.x(), max(0, min_y)}, {rect.ex(), min_y + rect.height()}};
					glScissor(rect.x(), rect.y(), rect.width(), rect.height());
				}

				prev_scissor_rect = element.scissor_rect_id;
			}

			program->use();
			vao->draw(element.primitive_type, element.num_indices, element.first_index);
		}
	}

	glDisable(GL_SCISSOR_TEST);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GlTexture::unbind();

	clear();
}

vector<pair<FRect, Matrix4>> Renderer2D::renderRects() const {
	vector<pair<FRect, Matrix4>> out;
	for(const auto &chunk : m_chunks) {
		for(const auto &elem : chunk.elements) {
			const int *inds = &chunk.indices[elem.first_index];
			int min_index = inds[0], max_index = inds[0];
			for(int i : intRange(elem.num_indices)) {
				max_index = max(max_index, inds[i]);
				min_index = min(min_index, inds[i]);
			}

			FRect erect = enclose(
				CSpan<float2>(&chunk.positions[min_index], &chunk.positions[max_index] + 1));
			out.emplace_back(erect, elem.matrix);
		}
	}

	return out;
}
}
