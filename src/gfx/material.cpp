// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <algorithm>
#include <unordered_map>

namespace fwk {

Material::Material(vector<PTexture> textures, FColor color, uint flags)
	: m_textures(move(textures)), m_color(color), m_flags(flags) {
	for(const auto &tex : m_textures)
		DASSERT(tex);
}

Material::Material(PTexture texture, FColor color, uint flags) : m_color(color), m_flags(flags) {
	if(texture)
		m_textures.emplace_back(move(texture));
}
Material::Material(FColor color, uint flags) : m_color(color), m_flags(flags) {}

bool Material::operator<(const Material &rhs) const {
	return std::tie(m_textures, m_color, m_flags) <
		   std::tie(rhs.m_textures, rhs.m_color, rhs.m_flags);
}

MaterialSet::MaterialSet(PMaterial default_mat, std::map<string, PMaterial> map)
	: m_default(move(default_mat)), m_map(move(map)) {
	DASSERT(m_default);
}

MaterialSet::~MaterialSet() = default;

PMaterial MaterialSet::operator[](const string &name) const {
	auto it = m_map.find(name);
	return it == m_map.end() ? m_default : it->second;
}

vector<PMaterial> MaterialSet::operator()(const vector<string> &names) const {
	vector<PMaterial> out;
	for(const auto &name : names)
		out.emplace_back((*this)[name]);
	return out;
}
}
