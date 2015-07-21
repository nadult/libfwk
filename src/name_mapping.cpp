/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_base.h"
#include <unordered_map>

namespace fwk {

struct NameMapping::Map : public std::unordered_map<string, int> {};

NameMapping::NameMapping(vector<string> names) : m_names(std::move(names)) {}

NameMapping::NameMapping(const NameMapping &rhs) : m_names(rhs.m_names) {
	if(rhs.m_map)
		m_map = make_unique<Map>(*rhs.m_map);
}

NameMapping::~NameMapping() = default;

vector<int> NameMapping::operator()(const vector<string> &names) const {
	if(!m_map) {
		m_map = make_unique<Map>();
		for(int n = 0; n < (int)m_names.size(); n++)
			m_map->emplace(m_names[n], n);
	}

	vector<int> out(names.size(), -1);
	for(int n = 0; n < (int)names.size(); n++) {
		auto it = m_map->find(names[n]);
		if(it != m_map->end())
			out[n] = it->second;
	}

	return out;
}
}
