/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_mesh.h"
#include "fwk_opengl.h"
#include "fwk_xml.h"
#include <algorithm>

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

struct ProgramFactory2D {
	PProgram operator()(const string &name) const {
		const char *src =
			name == "with_texture" ? fragment_shader_2d_tex_src : fragment_shader_2d_flat_src;
		Shader vertex_shader(ShaderType::vertex, vertex_shader_2d_src, "", name);
		Shader fragment_shader(ShaderType::fragment, src, "", name);

		return make_immutable<Program>(vertex_shader, fragment_shader,
									   vector<string>{"in_pos", "in_color", "in_tex_coord"});
	}
};

Renderer2D::Renderer2D(const IRect &viewport)
	: MatrixStack(simpleProjectionMatrix(viewport), simpleViewMatrix(viewport, float2(0, 0))),
	  m_viewport(viewport), m_current_scissor_rect(-1) {
	static ResourceManager<Program, ProgramFactory2D> mgr;
	m_tex_program = mgr["with_texture"];
	m_flat_program = mgr["without_texture"];
}

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
	if(m_chunks.empty() ||
	   m_chunks.back().positions.size() + num_verts > IndexBuffer::max_index_value)
		m_chunks.emplace_back();
	return m_chunks.back();
}

Renderer2D::Element &Renderer2D::makeElement(DrawChunk &chunk, PrimitiveType primitive_type,
											 shared_ptr<const DTexture> texture) {
	// TODO: merging won't work for triangle strip (have to add some more indices)
	auto &elems = chunk.elements;

	if(elems.empty() || elems.back().primitive_type != primitive_type ||
	   elems.back().texture != texture || fullMatrix() != elems.back().matrix ||
	   m_current_scissor_rect != elems.back().scissor_rect_id)
		elems.emplace_back(Element{fullMatrix(), move(texture), (int)chunk.indices.size(), 0,
								   m_current_scissor_rect, primitive_type});
	return elems.back();
}

void Renderer2D::addFilledRect(const FRect &rect, const FRect &tex_rect, CRange<FColor, 4> colors,
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
	int vertex_offset = (int)chunk.positions.size();
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
	int vertex_offset = (int)chunk.positions.size();
	chunk.appendVertices({p1, p2}, {}, {}, color);

	insertBack(chunk.indices, {vertex_offset, vertex_offset + 1});
	elem.num_indices += 2;
}

void Renderer2D::DrawChunk::appendVertices(CRange<float2> positions_, CRange<float2> tex_coords_,
										   CRange<FColor> colors_, FColor mat_color) {
	DASSERT(colors.size() == positions.size() || colors.empty());
	DASSERT(tex_coords.size() == positions.size() || tex_coords.empty());

	insertBack(positions, positions_);
	if(colors_.empty())
		colors.resize(positions.size(), IColor(mat_color));
	else
		for(auto col : colors_)
			colors.emplace_back(col * mat_color);
	if(tex_coords_.empty())
		tex_coords.resize(positions.size(), float2());
	else
		insertBack(tex_coords, tex_coords_);
}

void Renderer2D::addLines(CRange<float2> pos, CRange<FColor> color, FColor mat_color) {
	DASSERT(pos.size() % 2 == 0);

	auto &chunk = allocChunk(pos.size());
	Element &elem = makeElement(chunk, PrimitiveType::lines, PTexture());

	int vertex_offset = (int)chunk.positions.size();
	chunk.appendVertices(pos, {}, color, mat_color);
	for(int n = 0; n < (int)pos.size(); n++)
		chunk.indices.emplace_back(vertex_offset + n);
	elem.num_indices += (int)pos.size();
}

void Renderer2D::addQuads(CRange<float2> pos, CRange<float2> tex_coord, CRange<FColor> color,
						  const SimpleMaterial &material) {
	DASSERT(pos.size() % 4 == 0);

	auto &chunk = allocChunk(pos.size());
	Element &elem = makeElement(chunk, PrimitiveType::triangles, material.texture());
	int vertex_offset = (int)chunk.positions.size();
	int num_quads = pos.size() / 4;

	chunk.appendVertices(pos, tex_coord, color, material.color());
	for(int n = 0; n < num_quads; n++) {
		int inds[6] = {0, 1, 2, 0, 2, 3};
		for(int i = 0; i < 6; i++)
			chunk.indices.emplace_back(vertex_offset + n * 4 + inds[i]);
	}
	elem.num_indices += num_quads * 6;
}

void Renderer2D::addTris(CRange<float2> pos, CRange<float2> tex_coord, CRange<FColor> color,
						 const SimpleMaterial &material) {
	DASSERT(pos.size() % 3 == 0);

	int num_tris = pos.size() / 3;

	auto &chunk = allocChunk(pos.size());
	Element &elem = makeElement(chunk, PrimitiveType::triangles, material.texture());
	int vertex_offset = (int)chunk.positions.size();
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
	if(m_scissor_rects.empty() || m_current_scissor_rect == -1 ||
	   m_scissor_rects[m_current_scissor_rect] != *rect) {
		m_scissor_rects.emplace_back(*rect);
		m_current_scissor_rect = (int)m_scissor_rects.size() - 1;
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
	for(const auto &chunk : m_chunks) {
		VertexArray array({make_immutable<VertexBuffer>(chunk.positions),
						   make_immutable<VertexBuffer>(chunk.colors),
						   make_immutable<VertexBuffer>(chunk.tex_coords)},
						  make_immutable<IndexBuffer>(chunk.indices));

		for(const auto &element : chunk.elements) {
			auto &program = (element.texture ? m_tex_program : m_flat_program);
			ProgramBinder binder(program);
			binder.bind();
			if(element.texture)
				binder.setUniform("tex", 0);
			binder.setUniform("proj_view_matrix", element.matrix);
			if(element.texture)
				element.texture->bind();

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

			array.draw(element.primitive_type, element.num_indices, element.first_index);
		}
	}

	glDisable(GL_SCISSOR_TEST);
	DTexture::unbind();

	clear();
}
}
