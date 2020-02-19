// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom/element_ref.h"

#include "fwk/geom/graph.h"

namespace fwk {

// -------------------------------------------------------------------------
// -- VertexRef implementation ---------------------------------------------

const GLabel *VertexRef::operator->() const { return &(*m_graph)[m_id]; }
const GLabel *EdgeRef::operator->() const { return &(*m_graph)[m_id]; }
const GLabel *TriangleRef::operator->() const { return &(*m_graph)[m_id]; }

int VertexRef::numEdges(GLayers layers) const {
	auto &edges = m_graph->m_verts[m_id];
	if(layers == all<GLayer>)
		return edges.size();
	int count = 0;
	for(auto eid : m_graph->m_verts[m_id])
		if(layers & eid.layer())
			count++;
	return count;
}

int VertexRef::numEdgesFrom(GLayers layers) const {
	int count = 0;
	for(auto eid : m_graph->m_verts[m_id])
		if(eid.isSource() && (eid.layer() & layers))
			count++;
	return count;
}

int VertexRef::numEdgesTo(GLayers layers) const {
	int count = 0;
	for(auto eid : m_graph->m_verts[m_id])
		if(!eid.isSource() && (eid.layer() & layers))
			count++;
	return count;
}

GLayers VertexRef::layers() const { return m_graph->m_vert_layers[m_id]; }

EdgeRefs VertexRef::edgesFrom(GLayers layers) const {
	vector<EdgeId> out(pool_alloc);
	for(auto eid : m_graph->m_verts[m_id])
		if(eid.isSource() && eid.test(layers))
			out.emplace_back(eid);
	return {out, m_graph};
}

EdgeRefs VertexRef::edgesTo(GLayers layers) const {
	vector<EdgeId> out(pool_alloc);
	for(auto eid : m_graph->m_verts[m_id])
		if(!eid.isSource() && eid.test(layers))
			out.emplace_back(eid);
	return {out, m_graph};
}

EdgeRefs VertexRef::edges(GLayers layers) const {
	vector<EdgeId> out(pool_alloc); // TODO: keep counts? with layers it makes no sense...
	for(auto eid : m_graph->m_verts[m_id])
		if(eid.test(layers))
			out.emplace_back(eid);
	return {out, m_graph};
}

VertexRefs VertexRef::vertsAdj(GLayers layers) const {
	vector<VertexId> out(pool_alloc);
	for(auto eid : m_graph->m_verts[m_id])
		if(eid.test(layers)) {
			auto &edge = m_graph->m_edges[eid];
			out.emplace_back(edge.from == m_id ? edge.to : edge.from);
		}
	return {out, m_graph};
}

// TODO: what if we have multiple edges between two verts?
VertexRefs VertexRef::vertsFrom(GLayers layers) const {
	vector<VertexId> out(pool_alloc);
	for(auto eid : m_graph->m_verts[m_id])
		if(eid.isSource()) {
			auto vert = m_graph->ref(eid).other(m_id);
			if(layers == all<GLayer> || (vert.layers() & layers))
				out.emplace_back(vert);
		}
	return {out, m_graph};
}

// TODO: what if we have multiple edges between two verts?
VertexRefs VertexRef::vertsTo(GLayers layers) const {
	vector<VertexId> out(pool_alloc);
	for(auto eid : m_graph->m_verts[m_id])
		if(!eid.isSource()) {
			auto vert = m_graph->ref(eid).other(m_id);
			if(layers == all<GLayer> || (vert.layers() & layers))
				out.emplace_back(vert);
		}
	return {out, m_graph};
}

TriRefs VertexRef::tris(GLayers layers) const {
	vector<TriangleId> out(pool_alloc);
	for(auto tid : m_graph->m_vert_tris[m_id])
		if(tid.test(layers))
			out.emplace_back(tid);
	return {out, m_graph};
}

// -------------------------------------------------------------------------
// -- EdgeRef implementation -----------------------------------------------

Pair<VertexRef> EdgeRef::verts() const {
	auto &edge = m_graph->m_edges[m_id];
	return {{m_graph, edge.from}, {m_graph, edge.to}};
}
VertexRef EdgeRef::from() const { return VertexRef(m_graph, m_graph->m_edges[m_id].from); }
VertexRef EdgeRef::to() const { return VertexRef(m_graph, m_graph->m_edges[m_id].to); }
VertexRef EdgeRef::other(VertexId node) const {
	auto &edge = m_graph->m_edges[m_id];
	return VertexRef(m_graph, edge.from == node ? edge.to : edge.from);
}
GLayer EdgeRef::layer() const { return m_graph->m_edge_layers[m_id]; }

Maybe<EdgeRef> EdgeRef::twin(GLayers layers) const {
	return m_graph->findEdge(to(), from(), layers);
}

bool EdgeRef::adjacent(VertexId node_id) const { return isOneOf(node_id, from(), to()); }
bool EdgeRef::adjacent(EdgeId rhs_id) const {
	EdgeRef rhs(m_graph, rhs_id);
	return adjacent(rhs.from()) || adjacent(rhs.to());
}

EdgeRef EdgeRef::prevFrom() const {
	auto &vert_edges = m_graph->m_verts[from()];
	int idx = 0;
	for(; idx < vert_edges.size(); idx++)
		if(vert_edges[idx] == m_id)
			break;
	for(idx--; idx >= 0; idx--)
		if(vert_edges[idx].isSource())
			return {m_graph, vert_edges[idx]};
	for(idx = vert_edges.size() - 1;; idx--)
		if(vert_edges[idx].isSource())
			return {m_graph, vert_edges[idx]};
	// Unreachable
}

EdgeRef EdgeRef::nextFrom() const {
	auto &vert_edges = m_graph->m_verts[from()];
	int idx = 0;
	for(; idx < vert_edges.size(); idx++)
		if(vert_edges[idx] == m_id)
			break;
	for(idx++; idx < vert_edges.size(); idx++)
		if(vert_edges[idx].isSource())
			return {m_graph, vert_edges[idx]};
	for(idx = 0;; idx++)
		if(vert_edges[idx].isSource())
			return {m_graph, vert_edges[idx]};
	// Unreachable
}

EdgeRef EdgeRef::prevTo() const {
	FATAL("write me");
	return *this;
}

EdgeRef EdgeRef::nextTo() const {
	FATAL("write me");
	return *this;
}

// -------------------------------------------------------------------------
// -- TriangleRef implementation -------------------------------------------

array<VertexRef, 3> TriangleRef::verts() const {
	auto &tri = m_graph->m_tris[m_id];
	return {{{m_graph, tri.verts[0]}, {m_graph, tri.verts[1]}, {m_graph, tri.verts[2]}}};
}

VertexRef TriangleRef::vert(int idx) const { return {m_graph, m_graph->m_tris[m_id].verts[idx]}; }
GLayer TriangleRef::layer() const { return m_graph->m_tri_layers[m_id]; }

}
