/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of Junkers.*/

#include "fwk_gfx.h"
#include "fwk_opengl.h"

namespace fwk {

Program::Program(const Shader &vertex, const Shader &fragment,
				 const vector<string> &location_names) {
	DASSERT(vertex.getType() == Shader::tVertex);
	DASSERT(fragment.getType() == Shader::tFragment);

	m_handle = glCreateProgram();
	testGlError("Error while creating shader program");

	try {
		glAttachShader(m_handle, vertex.getHandle());
		testGlError("Error while attaching vertex shader to program");
		glAttachShader(m_handle, fragment.getHandle());
		testGlError("Error while attaching fragment shader to program");

		for(uint l = 0; l < location_names.size(); l++) {
			glBindAttribLocation(m_handle, l, location_names[l].c_str());
			testGlError("Error while binding attribute");
		}

		glLinkProgram(m_handle);
		testGlError("Error while linking program");

	} catch(...) {
		glDeleteProgram(m_handle);
		throw;
	}
}

Program::~Program() {
	if(m_handle)
		glDeleteProgram(m_handle);
}

string Program::getInfo() const {
	char buf[4096];
	glGetProgramInfoLog(m_handle, 4096, 0, buf);
	return string(buf);
}

ProgramBinder::ProgramBinder(PProgram program) : m_program(std::move(program)) {
	DASSERT(m_program);
}

namespace {

	unsigned s_active_program;
}

void ProgramBinder::bind() const {
	if(s_active_program == handle())
		return;

	s_active_program = handle();
	glUseProgram(handle());
	testGlError("Error while binding");
}

ProgramBinder::~ProgramBinder() {
	if(s_active_program == handle())
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
	glUniform1f(glGetUniformLocation(handle(), name), v);
}

void ProgramBinder::setUniform(const char *name, CRange<float> range) {
	bind();
	glUniform1fv(glGetUniformLocation(handle(), name), range.size(), range.data());
}

void ProgramBinder::setUniform(const char *name, CRange<Matrix4> range) {
	bind();
	glUniformMatrix4fv(glGetUniformLocation(handle(), name), range.size(), false,
					   (float *)range.data());
}

void ProgramBinder::setUniform(const char *name, CRange<float2> range) {
	bind();
	glUniform2fv(glGetUniformLocation(handle(), name), range.size(), (float *)range.data());
}

void ProgramBinder::setUniform(const char *name, CRange<float3> range) {
	bind();
	glUniform3fv(glGetUniformLocation(handle(), name), range.size(), (float *)range.data());
}

void ProgramBinder::setUniform(const char *name, CRange<float4> range) {
	bind();
	glUniform4fv(glGetUniformLocation(handle(), name), range.size(), (float *)range.data());
}

void ProgramBinder::setUniform(const char *name, int v) {
	bind();
	glUniform1i(glGetUniformLocation(handle(), name), v);
}

void ProgramBinder::setUniform(const char *name, const int2 &v) {
	bind();
	glUniform1iv(glGetUniformLocation(handle(), name), 2, &v[0]);
}

void ProgramBinder::setUniform(const char *name, const int3 &v) {
	bind();
	glUniform1iv(glGetUniformLocation(handle(), name), 3, &v[0]);
}

void ProgramBinder::setUniform(const char *name, const int4 &v) {
	bind();
	glUniform1iv(glGetUniformLocation(handle(), name), 4, &v[0]);
}

void ProgramBinder::setUniform(const char *name, const float2 &v) {
	bind();
	glUniform2f(glGetUniformLocation(handle(), name), v[0], v[1]);
}

void ProgramBinder::setUniform(const char *name, const float3 &v) {
	bind();
	glUniform3f(glGetUniformLocation(handle(), name), v[0], v[1], v[2]);
}

void ProgramBinder::setUniform(const char *name, const float4 &v) {
	bind();
	glUniform4f(glGetUniformLocation(handle(), name), v[0], v[1], v[2], v[3]);
}

void ProgramBinder::setUniform(const char *name, const Matrix4 &matrix) {
	bind();
	glUniformMatrix4fv(glGetUniformLocation(handle(), name), 1, 0, &matrix[0][0]);
}

int ProgramBinder::getUniformLocation(const char *name) const {
	return glGetUniformLocation(handle(), name);
}
}
