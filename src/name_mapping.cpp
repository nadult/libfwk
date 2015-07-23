/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_base.h"
#include <unordered_map>

namespace fwk {

struct NameMapping::Map : public std::unordered_map<string, int> {
	using std::unordered_map<string, int>::unordered_map;
};

NameMapping::NameMapping(const vector<string> &names) : m_map(make_cow<Map>(names.size())) {
	for(int n = 0; n < (int)names.size(); n++) {
		auto ret = mutate(m_map)->emplace(names[n], n);
		// TODO: think what to do about name duplicates
		if(!ret.second)
			THROW("Error while constructing name mapping");
	}
}

NameMapping::~NameMapping() = default;

int NameMapping::operator()(const string &name) const {
	auto it = m_map->find(name);
	return it == m_map->end() ? -1 : it->second;
}

vector<int> NameMapping::operator()(const vector<string> &names) const {
	vector<int> out;
	out.reserve(names.size());
	for(auto &name : names)
		out.emplace_back((*this)(name));
	return out;
}

vector<string> NameMapping::names() const {
	vector<string> out(size());
	for(const auto &pair : *m_map)
		out[pair.second] = pair.first;
	return out;
}

size_t NameMapping::size() const { return m_map ? m_map->size() : 0; }
}
