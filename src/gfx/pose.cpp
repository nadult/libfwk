// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/pose.h"

#include "fwk/gfx/draw_call.h"
#include "fwk/gfx/material.h"
#include "fwk/gfx/material_set.h"
#include "fwk/gfx/render_list.h"
#include <map>

namespace fwk {

static auto makeNameMap(const vector<string> &names) {
	vector<pair<string, int>> out(names.size());
	for(int n = 0; n < (int)names.size(); n++)
		out[n] = make_pair(names[n], n);
	std::sort(begin(out), end(out));
	return out;
}

Pose::Pose(vector<Matrix4> transforms, NameMap name_map)
	: m_name_map(move(name_map)), m_transforms(move(transforms)) {
	DASSERT(m_transforms.size() == m_name_map.size());
}
Pose::Pose(vector<Matrix4> transforms, const vector<string> &names)
	: Pose(move(transforms), makeNameMap(names)) {}

vector<int> Pose::mapNames(const vector<string> &names) const {
	auto dst_map = makeNameMap(names);
	int src_index = 0;
	vector<int> out(names.size());

	for(int n = 0; n < (int)dst_map.size(); n++) {
		const string &name = dst_map[n].first;
		while(m_name_map[src_index].first != name) {
			src_index++;
			if(src_index == (int)m_name_map.size())
				FATAL("Cannot find node in pose: %s", name.c_str());
		}

		out[dst_map[n].second] = m_name_map[src_index++].second;
	}
	return out;
}

vector<Matrix4> Pose::mapTransforms(const vector<int> &mapping) const {
	vector<Matrix4> out;
	out.reserve(mapping.size());
	for(auto it : mapping) {
		DASSERT(it >= 0 && it < (int)m_transforms.size());
		out.emplace_back(m_transforms[it]);
	}
	return out;
}
}
