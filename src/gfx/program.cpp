// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/program.h"
#include "fwk/gfx/shader.h"
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
}
