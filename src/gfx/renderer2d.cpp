/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_opengl.h"
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
	"attribute vec3 in_pos;												\n"
	"attribute vec4 in_color;											\n"
	"attribute vec2 in_tex_coord;										\n"
	"varying vec2 tex_coord;  											\n"
	"varying vec4 color;  												\n"
	"void main() {														\n"
	"	gl_Position = proj_view_matrix * vec4(in_pos, 1.0);				\n"
	"	tex_coord = in_tex_coord;										\n"
	"	color = in_color;												\n"
	"} 																	\n";

struct ProgramFactory2D {
	PProgram operator()(const string &name) const {
		bool with_texture = name == "with_texture";
		Shader vertex_shader(Shader::tVertex), fragment_shader(Shader::tFragment);
		vertex_shader.setSource(vertex_shader_2d_src);
		fragment_shader.setSource(with_texture ? fragment_shader_2d_tex_src
											   : fragment_shader_2d_flat_src);

		return make_immutable<Program>(vertex_shader, fragment_shader,
								 vector<string>{"in_pos", "in_color", "in_tex_coord"});
	}
};

Renderer2D::Renderer2D(const IRect &viewport)
	: MatrixStack(simpleProjectionMatrix(viewport), simpleViewMatrix(viewport, float2(0, 0))),
	  m_viewport(viewport) {
	static ResourceManager<Program, ProgramFactory2D> mgr;
	m_tex_program = mgr["with_texture"];
	m_flat_program = mgr["without_texture"];
}

void Renderer2D::setViewPos(const float2 &view_pos) {
	MatrixStack::setViewMatrix(simpleViewMatrix(m_viewport, view_pos));
}

Matrix4 Renderer2D::simpleProjectionMatrix(const IRect &viewport) {
	return ortho(viewport.min.x, viewport.max.x, viewport.min.y, viewport.max.y, -1.0f, 1.0f);
}

Matrix4 Renderer2D::simpleViewMatrix(const IRect &viewport, const float2 &look_at) {
	return translation(0, viewport.height(), 0) * scaling(1, -1, 1) *
		   translation(-look_at.x, -look_at.y, 0.0f);
}

void Renderer2D::addFilledRect(const FRect &rect, const FRect &tex_rect, const Material &material) {
	float3 pos[4];
	float2 pos_2d[4], tex_coords[4];

	rect.getCorners(pos_2d);
	tex_rect.getCorners(tex_coords);
	for(int n = 0; n < 4; n++)
		pos[n] = float3(pos_2d[n], 0.0f);

	addQuads(pos, tex_coords, nullptr, 1, material);
}

void Renderer2D::addRect(const FRect &rect, const Material &material) {
	float3 pos[4];
	float2 pos_2d[4], tex_coords[4];
	rect.getCorners(pos_2d);
	for(int n = 0; n < 4; n++)
		pos[n] = float3(pos_2d[n], 0.0f);

	Element &elem = makeElement(PrimitiveType::lines, PTexture());
	int vertex_offset = (int)m_positions.size();

	m_positions.insert(m_positions.end(), pos, pos + 4);
	m_colors.resize(m_colors.size() + 4, material.color());
	m_tex_coords.resize(m_tex_coords.size() + 4, float2(0, 0));

	const int num_indices = 8;
	int indices[num_indices] = {0, 1, 1, 2, 2, 3, 3, 0};
	for(int i = 0; i < num_indices; i++)
		m_indices.emplace_back(vertex_offset + indices[i]);
	elem.num_indices += num_indices;
}

Renderer2D::Element &Renderer2D::makeElement(PrimitiveType::Type primitive_type, PTexture texture) {
	// TODO: merging won't work for triangle strip (have to add some more indices)

	if(m_elements.empty() || m_elements.back().primitive_type != primitive_type ||
	   m_elements.back().texture != texture || fullMatrix() != m_elements.back().matrix)
		m_elements.emplace_back(
			Element{fullMatrix(), texture, (int)m_indices.size(), 0, primitive_type});
	return m_elements.back();
}

