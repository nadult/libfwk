/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <algorithm>
#include <unordered_map>

namespace fwk {

Material::Material(PProgram program, Color color, uint flags)
	: m_program(std::move(program)), m_color(color), m_flags(flags) {}

Material::Material(PProgram program, vector<PTexture> textures, Color color, uint flags)
	: m_program(std::move(program)), m_textures(std::move(textures)), m_color(color),
	  m_flags(flags) {
	for(const auto &tex : m_textures)
		DASSERT(tex);
}
Material::Material(PTexture texture, Color color, uint flags) : m_color(color), m_flags(flags) {
	if(texture)
		m_textures.emplace_back(std::move(texture));
}
Material::Material(Color color, uint flags) : m_color(color), m_flags(flags) {}

bool Material::operator<(const Material &rhs) const {
	return std::tie(m_program, m_textures, m_color, m_flags) <
		   std::tie(rhs.m_program, rhs.m_textures, rhs.m_color, rhs.m_flags);
}

MaterialSet::MaterialSet(PMaterial default_mat, std::map<string, PMaterial> map)
	: m_default(std::move(default_mat)), m_map(std::move(map)) {
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
