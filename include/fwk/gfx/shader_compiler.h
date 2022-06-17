// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/gl_shader.h"
#include "fwk/gfx/vulkan_instance.h"
#include "fwk/vector.h"

namespace fwk {

struct ShaderCompilerOptions {
	VulkanVersion version;
};

class ShaderCompiler {
  public:
	ShaderCompiler();
	FWK_MOVABLE_CLASS(ShaderCompiler);

	struct CompilationResult {
		vector<char> bytecode;
		string messages;
	};

	CompilationResult compile(ShaderType, ZStr) const;

  private:
	void *m_compiler = nullptr;
};

}