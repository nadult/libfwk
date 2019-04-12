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

	static PShader load(Type, ZStr file_name, const string &predefined_macros = string());
	static PShader make(Type, const string &source, const string &predefined_macros = {},
						const string &name = {});

	Type type() const;
	string source() const;

	u64 hash() const { return m_hash; }
	ZStr name() const { return m_name; }

  private:
	string m_name;
	u64 m_hash;
};
}
