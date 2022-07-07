// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/shader_compiler.h"

#include "fwk/enum_map.h"
#include "shaderc/shaderc.h"

namespace fwk {

// TODO: making sure that shaderc_shared.dll is available
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
	return *this;
}

using Stage = VShaderStage;
const EnumMap<VShaderStage, shaderc_shader_kind> type_map = {
	{shaderc_vertex_shader, shaderc_tess_control_shader, shaderc_tess_evaluation_shader,
	 shaderc_geometry_shader, shaderc_fragment_shader, shaderc_compute_shader}};

CompilationResult ShaderCompiler::compile(VShaderStage stage, ZStr code) const {
	CompilationResult out;
	auto result =
		shaderc_compile_into_spv((shaderc_compiler_t)m_compiler, code.c_str(), code.size(),
								 type_map[stage], "input.shader", "main", nullptr);
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