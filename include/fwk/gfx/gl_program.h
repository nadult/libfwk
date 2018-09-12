// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/gl_ref.h"

namespace fwk {

class GlProgram {
	GL_CLASS_DECL(GlProgram)
  public:
	static PProgram make(PShader compute);
	static PProgram make(PShader vertex, PShader fragment, CSpan<string> location_names = {});
	static PProgram make(PShader vertex, PShader geom, PShader fragment,
						 CSpan<string> location_names = {});
	static PProgram make(const string &vsh_file_name, const string &fsh_file_name,
						 const string &predefined_macros, CSpan<string> location_names = {});

	string getInfo() const;

  private:
	void set(CSpan<PShader>, CSpan<string> loc_names);
};
}
