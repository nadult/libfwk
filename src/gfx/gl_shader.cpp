// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_shader.h"

#include "fwk/enum_map.h"
#include "fwk/filesystem.h"
#include "fwk/gfx/opengl.h"
#include "fwk/math/hash.h"
#include "fwk/sys/expected.h"
#include "fwk/sys/on_fail.h"

namespace fwk {

GL_CLASS_IMPL(GlShader)

static const EnumMap<ShaderType, int> gl_type_map{
	{GL_VERTEX_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER, GL_COMPUTE_SHADER}};

// TODO: return Expected
PShader GlShader::load(Type type, ZStr file_name, const string &predefined_macros) {
	return make(type, loadFileString(file_name).get(), predefined_macros, file_name);
}

PShader GlShader::make(Type type, const string &source, const string &predefined_macros,
					   const string &name) {
	int gl_id = glCreateShader(gl_type_map[type]);
	int obj_id = storage.allocId(gl_id);
	new(&storage.objects[obj_id]) GlShader();
	PShader ref(obj_id);

	string full_source =
		predefined_macros.empty() ? source : predefined_macros + "\n#line 0\n" + source;
	ON_FAIL("Shader type: %", type);

	GLint length = (GLint)full_source.size();
	const char *string = full_source.data();

	ref->m_hash = fwk::hash<u64>(full_source);
	ref->m_name = name;

	glShaderSource(gl_id, 1, &string, &length);
	glCompileShader(gl_id);

	GLint status;
	glGetShaderiv(gl_id, GL_COMPILE_STATUS, &status);

	if(status != GL_TRUE) {
		char buf[4096];
		glGetShaderInfoLog(gl_id, sizeof(buf), 0, buf);
		CHECK_FAILED("Compilation error of '%s':\n%s", name.c_str(), buf);
		// TODO: allow shader to be in failed state; rollback isn't compatible with opengl
	}

	return ref;
}

ShaderType GlShader::type() const {
	GLint gl_type;
	glGetShaderiv(id(), GL_SHADER_TYPE, &gl_type);
	for(auto type : all<Type>())
		if(gl_type_map[type] == gl_type)
			return type;
	FATAL("Invalid shader type");
}
}
