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

	// Source loaded from file is added at the end of sources list
	static Ex<PShader> load(Type, ZStr file_name, vector<string> sources = {});
	static Ex<PShader> make(Type, vector<string> sources, Str name = {});

	Type type() const;
	CSpan<string> sources() const { return m_sources; }

	u64 hash() const { return m_hash; }
	ZStr name() const { return m_name; }

  private:
	string m_name;
	vector<string> m_sources;
	u64 m_hash;
};
}
