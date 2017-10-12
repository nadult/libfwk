// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/shader.h"
#include "fwk_opengl.h"

namespace fwk {

static string loadSource(Stream &stream) {
	string text;
	text.resize(stream.size());
	if(!text.empty()) {
		stream.loadData(&text[0], text.size());
		text[text.size()] = 0;
	}
	return text;
}

Shader::Shader(Type type, Stream &sr, const string &predefined_macros)
	: Shader(type, loadSource(sr), predefined_macros, sr.name()) {}
Shader::Shader(Type type, const string &source, const string &predefined_macros,
			   const string &name) {
	m_id = glCreateShader(type == Type::vertex ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER);
	testGlError("glCreateShader");

	string full_source =
		predefined_macros.empty() ? source : predefined_macros + "\n#line 0\n" + source;

	for(char c : full_source) {
		bool is_valid = (c >= 32 && c <= 125) || c == '\t' || c == '\n';
		if(!is_valid)
			CHECK_FAILED("Invalid character in shader source (ascii: %d)\n", (int)c);
	}

	GLint length = (GLint)full_source.size();
	const char *string = full_source.data();

	glShaderSource(m_id, 1, &string, &length);
	testGlError("glShaderSource");

	glCompileShader(m_id);
	testGlError("Error in glCompileShader");

	GLint status;
	glGetShaderiv(m_id, GL_COMPILE_STATUS, &status);
	testGlError("glGetShaderiv\n");

	if(status != GL_TRUE) {
		char buf[4096];
		glGetShaderInfoLog(m_id, sizeof(buf), 0, buf);
		CHECK_FAILED("Compilation error of '%s':\n%s", name.c_str(), buf);
	}
}

Shader::~Shader() {
	if(m_id)
		glDeleteShader(m_id);
}

Shader::Shader(Shader &&rhs) : m_id(rhs.m_id) { rhs.m_id = 0; }

ShaderType Shader::type() const {
	GLint type;
	glGetShaderiv(m_id, GL_SHADER_TYPE, &type);
	return type == GL_VERTEX_SHADER ? Type::vertex : Type::fragment;
}
}
