// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_program.h"

#include "fwk/format.h"
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

vector<char> GlProgram::getBinary() const {
	int size = 0;
	glGetProgramiv(id(), GL_PROGRAM_BINARY_LENGTH, &size);
	vector<char> out(size);
	GLenum format;
	glGetProgramBinary(id(), size, nullptr, &format, out.data());
	return out;
}

static const EnumMap<ProgramBindingType, int> binding_type_map = {
	GL_SHADER_STORAGE_BLOCK,
	GL_UNIFORM_BLOCK,
	GL_ATOMIC_COUNTER_BUFFER,
	GL_TRANSFORM_FEEDBACK_BUFFER,
};

vector<pair<string, int>> GlProgram::getBindings(ProgramBindingType type) const {
	int num = 0, max_name = 0;
	auto type_id = binding_type_map[type];

	glGetProgramInterfaceiv(id(), type_id, GL_ACTIVE_RESOURCES, &num);
	glGetProgramInterfaceiv(id(), type_id, GL_MAX_NAME_LENGTH, &max_name);

	vector<pair<string, int>> out;
	out.reserve(num);

	vector<char> buffer(max_name + 1);
	for(int n = 0; n < num; n++) {
		GLenum props[1] = {GL_BUFFER_BINDING};
		int values[1] = {0};
		glGetProgramResourceiv(id(), GL_SHADER_STORAGE_BLOCK, n, 1, props, 1, nullptr, values);

		int length = 0;
		glGetProgramResourceName(id(), GL_SHADER_STORAGE_BLOCK, n, buffer.size(), &length,
								 buffer.data());
		out.emplace_back(string(buffer.data(), (size_t)length), values[0]);
	}

	return out;
}
}
