/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_opengl.h"
#include <algorithm>

namespace fwk {

static const int gl_primitive_type[] = {GL_POINTS, GL_LINES, GL_TRIANGLES, GL_TRIANGLE_STRIP};

static_assert(arraySize(gl_primitive_type) == PrimitiveType::count, "");

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

static PProgram makeProgram(bool with_texture) {
	Shader vertex_shader(Shader::tVertex), fragment_shader(Shader::tFragment);
	vertex_shader.setSource(vertex_shader_src);
	fragment_shader.setSource(with_texture ? fragment_shader_tex_src : fragment_shader_flat_src);

	PProgram out = make_shared<Program>(vertex_shader, fragment_shader);
	out->bindAttribLocation("in_pos", 0);
	out->bindAttribLocation("in_color", 1);
	out->bindAttribLocation("in_tex_coord", 2);
	return out;
}

Renderer::Renderer(const Matrix4 &projection_matrix) {
	m_tex_program = makeProgram(true);
	m_tex_program->setUniform("tex", 0);
	m_flat_program = makeProgram(false);
	setProjectionMatrix(projection_matrix);
	setViewMatrix(identity());
}

Renderer::Renderer(const IRect &viewport) : Renderer(simple2DProjectionMatrix(viewport)) {
	setViewMatrix(simple2DViewMatrix(viewport, float2(0, 0)));
}

Matrix4 Renderer::simple2DProjectionMatrix(const IRect &viewport) {
	return ortho(viewport.min.x, viewport.max.x, viewport.min.y, viewport.max.y, -1.0f, 1.0f);
}

Matrix4 Renderer::simple2DViewMatrix(const IRect &viewport, const float2 &look_at) {
	return translation(0, viewport.height(), 0) * scaling(1, -1, 1) *
		   translation(-look_at.x, -look_at.y, 0.0f);
}

void Renderer::setProjectionMatrix(const Matrix4 &projection_matrix) {
	m_projection_matrix = projection_matrix;
	m_flat_program->setUniform("proj_matrix", m_projection_matrix);
	m_tex_program->setUniform("proj_matrix", m_projection_matrix);
}

void Renderer::pushViewMatrix() { m_matrix_stack.push_back(m_view_matrix); }

void Renderer::popViewMatrix() {
	DASSERT(!m_matrix_stack.empty());
	m_view_matrix = m_matrix_stack.back();
	m_matrix_stack.pop_back();
}

void Renderer::mulViewMatrix(const Matrix4 &matrix) { m_view_matrix = m_view_matrix * matrix; }

void Renderer::setViewMatrix(const Matrix4 &matrix) { m_view_matrix = matrix; }

const Frustum Renderer::frustum() const { return Frustum(m_projection_matrix); }

void Renderer::add2DFilledRect(const FRect &rect, const FRect &tex_rect, const Material &material) {
	float3 pos[4];
	float2 pos_2d[4], tex_coords[4];

	rect.getCorners(pos_2d);
	tex_rect.getCorners(tex_coords);
	for(int n = 0; n < 4; n++)
		pos[n] = float3(pos_2d[n], 0.0f);

	addQuads(pos, tex_coords, nullptr, 1, material);
}

void Renderer::add2DRect(const FRect &rect, const Material &material) {
	float3 pos[4];
	float2 pos_2d[4], tex_coords[4];
	rect.getCorners(pos_2d);
	for(int n = 0; n < 4; n++)
		pos[n] = float3(pos_2d[n], 0.0f);

	Element &elem = makeElement(PrimitiveType::lines, nullptr);
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

void Renderer::addMesh(PMesh mesh, const Material &material, const Matrix4 &matrix) {
	pushViewMatrix();
	mulViewMatrix(matrix);

	Element &elem = makeElement(mesh->m_primitive_type, material.texture());

	int vertex_offset = (int)m_positions.size();
	int num_vertices = mesh->m_positions.size();

	m_positions.insert(m_positions.end(), mesh->m_positions.data(),
					   mesh->m_positions.data() + num_vertices);
	m_colors.resize(m_colors.size() + num_vertices, material.color());
	m_tex_coords.insert(m_tex_coords.end(), mesh->m_tex_coords.data(),
						mesh->m_tex_coords.data() + num_vertices);

	m_indices.reserve(m_indices.size() + num_vertices);
	for(int n = 0; n < num_vertices; n++)
		m_indices.emplace_back(vertex_offset + n);
	elem.num_indices += num_vertices;

	popViewMatrix();
}

Renderer::Element &Renderer::makeElement(PrimitiveType::Type primitive_type, PTexture texture) {
	// TODO: merging won't work for triangle strip (have to add some more indices)

	Matrix4 mult_matrix = m_projection_matrix * m_view_matrix;
	if(m_elements.empty() || m_elements.back().primitive_type != primitive_type ||
	   m_elements.back().texture != texture || mult_matrix != m_elements.back().matrix)
		m_elements.emplace_back(
			Element{mult_matrix, texture, (int)m_indices.size(), 0, primitive_type});
	return m_elements.back();
}

void Renderer::addLines(const float3 *pos, const Color *color, int num_lines,
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

void Renderer::addQuads(const float3 *pos, const float2 *tex_coord, const Color *color,
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

void Renderer::addTris(const float3 *pos, const float2 *tex_coord, const Color *color, int num_tris,
					   const Material &material) {
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

void Renderer::render(bool mode_3d) {
	if(mode_3d) {
		setBlendingMode(bmDisabled);
		glEnable(GL_CULL_FACE);
		glEnable(GL_DEPTH_TEST);
		glDepthFunc(GL_LESS);
		glDepthMask(1);
	} else {
		setBlendingMode(bmNormal);
		glDisable(GL_CULL_FACE);
		glDisable(GL_DEPTH_TEST);
		glDepthMask(0);
	}

	NiceVertexArray array({{make_shared<NiceVertexBuffer>(m_positions)},
						   {make_shared<NiceVertexBuffer>(m_colors), true},
						   {make_shared<NiceVertexBuffer>(m_tex_coords)}},
						  {make_shared<NiceIndexBuffer>(m_indices)});

	for(const auto &element : m_elements) {
		auto &program = (element.texture ? m_tex_program : m_flat_program);
		program->bind();
		program->setUniform("proj_view_matrix", element.matrix);
		if(element.texture)
			element.texture->bind();
		array.drawPrimitives(element.primitive_type, element.num_indices, element.first_index);
	}

	m_positions.clear();
	m_colors.clear();
	m_tex_coords.clear();
	m_indices.clear();
	m_elements.clear();

	DTexture::unbind();
	Program::unbind();
}

void Renderer::clearColor(Color color) {
	float4 col = color;
	glClearColor(col.x, col.y, col.z, col.w);
	glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::clearDepth(float depth_value) {
	glClearDepth(depth_value);
	glDepthMask(1);
	glClear(GL_DEPTH_BUFFER_BIT);
}

void Renderer::setBlendingMode(BlendingMode mode) {
	if(mode == bmDisabled)
		glDisable(GL_BLEND);
	else
		glEnable(GL_BLEND);

	if(mode == bmNormal)
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static IRect s_scissor_rect = IRect::empty();

void Renderer::setScissorRect(const IRect &rect) {
	//	s_scissor_rect = rect;
	//	glScissor(rect.min.x, s_viewport_size.y - rect.max.y, rect.width(), rect.height());
	//	testGlError("glScissor");
}

const IRect Renderer::getScissorRect() { return s_scissor_rect; }

void Renderer::setScissorTest(bool is_enabled) {
	if(is_enabled)
		glEnable(GL_SCISSOR_TEST);
	else
		glDisable(GL_SCISSOR_TEST);
}
}
