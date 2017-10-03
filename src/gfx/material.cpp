// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/material.h"
#include "fwk/gfx/material_set.h"

namespace fwk {

Material::Material(vector<PTexture> textures, IColor color, Flags flags, u16 custom_flags)
	: textures(move(textures)), color(color), custom_flags(custom_flags), flags(flags) {
	for(const auto &tex : textures)
		DASSERT(tex);
}

Material::Material(IColor color, Flags flags, u16 custom_flags)
	: color(color), custom_flags(custom_flags), flags(flags) {}

MaterialSet::MaterialSet(const Material &default_mat, std::map<string, Material> map)
	: m_default(move(default_mat)), m_map(move(map)) {}

MaterialSet::~MaterialSet() = default;

const Material &MaterialSet::operator[](const string &name) const {
	auto it = m_map.find(name);
	return it == m_map.end() ? m_default : it->second;
}

vector<Material> MaterialSet::operator()(CSpan<string> names) const {
	return transform(names, [this](const string &name) { return (*this)[name]; });
}
}
