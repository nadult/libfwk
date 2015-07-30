/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_opengl.h"

namespace fwk {

Shader::Shader(Shader::Type type) : m_is_compiled(false) {
	m_handle = glCreateShader(type == tVertex ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
	testGlError("Error while creating shader");
}

Shader::Shader(Shader &&rhs) : m_handle(0) { *this = std::move(rhs); }

void Shader::operator=(Shader &&rhs) {
	if(m_handle != ~0u)
		glDeleteShader(m_handle);

	name = std::move(rhs.name);
	m_handle = rhs.m_handle;
	rhs.m_handle = 0;
	m_is_compiled = rhs.m_is_compiled;
}

Shader::~Shader() {
	if(m_handle != ~0u) {
		glDeleteShader(m_handle);
	}
}

void Shader::load(Stream &sr) {
	string text;
	name = sr.name();
	text.resize(sr.size());
	if(!text.empty()) {
		sr.loadData(&text[0], text.size());
		text[text.size()] = 0;
	}
	setSource(text);
}

void Shader::save(Stream &sr) const {
	string text = getSource();
	sr.saveData(&text[0], text.size());
}

string Shader::getSource() const {
	GLint length;
	glGetShaderiv(m_handle, GL_SHADER_SOURCE_LENGTH, &length);

	string source((size_t)length, (char)0);
	glGetShaderSource(m_handle, length + 1, 0, &source[0]);
	return source;
}

void Shader::setSource(const string &str) {
	GLint length = (GLint)str.size();
	const char *text = &str[0];

	m_is_compiled = false;
	for(int n = 0; n < length; n++) {
		bool is_valid = (text[n] >= 32 && text[n] <= 125) || text[n] == '\t' || text[n] == '\n';
		if(!is_valid)
			THROW("fuck: %s", str.c_str());
	}
	glShaderSource(m_handle, 1, &text, &length);
	testGlError("Error in glShaderSource");

	compile();
}

void Shader::compile() {
	if(m_is_compiled)
		return;

	glCompileShader(m_handle);
	testGlError("Error in glCompileShader");

	GLint status;
	glGetShaderiv(m_handle, GL_COMPILE_STATUS, &status);
	if(status != GL_TRUE) {
		char buf[4096];
		glGetShaderInfoLog(m_handle, sizeof(buf), 0, buf);
		THROW("Compilation log (%s):\n%s", name.c_str(), buf);
	}

	m_is_compiled = true;
}

Shader::Type Shader::getType() const {
	GLint type;
	glGetShaderiv(m_handle, GL_SHADER_TYPE, &type);
	return type == GL_VERTEX_SHADER ? tVertex : tFragment;
}
}
