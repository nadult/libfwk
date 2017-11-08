// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx_base.h"
#include "fwk/sys/immutable_ptr.h"
#include "fwk_vector.h"

namespace fwk {

class Program : public immutable_base<Program> {
  public:
	Program(const Shader &vertex, const Shader &fragment,
			const vector<string> &location_names = {});
	Program(const string &vsh_file_name, const string &fsh_file_name,
			const string &predefined_macros, const vector<string> &location_names = {});
	~Program();

	Program(const Program &) = delete;
	void operator=(const Program &) = delete;

	unsigned id() const { return m_id; }
	string getInfo() const;

  private:
	uint m_id;
};
}
