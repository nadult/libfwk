// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom/immutable_graph.h"
#include "fwk/geom/order_edges.h"
#include "fwk/sys/assert.h"

namespace fwk {

template <class T, EnableIfVec<T, 2>...>
vector<vector<EdgeId>> planarLoops(const ImmutableGraph &graph, CSpan<T> points) {
	if(!graph.isUndirected()) {
		// TODO: not very optimal...
		auto und_graph = graph.asUndirected();
		orderEdges<T>(und_graph, points);
		und_graph.computeExtendedInfo();

		auto out = planarLoops(und_graph, points);
		vector<EdgeId> mapping;
		mapping.reserve(und_graph.numEdges() - graph.numEdges());
		for(EdgeId eid : indexRange<EdgeId>(graph.numEdges()))
			if(!graph.ref(eid).twin())
				mapping.emplace_back(eid);
		for(auto &loop : out) {
			for(auto &eid : loop)
				if(eid >= graph.numEdges())
					eid = mapping[eid - graph.numEdges()];
		}
		return out;
	}

	vector<vector<EdgeId>> out;
	vector<bool> visited(graph.numEdges(), false);

	int num_degenerate = 0;
	for(auto eref : graph.edgeRefs())
		if(eref.from() == eref.to()) {
			visited[eref] = true;
			num_degenerate++;
		}

	vector<EdgeId> loop;
	loop.reserve(graph.numEdges());

	for(auto eref : graph.edgeRefs()) {
		if(visited[eref])
			continue;
		VertexId start = eref.from();
		loop.clear();

		auto cur = eref;
		while(!visited[cur]) {
			visited[cur] = true;
			loop.emplace_back(cur.id());
			cur = cur.twin()->prevFrom();
		}

		out.emplace_back(loop);
	}

	int edge_sum = 0;
	for(auto &loop : out) {
		DASSERT(loop.size() >= 2);
		DASSERT(graph.ref(loop.back()).to() == graph.ref(loop.front()).from());
		edge_sum += loop.size();
	}
	DASSERT_EQ(edge_sum, graph.numEdges() - num_degenerate);
	// TODO: if not, then return invalid computation (mabe do some more checks as well)

	return out;
}

}
