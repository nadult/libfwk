/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_opengl.h"
#include <algorithm>

namespace fwk {

// static int s_gl_primitives[PrimitiveTypeId::count] = {GL_POINTS, GL_LINES, GL_TRIANGLES,
//													  GL_TRIANGLE_STRIP, GL_QUADS};

/*
void renderMesh(const Stream &stream, PTexture tex, Color color) {
	PROFILE("Renderer::renderMesh");
	if(!stream.vertex_count)
		return;
	DASSERT(stream.positions);

	glColor4ub(color.r, color.g, color.b, color.a);
	if(tex)
		tex->bind();
	else
		DTexture::unbind();

	glVertexPointer(3, GL_FLOAT, 0, stream.positions);
	glEnableClientState(GL_VERTEX_ARRAY);

	if(stream.normals) {
		glNormalPointer(GL_FLOAT, 0, stream.normals);
		glEnableClientState(GL_NORMAL_ARRAY);
	} else
		glDisableClientState(GL_NORMAL_ARRAY);

	if(stream.tex_coords) {
		glTexCoordPointer(2, GL_FLOAT, 0, stream.tex_coords);
		glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	}

	glDrawArrays(s_gl_primitives[stream.primitive_type], 0, stream.vertex_count);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}*/

static const char *fragment_shader_tex_src =
	"#version 100\n"
	"uniform sampler2D tex; 											\n"
	"uniform lowp vec4 color;											\n"
	"varying mediump vec2 tex_coord;									\n"
	"void main() {														\n"
	"	gl_FragColor = color * texture2D(tex, tex_coord);				\n"
	"}																	\n";

static const char *fragment_shader_flat_src =
	"#version 100\n"
	"uniform lowp vec4 color;											\n"
	"void main() {														\n"
	"	gl_FragColor = color;											\n"
	"}																	\n";

static const char *vertex_shader_src =
	"#version 100\n"
	"uniform mat4 view_matrix, proj_matrix;								\n"
	"attribute vec3 in_pos;												\n"
	"attribute vec2 in_tex_coord;										\n"
	"varying vec2 tex_coord;  											\n"
	"void main() {														\n"
	"	gl_Position = proj_matrix * view_matrix * vec4(in_pos, 1.0);	\n"
	"	tex_coord = in_tex_coord;										\n"
	"} 																	\n";

static PProgram makeProgram(bool with_texture) {
	Shader vertex_shader(Shader::tVertex), fragment_shader(Shader::tFragment);
	vertex_shader.setSource(vertex_shader_src);
	fragment_shader.setSource(with_texture ? fragment_shader_tex_src : fragment_shader_flat_src);

	PProgram out = new Program(vertex_shader, fragment_shader);
	out->bindAttribLocation("in_pos", 0);
	out->bindAttribLocation("in_tex_coord", 1);
	return out;
}

Renderer::Renderer(const IRect &viewport) : m_viewport(viewport) {
	m_tex_program = makeProgram(true);
	m_tex_program->setUniform("tex", 0);
	m_flat_program = makeProgram(false);
	lookAt(float2(0.0f, 0.0f));
}

void Renderer::lookAt(const float2 &look_at) {
	m_projection_matrix =
		ortho(m_viewport.min.x, m_viewport.max.x, m_viewport.min.y, m_viewport.max.y, -1.0f, 1.0f);
	m_view_matrix = translation(0, m_viewport.height(), 0) * scaling(1, -1, 1);
	m_view_matrix = translation(-look_at.x, look_at.y, 0.0f) * m_view_matrix;

	m_inv_view_matrix = inverse(m_view_matrix);
	m_flat_program->setUniform("view_matrix", m_view_matrix);
	m_flat_program->setUniform("proj_matrix", m_projection_matrix);
	m_tex_program->setUniform("view_matrix", m_view_matrix);
	m_tex_program->setUniform("proj_matrix", m_projection_matrix);

	m_rects.clear();
}

void Renderer::addRect(const FRect &rect, const FRect &tex_rect, PTexture texture, RectStyle style) {
	m_rects.emplace_back(RectInstance{rect, tex_rect, texture, style});
}

void Renderer::render() {
	//	setupView(translation(m_viewport.min.x, m_viewport.min.y, -m_zero_depth - 1000.0f) * m_w2s);
	//	setDepthTest(true);
	renderRects();
	//	glDepthMask(1);
	DTexture::unbind();

	m_rects.clear();
}

void Renderer::renderRects() const {
	if(m_rects.empty())
		return;

	//TODO: make it a bit more efficient :)
	
	vector<float3> positions(m_rects.size() * 4);
	vector<float4> colors(m_rects.size() * 4);
	vector<float2> tex_coords(m_rects.size() * 4);

	for(int n = 0; n < (int)m_rects.size(); n++) {
		float2 corners[4];
		m_rects[n].rect.getCorners(corners);
		m_rects[n].tex_rect.getCorners(&tex_coords[n * 4]);
		for(int i = 0; i < 4; i++) {
			colors[n * 4 + i] = m_rects[n].style.fill_color;
			positions[n * 4 + i] = float3(corners[i], 0.0f);
		}
	}

	VertexArray array;
	VertexBuffer buf_pos, buf_color, buf_tex_coord;

	array.bind();
	buf_pos.setData(sizeof(float3) * positions.size(), positions.data(), GL_STATIC_DRAW);
	array.addAttrib(3, GL_FLOAT, 0, sizeof(float3), 0);

//	buf_color.setData(sizeof(float4) * colors.size(), colors.data(), GL_STATIC_DRAW);
//	array.addAttrib(4, GL_FLOAT, 0, sizeof(float4), 0);

	buf_tex_coord.setData(sizeof(float2) * tex_coords.size(), tex_coords.data(), GL_STATIC_DRAW);
	array.addAttrib(2, GL_FLOAT, 0, sizeof(float2), 0);

	VertexArray::unbind();
	VertexBuffer::unbind();

	array.bind();

	PTexture cur_tex;
	m_flat_program->bind();

	for(int n = 0; n < (int)m_rects.size(); n++) {
		if(m_rects[n].style.fill_color != Color::transparent) {
			if(m_rects[n].texture)
				m_rects[n].texture->bind();
			PProgram program = (m_rects[n].texture? m_tex_program : m_flat_program);
			program->setUniform("color", (float4)m_rects[n].style.fill_color);
			program->bind();
			glDrawArrays(GL_TRIANGLE_FAN, n * 4, 4);
		}
		if(m_rects[n].style.border_color != Color::transparent) {
			m_flat_program->bind();
			m_flat_program->setUniform("color", (float4)m_rects[n].style.border_color);
			glDrawArrays(GL_LINE_LOOP, n * 4, 4);
		}
	}

	VertexArray::unbind();
	DTexture::unbind();
	Program::unbind();
}
}
