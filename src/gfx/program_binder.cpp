// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/program_binder.h"
#include "fwk_opengl.h"

namespace fwk {

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

void ProgramBinder::setUniform(const char *name, CSpan<float> range) {
	bind();
	glUniform1fv(glGetUniformLocation(id(), name), range.size(), range.data());
}

void ProgramBinder::setUniform(const char *name, CSpan<Matrix4> range) {
	bind();
	glUniformMatrix4fv(glGetUniformLocation(id(), name), range.size(), false,
					   (float *)range.data());
}

void ProgramBinder::setUniform(const char *name, CSpan<float2> range) {
	bind();
	glUniform2fv(glGetUniformLocation(id(), name), range.size(), (float *)range.data());
}

void ProgramBinder::setUniform(const char *name, CSpan<float3> range) {
	bind();
	glUniform3fv(glGetUniformLocation(id(), name), range.size(), (float *)range.data());
}

void ProgramBinder::setUniform(const char *name, CSpan<float4> range) {
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
