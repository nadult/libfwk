/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of Junkers.*/

#include "fwk_gfx.h"
#include "fwk_opengl.h"

namespace fwk {

namespace {

	unsigned s_active_program;
}

Program::Program(const Shader &vertex, const Shader &fragment) :m_is_linked(false) {
	DASSERT(vertex.getType() == Shader::tVertex);
	DASSERT(fragment.getType() == Shader::tFragment);

	m_handle = glCreateProgram();
	testGlError("Error while creating shader program");

	try {
		glAttachShader(m_handle, vertex.getHandle());
		testGlError("Error while attaching vertex shader to program");
		glAttachShader(m_handle, fragment.getHandle());
		testGlError("Error while attaching fragment shader to program");
	} catch(...) {
		glDeleteProgram(m_handle);
		throw;
	}
}

Program::~Program() {
	if(m_handle) {
		if(s_active_program == m_handle)
			s_active_program = 0;
		glDeleteProgram(m_handle);
	}
}

void Program::bind() const {
	DASSERT(m_is_linked);
	if(s_active_program == m_handle)
		return;

	s_active_program = m_handle;
	glUseProgram(m_handle);
	try {
		testGlError("Error while binding");
	} catch(...) {
		printf("%s\n", getInfo().c_str());
		throw;
	}
}

void Program::unbind() {
	if(s_active_program == 0)
		return;
	s_active_program = 0;
	glUseProgram(0);
}

string Program::getInfo() const {
	char buf[4096];
	glGetProgramInfoLog(m_handle, 4096, 0, buf);
	return string(buf);
}

void Program::setUniform(const char *name, float v) {
	bind();
	glUniform1f(glGetUniformLocation(m_handle, name), v);
	//DASSERT(glGetError() == GL_NO_ERROR);
}

void Program::setUniform(const char *name, const float *v, size_t count) {
	bind();
	glUniform1fv(glGetUniformLocation(m_handle, name), count, v);
	//DASSERT(glGetError() == GL_NO_ERROR);
}

void Program::setUniform(const char *name, const Matrix4 *v, size_t count) {
	bind();
	glUniformMatrix4fv(glGetUniformLocation(m_handle, name), count, 0, &v[0][0][0]);
	//DASSERT(glGetError() == GL_NO_ERROR);
}

void Program::setUniform(const char *name, int v) {
	bind();
	glUniform1i(glGetUniformLocation(m_handle, name), v);
	//DASSERT(glGetError() == GL_NO_ERROR);
}

void Program::setUniform(const char *name, const int2 &v) {
	bind();
	glUniform1iv(glGetUniformLocation(m_handle, name), 2, &v[0]);
	//DASSERT(glGetError() == GL_NO_ERROR);
}

void Program::setUniform(const char *name, const int3 &v) {
	bind();
	glUniform1iv(glGetUniformLocation(m_handle, name), 3, &v[0]);
	//DASSERT(glGetError() == GL_NO_ERROR);
}

void Program::setUniform(const char *name, const int4 &v) {
	bind();
	glUniform1iv(glGetUniformLocation(m_handle, name), 4, &v[0]);
	//DASSERT(glGetError() == GL_NO_ERROR);
}

void Program::setUniform(const char *name, const float2 &v) {
	bind();
	glUniform2f(glGetUniformLocation(m_handle, name), v[0], v[1]);
	//DASSERT(glGetError() == GL_NO_ERROR);
}

void Program::setUniform(const char *name, const float3 &v) {
	bind();
	glUniform3f(glGetUniformLocation(m_handle, name), v[0], v[1], v[2]);
	//DASSERT(glGetError() == GL_NO_ERROR);
}

void Program::setUniform(const char *name, const float4 &v) {
	bind();
	glUniform4f(glGetUniformLocation(m_handle, name), v[0], v[1], v[2], v[3]);
	//DASSERT(glGetError() == GL_NO_ERROR);
}

void Program::setUniform(const char *name, const Matrix4 &matrix) {
	bind();
	glUniformMatrix4fv(glGetUniformLocation(m_handle, name), 1, 0, &matrix[0][0]);
	//DASSERT(glGetError() == GL_NO_ERROR);
}
	
int Program::getUniformLocation(const char *name) {
	return glGetUniformLocation(m_handle, name);
}

void Program::bindAttribLocation(const char *name, unsigned loc) {
	glBindAttribLocation(m_handle, loc, name);
	m_is_linked = false;
}

void Program::link() {
	if(m_is_linked)
		return;
	m_is_linked = true;
	glLinkProgram(m_handle);
	testGlError("Error while linking program");
}
}
