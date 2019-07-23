// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom/immutable_graph.h"
#include "fwk/math/direction.h"
#include "fwk/pod_vector.h"

namespace fwk {

// TODO: uporządkowanie funkcji do sortowania po kątach

template <class T> void orderEdges(ImmutableGraph &graph, CSpan<T> points) {
	using Vec = decltype(T() - T());
	PodVector<Vec> vecs(graph.numEdges());
	for(auto eid : graph.edgeIds())
		vecs[eid] = points[graph.to(eid)] - points[graph.from(eid)];

	PodVector<int> quadrant(graph.numEdges());
	PodVector<EdgeId> tedges(graph.numEdges());

	DASSERT(ccwSide(float2(1, 0), float2(0, 1)));
	DASSERT(ccwSide(float2(0, 1), float2(-1, 0)));

	auto orderEdges = [&](Span<EdgeId> edges) {
		int quad_counts[4] = {0, 0, 0, 0};
		for(int i : intRange(edges)) {
			quadrant[i] = fwk::quadrant(vecs[edges[i]]);
			quad_counts[quadrant[i]]++;
		}

		int quad_offsets[4];
		int off = 0;
		for(int q : intRange(4)) {
			quad_offsets[q] = off;
			off += quad_counts[q];
			quad_counts[q] = 0;
		}

		// Bubble sort within a quadrant should be ok
		for(int i : intRange(edges)) {
			auto eid = edges[i];
			int quad = quadrant[i];
			int qcount = quad_counts[quad]++;
			EdgeId *qedges = &tedges[quad_offsets[quad]];

			int off = 0;
			while(off < qcount && ccwSide(vecs[qedges[off]], vecs[eid]))
				off++;

			for(int j = qcount; j > off; j--)
				qedges[j] = qedges[j - 1];
			qedges[off] = eid;
		}

		copy(edges, cspan(begin(tedges), edges.size()));
	};

	for(auto nref : graph.vertexRefs()) {
		auto ninfo = graph.m_vert_info[nref];
		if(ninfo.num_edges_from > 1) {
			EdgeId *base = graph.m_incidence_info.data() + ninfo.first_edge;
			orderEdges(span(base, ninfo.num_edges_from));
		}
	}

	for(auto &vec : vecs)
		vec = -vec;

	for(auto nref : graph.vertexRefs()) {
		auto &ninfo = graph.m_vert_info[nref];
		if(ninfo.num_edges_to > 1) {
			EdgeId *base = graph.m_incidence_info.data() + ninfo.first_edge;
			orderEdges(span(base + ninfo.num_edges_from, ninfo.num_edges_to));
		}
	}
}

}
