// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/dynamic.h"
#include "fwk/sys/expected.h"
#include "fwk/vulkan_base.h"

#include "../extern/spirv-reflect/spirv_reflect.h"

// TODO: is this additional layer really needed?
namespace fwk {
class ShaderReflectionModule {
  public:
	static Ex<ShaderReflectionModule> create(CSpan<char> spirv_bytecode);
	FWK_MOVABLE_CLASS(ShaderReflectionModule);

	SpvReflectShaderModule &operator*() { return *m_module.get(); }
	SpvReflectShaderModule *operator->() { return m_module.get(); }

  private:
	ShaderReflectionModule(Dynamic<SpvReflectShaderModule>);
	void free();

	Dynamic<SpvReflectShaderModule> m_module;
};

}