void Renderer2D::addLines(const float3 *pos, const Color *color, int num_lines,
						  const Material &material) {
	Element &elem = makeElement(PrimitiveType::lines, material.texture());
	int vertex_offset = (int)m_positions.size();
	int num_vertices = num_lines * 2;

	m_positions.insert(m_positions.end(), pos, pos + num_vertices);
	m_colors.insert(m_colors.end(), color, color + num_vertices);
	for(int n = (int)m_colors.size() - num_vertices; n < (int)m_colors.size(); n++)
		m_colors[n] = m_colors[n] * material.color();
	m_tex_coords.resize(m_tex_coords.size() + num_vertices, float2(0, 0));

	for(int n = 0; n < num_vertices; n++)
		m_indices.emplace_back(vertex_offset + n);
	elem.num_indices += num_lines * 2;
}

void Renderer2D::addQuads(const float3 *pos, const float2 *tex_coord, const Color *color,
						  int num_quads, const Material &material) {
	Element &elem = makeElement(PrimitiveType::triangles, material.texture());
	int vertex_offset = (int)m_positions.size();
	int num_vertices = num_quads * 4;

	m_positions.insert(m_positions.end(), pos, pos + num_vertices);
	if(color) {
		m_colors.insert(m_colors.end(), color, color + num_vertices);
		for(int n = (int)m_colors.size() - num_vertices; n < (int)m_colors.size(); n++)
			m_colors[n] = m_colors[n] * material.color();
	} else { m_colors.resize(m_colors.size() + num_vertices, material.color()); }
	m_tex_coords.insert(m_tex_coords.end(), tex_coord, tex_coord + num_vertices);

	m_indices.reserve(m_indices.size() + num_quads * 6);
	for(int n = 0; n < num_quads; n++) {
		int inds[6] = {0, 1, 2, 0, 2, 3};
		for(int i = 0; i < 6; i++)
			m_indices.emplace_back(vertex_offset + n * 4 + inds[i]);
	}
	elem.num_indices += num_quads * 6;
}

void Renderer2D::addTris(const float3 *pos, const float2 *tex_coord, const Color *color,
						 int num_tris, const Material &material) {
	Element &elem = makeElement(PrimitiveType::triangles, material.texture());
	int vertex_offset = (int)m_positions.size();
	int num_vertices = num_tris * 3;

	m_positions.insert(m_positions.end(), pos, pos + num_vertices);
	m_colors.insert(m_colors.end(), color, color + num_vertices);
	for(int n = (int)m_colors.size() - num_vertices; n < (int)m_colors.size(); n++)
		m_colors[n] = m_colors[n] * material.color();
	m_tex_coords.insert(m_tex_coords.end(), tex_coord, tex_coord + num_vertices);

	m_indices.reserve(m_indices.size() + num_tris * 3);
	for(int n = 0; n < num_tris; n++) {
		int inds[3] = {0, 1, 2};
		for(int i = 0; i < 3; i++)
			m_indices.emplace_back(vertex_offset + n * 3 + inds[i]);
	}
	elem.num_indices += num_tris * 3;
}

void Renderer2D::render() {
	glViewport(m_viewport.min.x, m_viewport.min.y, m_viewport.width(), m_viewport.height());
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDepthMask(0);

	VertexArray array({make_immutable<VertexBuffer>(m_positions), make_immutable<VertexBuffer>(m_colors),
					   make_immutable<VertexBuffer>(m_tex_coords)},
					  make_immutable<IndexBuffer>(m_indices));

	for(const auto &element : m_elements) {
		auto &program = (element.texture ? m_tex_program : m_flat_program);
		ProgramBinder binder(program);
		binder.bind();
		if(element.texture)
			binder.setUniform("tex", 0);
		binder.setUniform("proj_view_matrix", element.matrix);
		if(element.texture)
			element.texture->bind();
		array.draw(element.primitive_type, element.num_indices, element.first_index);
	}

	m_positions.clear();
	m_colors.clear();
	m_tex_coords.clear();
	m_indices.clear();
	m_elements.clear();

	DTexture::unbind();
}
}
