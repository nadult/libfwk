// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "../extern/spirv-reflect/spirv_reflect.h"

extern "C" {
#include "../extern/spirv-reflect/spirv_reflect.c"
}

#include "fwk/gfx/shader_reflection.h"

#include "fwk/enum_map.h"

namespace fwk {

Ex<ShaderReflectionModule> ShaderReflectionModule::create(CSpan<char> bytecode) {
	Dynamic<SpvReflectShaderModule> module(new SpvReflectShaderModule);
	auto result = spvReflectCreateShaderModule(bytecode.size(), bytecode.data(), module.get());
	if(result != SPV_REFLECT_RESULT_SUCCESS)
		return ERROR("spvReflectCreateShaderModule failed with result: %", uint(result));
	return ShaderReflectionModule(move(module));
}

ShaderReflectionModule::ShaderReflectionModule(Dynamic<SpvReflectShaderModule> module)
	: m_module(move(module)) {}
ShaderReflectionModule::~ShaderReflectionModule() { free(); }
ShaderReflectionModule::ShaderReflectionModule(ShaderReflectionModule &&rhs)
	: m_module(move(rhs.m_module)) {}
ShaderReflectionModule &ShaderReflectionModule::operator=(ShaderReflectionModule &&rhs) {
	if(&rhs != this) {
		free();
		swap(m_module, rhs.m_module);
	}
	return *this;
}

void ShaderReflectionModule::free() {
	if(m_module) {
		spvReflectDestroyShaderModule(m_module.get());
		m_module.reset();
	}
}
}