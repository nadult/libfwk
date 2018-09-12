// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_program.h"

#include "fwk/gfx/gl_shader.h"
#include "fwk/gfx/opengl.h"
#include "fwk/pod_vector.h"
#include "fwk/sys/stream.h"

namespace fwk {

GL_CLASS_IMPL(GlProgram)

PProgram GlProgram::make(PShader compute) {
	PProgram ref(storage.make());
	DASSERT(compute && compute->type() == ShaderType::compute);

	ref->set({compute}, {});
	return ref;
}

PProgram GlProgram::make(PShader vertex, PShader fragment, CSpan<string> location_names) {
	PProgram ref(storage.make());
	DASSERT(vertex && vertex->type() == ShaderType::vertex);
	DASSERT(fragment && fragment->type() == ShaderType::fragment);

	ref->set({vertex, fragment}, location_names);
	return ref;
}

PProgram GlProgram::make(PShader vertex, PShader geom, PShader fragment,
						 CSpan<string> location_names) {
	PProgram ref(storage.make());
	DASSERT(vertex && vertex->type() == ShaderType::vertex);
	DASSERT(geom && geom->type() == ShaderType::geometry);
	DASSERT(fragment && fragment->type() == ShaderType::fragment);

	ref->set({vertex, geom, fragment}, location_names);
	return ref;
}

void GlProgram::set(CSpan<PShader> shaders, CSpan<string> loc_names) {
	for(auto &shader : shaders)
		glAttachShader(id(), shader.id());
	for(int l = 0; l < loc_names.size(); l++)
		glBindAttribLocation(id(), l, loc_names[l].c_str());

	glLinkProgram(id());
	auto err = glGetError();

	GLint param;
	glGetProgramiv(id(), GL_LINK_STATUS, &param);
	if(param == GL_FALSE) {
		glGetProgramiv(id(), GL_INFO_LOG_LENGTH, &param);
		PodVector<char> buffer(param);
		glGetProgramInfoLog(id(), buffer.size(), 0, buffer.data());
		buffer[buffer.size() - 1] = 0;
		CHECK_FAILED("Error while linking program:\n%s", buffer.data());
	}

	for(auto &shader : shaders)
		glDetachShader(id(), shader.id());
}

static auto loadShader(const string &file_name, const string &predefined_macros, ShaderType type) {
	Loader loader(file_name);
	return GlShader::make(type, loader, predefined_macros);
}

PProgram GlProgram::make(const string &vsh_file_name, const string &fsh_file_name,
						 const string &predefined_macros, CSpan<string> location_names) {
	return make(loadShader(vsh_file_name, predefined_macros, ShaderType::vertex),
				loadShader(fsh_file_name, predefined_macros, ShaderType::fragment), location_names);
}

string GlProgram::getInfo() const {
	char buf[4096];
	glGetProgramInfoLog(id(), 4096, 0, buf);
	return string(buf);
}
}
