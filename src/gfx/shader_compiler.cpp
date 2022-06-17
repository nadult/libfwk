// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/shader_compiler.h"

#include "shaderc/shaderc.h"

namespace fwk {

using CompilationResult = ShaderCompiler::CompilationResult;

ShaderCompiler::ShaderCompiler() { m_compiler = shaderc_compiler_initialize(); }

ShaderCompiler::~ShaderCompiler() {
	if(m_compiler)
		shaderc_compiler_release((shaderc_compiler_t)m_compiler);
}
ShaderCompiler::ShaderCompiler(ShaderCompiler &&rhs) : m_compiler(rhs.m_compiler) {
	rhs.m_compiler = nullptr;
}
ShaderCompiler &ShaderCompiler::operator=(ShaderCompiler &&rhs) {
	if(m_compiler)
		shaderc_compiler_release((shaderc_compiler_t)m_compiler);
	m_compiler = rhs.m_compiler;
	rhs.m_compiler = nullptr;
}

const EnumMap<ShaderType, shaderc_shader_kind> type_map = {{
	{ShaderType::compute, shaderc_compute_shader},
	{ShaderType::vertex, shaderc_vertex_shader},
	{ShaderType::fragment, shaderc_fragment_shader},
	{ShaderType::geometry, shaderc_geometry_shader},
}};

CompilationResult ShaderCompiler::compile(ShaderType type, ZStr code) const {
	CompilationResult out;
	auto result =
		shaderc_compile_into_spv((shaderc_compiler_t)m_compiler, code.c_str(), code.size(),
								 type_map[type], "input.shader", "main", nullptr);
	int num_warnings = shaderc_result_get_num_warnings(result);
	int num_errors = shaderc_result_get_num_errors(result);
	auto status = shaderc_result_get_compilation_status(result);

	if(status == shaderc_compilation_status_success) {
		auto length = shaderc_result_get_length(result);
		auto *bytes = shaderc_result_get_bytes(result);
		out.bytecode.assign(bytes, bytes + length);
	}

	out.messages = shaderc_result_get_error_message(result);
	shaderc_result_release(result);
	return out;
}

}