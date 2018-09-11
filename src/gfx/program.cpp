// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/program.h"

#include "fwk/gfx/opengl.h"
#include "fwk/gfx/shader.h"
#include "fwk/pod_vector.h"
#include "fwk/sys/stream.h"

namespace fwk {

Program::Program(const Shader &compute) {
	PASSERT_GFX_THREAD();
	DASSERT(compute.type() == ShaderType::compute && compute.isValid());

	m_id = glCreateProgram();
	testGlError("Error while creating shader program");
	{
		glAttachShader(m_id, compute.id());
		testGlError("Error while attaching vertex shader to program");
		link();
	}

	// TODO: finally:
	// glDeleteProgram(m_id);
}

Program::Program(const Shader &vertex, const Shader &fragment,
				 const vector<string> &location_names) {
	PASSERT_GFX_THREAD();
	DASSERT(vertex.type() == ShaderType::vertex && vertex.isValid());
	DASSERT(fragment.type() == ShaderType::fragment && fragment.isValid());

	m_id = glCreateProgram();
	testGlError("Error while creating shader program");
	{
		glAttachShader(m_id, vertex.id());
		testGlError("Error while attaching vertex shader to program");
		glAttachShader(m_id, fragment.id());
		testGlError("Error while attaching fragment shader to program");

		for(int l = 0; l < location_names.size(); l++) {
			glBindAttribLocation(m_id, l, location_names[l].c_str());
			testGlError("Error while binding attribute");
		}

		link();

		glDetachShader(m_id, vertex.id());
		glDetachShader(m_id, fragment.id());
	}

	// TODO: finally:
	// glDeleteProgram(m_id);
}

Program::Program(const Shader &vertex, const Shader &geom, const Shader &fragment,
				 const vector<string> &location_names) {
	PASSERT_GFX_THREAD();
	DASSERT(vertex.type() == ShaderType::vertex && vertex.isValid());
	DASSERT(geom.type() == ShaderType::geometry && geom.isValid());
	DASSERT(fragment.type() == ShaderType::fragment && fragment.isValid());

	m_id = glCreateProgram();
	testGlError("Error while creating shader program");
	{
		glAttachShader(m_id, vertex.id());
		testGlError("Error while attaching vertex shader to program");
		glAttachShader(m_id, geom.id());
		testGlError("Error while attaching geometry shader to program");
		glAttachShader(m_id, fragment.id());
		testGlError("Error while attaching fragment shader to program");

		for(int l = 0; l < location_names.size(); l++) {
			glBindAttribLocation(m_id, l, location_names[l].c_str());
			testGlError("Error while binding attribute");
		}

		link();

		glDetachShader(m_id, vertex.id());
		glDetachShader(m_id, geom.id());
		glDetachShader(m_id, fragment.id());
	}

	// TODO: finally:
	// glDeleteProgram(m_id);
}

void Program::link() {
	glLinkProgram(m_id);
	auto err = glGetError();

	GLint param;
	glGetProgramiv(m_id, GL_LINK_STATUS, &param);
	if(param == GL_FALSE) {
		glGetProgramiv(m_id, GL_INFO_LOG_LENGTH, &param);
		PodVector<char> buffer(param);
		glGetProgramInfoLog(m_id, buffer.size(), 0, buffer.data());
		buffer[buffer.size() - 1] = 0;
		CHECK_FAILED("Error while linking program:\n%s", buffer.data());
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
	PASSERT_GFX_THREAD();
	if(m_id)
		glDeleteProgram(m_id);
}

string Program::getInfo() const {
	char buf[4096];
	glGetProgramInfoLog(m_id, 4096, 0, buf);
	return string(buf);
}
}
