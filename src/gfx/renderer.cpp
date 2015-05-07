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
	setViewMatrix(Matrix4::identity());
}

void Renderer::addDrawCall(const DrawCall &draw_call, const Material &material, const Matrix4 &matrix) {
	m_instances.emplace_back(Instance{fullMatrix() * matrix, material, draw_call});
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

	m_instances.clear();

	DTexture::unbind();
	Program::unbind();
}
}
