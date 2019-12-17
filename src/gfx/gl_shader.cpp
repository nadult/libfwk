// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_shader.h"

#include "fwk/enum_map.h"
#include "fwk/gfx/opengl.h"
#include "fwk/math/hash.h"
#include "fwk/sys/expected.h"
#include "fwk/sys/file_system.h"
#include "fwk/sys/on_fail.h"

namespace fwk {

GL_CLASS_IMPL(GlShader)

static const EnumMap<ShaderType, int> gl_type_map{
	{GL_VERTEX_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER, GL_COMPUTE_SHADER}};

Ex<PShader> GlShader::load(Type type, ZStr file_name, vector<string> sources) {
	sources.emplace_back(EXPECT_PASS(loadFileString(file_name)));
	return make(type, move(sources), file_name);
}

Ex<PShader> GlShader::make(Type type, vector<string> sources, Str name) {
	DASSERT(sources);

	int gl_id = glCreateShader(gl_type_map[type]);
	int obj_id = storage.allocId(gl_id);
	new(&storage.objects[obj_id]) GlShader();
	PShader ref(obj_id);

	ref->m_hash = fwk::hash<u64>(sources);
	ref->m_name = name;
	ref->m_sources = sources;

	for(int n = 1; n < sources.size(); n++)
		sources[n] = format("#line 1 %\n", n) + sources[n];

	auto lengths = transform(sources, [](Str text) { return GLint(text.size()); });
	auto strings = transform(sources, [](Str text) { return text.data(); });

	glShaderSource(gl_id, strings.size(), strings.data(), lengths.data());
	glCompileShader(gl_id);

	GLint status;
	glGetShaderiv(gl_id, GL_COMPILE_STATUS, &status);

	if(status != GL_TRUE) {
		GLint length = 0;
		glGetShaderiv(gl_id, GL_INFO_LOG_LENGTH, &length);

		vector<char> buffer(length);
		glGetShaderInfoLog(gl_id, buffer.size(), 0, buffer.data());
		return ERROR("Compilation error of '%' (type: %):\n%", name, type, buffer.data());
	}

	return ref;
}

ShaderType GlShader::type() const {
	GLint gl_type;
	glGetShaderiv(id(), GL_SHADER_TYPE, &gl_type);
	for(auto type : all<Type>)
		if(gl_type_map[type] == gl_type)
			return type;
	FATAL("Invalid ShaderType");
}
}
