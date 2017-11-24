// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/hash_map.h"

#include "fwk/gfx/material_set.h"

namespace fwk {

MaterialSet::MaterialSet(const Material &default_mat, HashMap<string, Material> map)
	: m_default(move(default_mat)), m_map(move(map)) {}
MaterialSet::MaterialSet(const Material &default_mat) : m_default(move(default_mat)) {}
FWK_COPYABLE_CLASS_IMPL(MaterialSet);

const Material &MaterialSet::operator[](const string &name) const {
	auto it = m_map.find(name);
	return it == m_map.end() ? m_default : it->second;
}

vector<Material> MaterialSet::operator()(CSpan<string> names) const {
	return transform(names, [this](const string &name) { return (*this)[name]; });
}

const HashMap<string, Material> &MaterialSet::map() const { return m_map; }
}
