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

struct ProgramFactory {
	PProgram operator()(const string &name) const {
		bool with_texture = name == "with_texture";
		Shader vertex_shader(Shader::tVertex), fragment_shader(Shader::tFragment);
		vertex_shader.setSource(vertex_shader_src);
		fragment_shader.setSource(with_texture ? fragment_shader_tex_src
											   : fragment_shader_flat_src);

		PProgram out = make_shared<Program>(vertex_shader, fragment_shader);
		out->bindAttribLocation("in_pos", 0);
		out->bindAttribLocation("in_color", 1);
		out->bindAttribLocation("in_tex_coord", 2);

		if(with_texture)
			out->setUniform("tex", 0);
		return out;
	}
};


Renderer::Renderer(const Matrix4 &projection_matrix) :MatrixStack(projection_matrix) {
	static ResourceManager<Program, ProgramFactory> mgr;
	m_tex_program = mgr["with_texture"];
	m_flat_program = mgr["without_texture"];
}

void Renderer::addDrawCall(const DrawCall &draw_call, const Material &material,
						   const Matrix4 &matrix) {
	m_instances.emplace_back(Instance{fullMatrix() * matrix, material, draw_call});
}
	
void Renderer::addLines(const float3 *positions, int count, Color color, const Matrix4 &matrix) {
	m_lines.emplace_back(LineInstance{fullMatrix() * matrix, (int)m_line_positions.size(), count});
	m_line_positions.insert(m_line_positions.end(), positions, positions + count);
	m_line_colors.resize(m_line_colors.size() + count, color);
}
	
void Renderer::addBBoxLines(const FBox &bbox, Color color, const Matrix4 &matrix) {
	float3 verts[8];
	bbox.getCorners(verts);

	int indices[] = { 0, 1, 1, 3, 3, 2, 2, 0, 4, 5, 5, 7, 7, 6, 6, 4, 0, 4, 1, 5, 3, 7, 2, 6 };
	float3 out_verts[arraySize(indices)];
	for(int i = 0; i < arraySize(indices); i++)
		out_verts[i] = verts[indices[i]];
	addLines(out_verts, arraySize(out_verts), color, matrix);
}

void Renderer::render() {
	GfxDevice::setBlendingMode(GfxDevice::bmDisabled);
	glEnable(GL_CULL_FACE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(1);

	for(const auto &instance : m_instances) {
		auto &program = (instance.material.texture() ? m_tex_program : m_flat_program);
		program->bind();
		program->setUniform("proj_view_matrix", instance.matrix);
		if(instance.material.texture())
			instance.material.texture()->bind();
		instance.draw_call.issue();
	}

	VertexArray line_array({make_shared<VertexBuffer>(m_line_positions),
			                make_shared<VertexBuffer>(m_line_colors),
							VertexArraySource(float2(0, 0))});
	DTexture::unbind();
	m_flat_program->bind();
	for(const auto &instance : m_lines) {
		m_flat_program->setUniform("proj_view_matrix", instance.matrix);
		line_array.draw(PrimitiveType::lines, instance.count, instance.first);
	}

	m_instances.clear();
	m_line_positions.clear();
	m_line_colors.clear();
	m_lines.clear();

	DTexture::unbind();
	Program::unbind();
}
}
