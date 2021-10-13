// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/hash_map.h"

#include "fwk/format.h"
#include "fwk/gfx/shader_defs.h"

namespace fwk {

ShaderDefs::Setter::Setter(Str name, ShaderDefs &parent) : name(name), parent(parent) {}
ShaderDefs::Setter::~Setter() {}

void ShaderDefs::Setter::operator=(bool value) {
	if(value)
		parent.set(name, format("#define %\n", name));
	else
		parent.remove(name);
}

ShaderDefs::ShaderDefs() = default;
FWK_COPYABLE_CLASS_IMPL(ShaderDefs);

void ShaderDefs::set(string name, string value) { macros[name] = move(value); }

void ShaderDefs::remove(Str name) { macros.erase(name); }

ShaderDefs ShaderDefs::operator+(Str def) const {
	auto temp = *this;
	temp[def] = true;
	return temp;
}

string ShaderDefs::text() const {
	TextFormatter out;

	auto list = macros.values();
	// TODO: why sort?
	makeSorted(list);

	for(auto &elem : list)
		out << elem;
	return out.text();
}
}
