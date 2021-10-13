// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/gl_ref.h"
#include "fwk/gfx_base.h"

namespace fwk {

DEFINE_ENUM(ShaderType, vertex, geometry, fragment, compute);

class GlShader {
	GL_CLASS_DECL(GlShader)
  public:
	using Type = ShaderType;

	static PShader compile(Type, Str source);
	static Ex<PShader> compileAndCheck(Type, Str source);

	Type type() const;
	u64 hash() const { return m_hash; }

	bool isCompiled() const;
	string compilationLog() const;

  private:
	u64 m_hash;
};
}
