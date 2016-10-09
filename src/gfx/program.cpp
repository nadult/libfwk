/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of Junkers.*/

#include "fwk_gfx.h"
#include "fwk_opengl.h"

namespace fwk {

Program::Program(const Shader &vertex, const Shader &fragment,
				 const vector<string> &location_names) {
	DASSERT(vertex.type() == ShaderType::vertex && vertex.isValid());
	DASSERT(fragment.type() == ShaderType::fragment && fragment.isValid());

	m_id = glCreateProgram();
	testGlError("Error while creating shader program");

	try {
		glAttachShader(m_id, vertex.id());
		testGlError("Error while attaching vertex shader to program");
		glAttachShader(m_id, fragment.id());
		testGlError("Error while attaching fragment shader to program");

		for(int l = 0; l < location_names.size(); l++) {
			glBindAttribLocation(m_id, l, location_names[l].c_str());
			testGlError("Error while binding attribute");
		}

		glLinkProgram(m_id);
		testGlError("Error while linking program");

	} catch(...) {
		glDeleteProgram(m_id);
		throw;
	}
}

static Shader loadShader(const string &file_name, const string &predefined_macros,
						 ShaderType type) {
	Loader loader(file_name);
	return Shader(type, loader, predefined_macros);
}

Program::Program(const string &vsh_file_name, const string &fsh_file_name,
				 const string &predefined_macros, const vector<string> &location_names)
	: Program(loadShader(vsh_file_name, predefined_macros, ShaderType::vertex),
			  loadShader(fsh_file_name, predefined_macros, ShaderType::fragment), location_names) {}

Program::~Program() {
	if(m_id)
		glDeleteProgram(m_id);
}

string Program::getInfo() const {
	char buf[4096];
	glGetProgramInfoLog(m_id, 4096, 0, buf);
	return string(buf);
}

ProgramBinder::ProgramBinder(PProgram program) : m_program(move(program)) { DASSERT(m_program); }

namespace {

	unsigned s_active_program;
}

void ProgramBinder::bind() const {
	if(s_active_program == id())
		return;

	s_active_program = id();
	glUseProgram(id());
	testGlError("Error while binding");
}

ProgramBinder::~ProgramBinder() {
	if(s_active_program == id())
		unbind();
}

void ProgramBinder::unbind() {
	if(s_active_program == 0)
		return;
	s_active_program = 0;
	glUseProgram(0);
}

void ProgramBinder::setUniform(const char *name, float v) {
	bind();
	glUniform1f(glGetUniformLocation(id(), name), v);
}

void ProgramBinder::setUniform(const char *name, CRange<float> range) {
	bind();
	glUniform1fv(glGetUniformLocation(id(), name), range.size(), range.data());
}

void ProgramBinder::setUniform(const char *name, CRange<Matrix4> range) {
	bind();
	glUniformMatrix4fv(glGetUniformLocation(id(), name), range.size(), false,
					   (float *)range.data());
}

void ProgramBinder::setUniform(const char *name, CRange<float2> range) {
	bind();
	glUniform2fv(glGetUniformLocation(id(), name), range.size(), (float *)range.data());
}

void ProgramBinder::setUniform(const char *name, CRange<float3> range) {
	bind();
	glUniform3fv(glGetUniformLocation(id(), name), range.size(), (float *)range.data());
}

void ProgramBinder::setUniform(const char *name, CRange<float4> range) {
	bind();
	glUniform4fv(glGetUniformLocation(id(), name), range.size(), (float *)range.data());
}

void ProgramBinder::setUniform(const char *name, int v) {
	bind();
	glUniform1i(glGetUniformLocation(id(), name), v);
}

void ProgramBinder::setUniform(const char *name, const int2 &v) {
	bind();
	glUniform1iv(glGetUniformLocation(id(), name), 2, &v[0]);
}

void ProgramBinder::setUniform(const char *name, const int3 &v) {
	bind();
	glUniform1iv(glGetUniformLocation(id(), name), 3, &v[0]);
}

void ProgramBinder::setUniform(const char *name, const int4 &v) {
	bind();
	glUniform1iv(glGetUniformLocation(id(), name), 4, &v[0]);
}

void ProgramBinder::setUniform(const char *name, const float2 &v) {
	bind();
	glUniform2f(glGetUniformLocation(id(), name), v[0], v[1]);
}

void ProgramBinder::setUniform(const char *name, const float3 &v) {
	bind();
	glUniform3f(glGetUniformLocation(id(), name), v[0], v[1], v[2]);
}

void ProgramBinder::setUniform(const char *name, const float4 &v) {
	bind();
	glUniform4f(glGetUniformLocation(id(), name), v[0], v[1], v[2], v[3]);
}

void ProgramBinder::setUniform(const char *name, const Matrix4 &matrix) {
	bind();
	glUniformMatrix4fv(glGetUniformLocation(id(), name), 1, 0, &matrix[0][0]);
}

int ProgramBinder::getUniformLocation(const char *name) const {
	return glGetUniformLocation(id(), name);
}
}
