// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx_base.h"

namespace fwk {

DEFINE_ENUM(ShaderType, vertex, geometry, fragment, compute);

class Shader {
  public:
	using Type = ShaderType;

	Shader(Type, Stream &, const string &predefined_macros = string());
	Shader(Type, const string &source, const string &predefined_macros = string(),
		   const string &name = string());
	Shader(Shader &&);
	~Shader();

	Shader(const Shader &) = delete;
	void operator=(const Shader &) = delete;

	Type type() const;
	string source() const;
	uint id() const { return m_id; }
	bool isValid() const { return m_id != 0; }

  private:
	uint m_id;
};
}
