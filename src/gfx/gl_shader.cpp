// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_shader.h"

#include "fwk/enum_map.h"
#include "fwk/gfx/opengl.h"
#include "fwk/io/file_system.h"
#include "fwk/math/hash.h"
#include "fwk/parse.h"
#include "fwk/sys/expected.h"

namespace fwk {

GL_CLASS_IMPL(GlShader)

static const EnumMap<ShaderType, int> gl_type_map{
	{GL_VERTEX_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER, GL_COMPUTE_SHADER}};

PShader GlShader::compile(Type type, Str source) {
	DASSERT(!source.empty());

	int gl_id = glCreateShader(gl_type_map[type]);
	int obj_id = storage.allocId(gl_id);
	new(&storage.objects[obj_id]) GlShader();
	PShader ref(obj_id);

	ref->m_hash = fwk::hash<u64>(source);

	const char *strings[1] = {source.data()};
	int lengths[1] = {(int)source.size()};

	glShaderSource(gl_id, 1, strings, lengths);
	glCompileShader(gl_id);

	return ref;
}

Ex<PShader> GlShader::compileAndCheck(Type type, Str source) {
	auto result = compile(type, source);
	if(!result->isCompiled())
		return ERROR("Error while compiling shader: '%'\nCompilation log:\n%", source,
					 result->compilationLog());
	return result;
}

ShaderType GlShader::type() const {
	GLint gl_type;
	glGetShaderiv(id(), GL_SHADER_TYPE, &gl_type);
	for(auto type : all<Type>)
		if(gl_type_map[type] == gl_type)
			return type;
	FATAL("Invalid ShaderType");
}

string GlShader::compilationLog() const {
	GLint length = 0;
	auto gl_id = id();
	glGetShaderiv(gl_id, GL_INFO_LOG_LENGTH, &length);

	string text(length, ' ');
	glGetShaderInfoLog(gl_id, text.size(), 0, text.data());
	return text;
}

bool GlShader::isCompiled() const {
	GLint status;
	glGetShaderiv(id(), GL_COMPILE_STATUS, &status);
	return status == GL_TRUE;
}
}
