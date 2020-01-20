// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_shader.h"

#include "fwk/enum_map.h"
#include "fwk/gfx/opengl.h"
#include "fwk/io/file_system.h"
#include "fwk/math/hash.h"
#include "fwk/parse.h"
#include "fwk/sys/expected.h"
#include "fwk/sys/on_fail.h"

namespace fwk {

GL_CLASS_IMPL(GlShader)

static const EnumMap<ShaderType, int> gl_type_map{
	{GL_VERTEX_SHADER, GL_GEOMETRY_SHADER, GL_FRAGMENT_SHADER, GL_COMPUTE_SHADER}};

Ex<PShader> GlShader::load(Type type, ZStr file_name, vector<string> sources) {
	sources.emplace_back(EX_PASS(loadFileString(file_name)));
	return make(type, move(sources), file_name);
}

// Translates source_id numbers into names
//
// Errors on different vendors:
// nvidia: source_id(line_number) : error/warning CXXXX: ...
// intel:  source_id:line_number(character_number): error/warning: ...
// amd:    ???
string annotateErrors(ZStr text, CSpan<Str> source_names) NOEXCEPT {
	auto lines = tokenize(text, '\n');
	auto vendor = gl_info->vendor;

	for(auto &line : lines) {
		int source_id = -1, line_nr = -1, char_pos = -1;
		Str text;

		if(vendor == GlVendor::nvidia) {
			auto pos1 = line.find('(');
			auto pos2 = line.find(") : ");

			if(isOneOf(string::npos, pos1, pos2) || pos1 >= pos2)
				continue;

			line[pos1] = 0;
			line[pos2] = 0;

			source_id = tryFromString<uint>(line.data(), -1);
			line_nr = tryFromString<uint>(line.data() + pos1 + 1, -1);
			text = line.data() + pos2 + 4;
		} else if(vendor == GlVendor::intel) {
			auto pos1 = line.find(':');
			auto pos2 = line.find("(");
			auto pos3 = line.find("): ");

			if(isOneOf(string::npos, pos1, pos2, pos3) || !(pos1 < pos2 && pos2 < pos3))
				continue;

			line[pos1] = 0;
			line[pos2] = 0;
			line[pos3] = 0;

			source_id = tryFromString<uint>(line.data(), -1);
			line_nr = tryFromString<uint>(line.data() + pos1 + 1, -1);
			char_pos = tryFromString<uint>(line.data() + pos2 + 1, -1);

			if(char_pos == -1)
				continue;
			text = line.data() + pos3 + 3;
		} else {
			// TODO: write me
		}

		if(line_nr > 0 && source_names.inRange(source_id)) {
			if(char_pos >= 0)
				line = format("%:%(%): %", source_names[source_id], line_nr, char_pos, text);
			else
				line = format("%:%: %", source_names[source_id], line_nr, text);
		}
	}

	// Merges lines and adds small indent
	TextFormatter fmt;
	for(int n = 0; n < lines.size(); n++)
		fmt(n + 1 != lines.size() ? "  %\n" : "  %", lines[n]);

	return fmt.text();
}

Ex<PShader> GlShader::make(Type type, vector<string> sources, Str name, CSpan<Str> source_names) {
	DASSERT(sources);
	if(source_names)
		DASSERT(source_names.size() == sources.size());

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

		string text(length, ' ');
		glGetShaderInfoLog(gl_id, text.size(), 0, text.data());
		if(source_names)
			text = annotateErrors(text, source_names);
		return ERROR("Error while compiling % shader '%':\n%\n", type, name, text);
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
