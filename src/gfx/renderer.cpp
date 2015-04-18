/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_opengl.h"
#include <algorithm>

namespace fwk {

static const char *fragment_shader_tex_src =
	"#version 100\n"
	"uniform sampler2D tex; 											\n"
	"varying lowp vec4 color;											\n"
	"varying mediump vec2 tex_coord;									\n"
	"void main() {														\n"
	"	gl_FragColor = color * texture2D(tex, tex_coord);				\n"
	"}																	\n";

static const char *fragment_shader_flat_src =
	"#version 100\n"
	"varying lowp vec4 color;											\n"
	"void main() {														\n"
	"	gl_FragColor = color;											\n"
	"}																	\n";

static const char *vertex_shader_src =
	"#version 100\n"
	"uniform mat4 view_matrix, proj_matrix;								\n"
	"attribute vec3 in_pos;												\n"
	"attribute vec4 in_color;											\n"
	"attribute vec2 in_tex_coord;										\n"
	"varying vec2 tex_coord;  											\n"
	"varying vec4 color;  												\n"
	"void main() {														\n"
	"	gl_Position = proj_matrix * view_matrix * vec4(in_pos, 1.0);	\n"
	"	tex_coord = in_tex_coord;										\n"
	"	color = in_color;												\n"
	"} 																	\n";

static PProgram makeProgram(bool with_texture) {
	Shader vertex_shader(Shader::tVertex), fragment_shader(Shader::tFragment);
	vertex_shader.setSource(vertex_shader_src);
	fragment_shader.setSource(with_texture ? fragment_shader_tex_src : fragment_shader_flat_src);

	PProgram out = new Program(vertex_shader, fragment_shader);
	out->bindAttribLocation("in_pos", 0);
	out->bindAttribLocation("in_color", 1);
	out->bindAttribLocation("in_tex_coord", 2);
	return out;
}

Renderer::Renderer(const IRect &viewport) : m_viewport(viewport) {
	m_tex_program = makeProgram(true);
	m_tex_program->setUniform("tex", 0);
	m_flat_program = makeProgram(false);
	m_projection_matrix =
		ortho(m_viewport.min.x, m_viewport.max.x, m_viewport.min.y, m_viewport.max.y, -1.0f, 1.0f);
	m_flat_program->setUniform("proj_matrix", m_projection_matrix);
	m_tex_program->setUniform("proj_matrix", m_projection_matrix);
	setViewMatrix(float2(0.0f, 0.0f));
}

void Renderer::pushViewMatrix() { m_matrix_stack.push_back(m_view_matrix); }

void Renderer::popViewMatrix() {
	DASSERT(!m_matrix_stack.empty());
	m_view_matrix = m_matrix_stack.back();
	m_matrix_stack.pop_back();
}

void Renderer::mulViewMatrix(const Matrix4 &matrix) { m_view_matrix = m_view_matrix * matrix; }

void Renderer::setViewMatrix(const Matrix4 &matrix) { m_view_matrix = matrix; }

void Renderer::setViewMatrix(const float2 &look_at) {
	m_view_matrix = translation(0, m_viewport.height(), 0) * scaling(1, -1, 1) *
					translation(-look_at.x, -look_at.y, 0.0f);
}

void Renderer::addRect(const FRect &rect, const FRect &tex_rect, PTexture texture, Color color) {
	float3 pos[4];
	float2 pos_2d[4], tex_coords[4];

	rect.getCorners(pos_2d);
	tex_rect.getCorners(tex_coords);
	for(int n = 0; n < 4; n++)
		pos[n] = float3(pos_2d[n], 0.0f);

	Color colors[4] = {color, color, color, color};
	addQuads(pos, tex_coords, colors, 1, texture);
}

void Renderer::addLineRect(const FRect &rect, Color color) {
	float3 pos[4];
	float2 pos_2d[4], tex_coords[4];
	rect.getCorners(pos_2d);
	for(int n = 0; n < 4; n++)
		pos[n] = float3(pos_2d[n], 0.0f);

	Element &elem = makeElement(GL_LINE_STRIP, nullptr);
	int vertex_offset = (int)m_positions.size();

	m_positions.insert(m_positions.end(), pos, pos + 4);
	m_colors.resize(m_colors.size() + 4, color);
	m_tex_coords.resize(m_tex_coords.size() + 4, float2(0, 0));

	const int num_indices = 8;
	int indices[num_indices] = {0, 1, 1, 2, 2, 3, 3, 0};
	for(int i = 0; i < num_indices; i++)
		m_indices.emplace_back(vertex_offset + indices[i]);
	elem.num_indices += num_indices;
}

Renderer::Element &Renderer::makeElement(int primitive_type, PTexture texture) {
	if(m_elements.empty() || m_elements.back().primitive_type != primitive_type ||
	   m_elements.back().texture != texture || m_view_matrix != m_elements.back().view_matrix)
		m_elements.emplace_back(
			Element{m_view_matrix, texture, (int)m_indices.size(), 0, primitive_type});
	return m_elements.back();
}

void Renderer::addLines(const float3 *pos, const Color *color, int num_lines) {
	Element &elem = makeElement(GL_LINE_STRIP, nullptr);
	int vertex_offset = (int)m_positions.size();
	int num_vertices = num_lines * 2;

	m_positions.insert(m_positions.end(), pos, pos + num_vertices);
	m_colors.insert(m_colors.end(), color, color + num_vertices);
	m_tex_coords.resize(m_tex_coords.size() + num_vertices, float2(0, 0));

	for(int n = 0; n < num_vertices; n++)
		m_indices.emplace_back(vertex_offset + n);
	elem.num_indices += num_lines * 2;
}

void Renderer::addQuads(const float3 *pos, const float2 *tex_coord, const Color *color,
						int num_quads, PTexture texture) {
	Element &elem = makeElement(GL_TRIANGLES, texture);
	int vertex_offset = (int)m_positions.size();
	int num_vertices = num_quads * 4;

	m_positions.insert(m_positions.end(), pos, pos + num_vertices);
	m_colors.insert(m_colors.end(), color, color + num_vertices);
	m_tex_coords.insert(m_tex_coords.end(), tex_coord, tex_coord + num_vertices);

	m_indices.reserve(m_indices.size() + num_quads * 6);
	for(int n = 0; n < num_quads; n++) {
		int inds[6] = {0, 1, 2, 0, 2, 3};
		for(int i = 0; i < 6; i++)
			m_indices.emplace_back(vertex_offset + n * 4 + inds[i]);
	}
	elem.num_indices += num_quads * 6;
}

void Renderer::addTris(const float3 *pos, const float2 *tex_coord, const Color *color, int num_tris,
					   PTexture texture) {
	Element &elem = makeElement(GL_TRIANGLES, texture);
	int vertex_offset = (int)m_positions.size();
	int num_vertices = num_tris * 3;

	m_positions.insert(m_positions.end(), pos, pos + num_vertices);
	m_colors.insert(m_colors.end(), color, color + num_vertices);
	m_tex_coords.insert(m_tex_coords.end(), tex_coord, tex_coord + num_vertices);

	m_indices.reserve(m_indices.size() + num_tris * 3);
	for(int n = 0; n < num_tris; n++) {
		int inds[3] = {0, 1, 2};
		for(int i = 0; i < 3; i++)
			m_indices.emplace_back(vertex_offset + n * 3 + inds[i]);
	}
	elem.num_indices += num_tris * 3;
}

void Renderer::render() {
	if(m_elements.empty())
		return;

	setBlendingMode(bmNormal);

	VertexArray array;
	VertexBuffer buf_pos, buf_color, buf_tex_coord;

	array.bind();
	buf_pos.setData(sizeof(float3) * m_positions.size(), m_positions.data(), GL_STATIC_DRAW);
	array.addAttrib(3, GL_FLOAT, 0, sizeof(float3), 0);

	static_assert(sizeof(Color) == sizeof(unsigned char) * 4, "");
	buf_color.setData(sizeof(Color) * m_colors.size(), m_colors.data(), GL_STATIC_DRAW);
	array.addAttrib(4, GL_UNSIGNED_BYTE, 1, sizeof(Color), 0);

	buf_tex_coord.setData(sizeof(float2) * m_tex_coords.size(), m_tex_coords.data(),
						  GL_STATIC_DRAW);
	array.addAttrib(2, GL_FLOAT, 0, sizeof(float2), 0);

	GLuint index_buffer;
	glGenBuffers(1, &index_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(uint), m_indices.data(),
				 GL_STATIC_DRAW);

	VertexArray::unbind();
	VertexBuffer::unbind();

	array.bind();

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
	for(const auto &element : m_elements) {
		auto &program = (element.texture ? m_tex_program : m_flat_program);
		program->bind();
		program->setUniform("view_matrix", element.view_matrix);
		if(element.texture)
			element.texture->bind();
		glDrawElements(element.primitive_type, element.num_indices, GL_UNSIGNED_INT,
					   (void *)(element.first_index * sizeof(uint)));
	}

	m_positions.clear();
	m_colors.clear();
	m_tex_coords.clear();
	m_indices.clear();
	m_elements.clear();

	glDeleteBuffers(1, &index_buffer);
	VertexArray::unbind();
	DTexture::unbind();
	Program::unbind();
}
}
