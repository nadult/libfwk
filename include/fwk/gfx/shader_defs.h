// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/fwd_member.h"
#include "fwk/gfx_base.h"

namespace fwk {

class ShaderDefs {
  public:
	ShaderDefs();
	FWK_COPYABLE_CLASS(ShaderDefs);

	struct Setter {
		Setter(Str name, ShaderDefs &parent);
		~Setter();

		template <class T> void operator=(const T &value) {
			parent.set(name, format("#define % %\n", name, value));
		}
		void operator=(bool value);

		Str name;
		ShaderDefs &parent;
	};

	auto operator[](Str name) { return Setter(name, *this); }
	void set(string, string);
	void remove(Str);

	ShaderDefs operator+(Str additional_def) const;

	operator string() const { return text(); }
	string text() const;

  private:
	FwdMember<HashMap<string, string>> macros;
};
}
