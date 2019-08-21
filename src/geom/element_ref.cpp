// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom/element_ref.h"

#include "fwk/geom/graph.h"

namespace fwk {

// -------------------------------------------------------------------------
// -- VertexRef implementation ---------------------------------------------

const GraphLabel *VertexRef::operator->() const { return &(*m_graph)[m_id]; }
const GraphLabel *EdgeRef::operator->() const { return &(*m_graph)[m_id]; }

int VertexRef::numEdges(Layers layers) const {
	auto &edges = m_graph->m_verts[m_id];
	if(layers == all<Layer>)
		return edges.size();
	int count = 0;
	for(auto eid : m_graph->m_verts[m_id])
		if(layers & eid.layer())
			count++;
	return count;
}

int VertexRef::numEdgesFrom(Layers layers) const {
	int count = 0;
	for(auto eid : m_graph->m_verts[m_id])
		if(eid.isSource() && (eid.layer() & layers))
			count++;
	return count;
}

int VertexRef::numEdgesTo(Layers layers) const {
	int count = 0;
	for(auto eid : m_graph->m_verts[m_id])
		if(!eid.isSource() && (eid.layer() & layers))
			count++;
	return count;
}

GraphLayers VertexRef::layers() const { return m_graph->m_vert_layers[m_id]; }

PooledEdgeRefs VertexRef::edgesFrom(Layers layers) const {
	PoolVector<EdgeId> out;
	for(auto eid : m_graph->m_verts[m_id])
		if(eid.isSource() && eid.test(layers))
			out.emplace_back(eid);
	return {out, m_graph};
}

PooledEdgeRefs VertexRef::edgesTo(Layers layers) const {
	PoolVector<EdgeId> out;
	for(auto eid : m_graph->m_verts[m_id])
		if(!eid.isSource() && eid.test(layers))
			out.emplace_back(eid);
	return {out, m_graph};
}

PooledEdgeRefs VertexRef::edges(Layers layers) const {
	PoolVector<EdgeId> out; // TODO: keep counts? with layers it makes no sense...
	for(auto eid : m_graph->m_verts[m_id])
		if(eid.test(layers))
			out.emplace_back(eid);
	return {out, m_graph};
}

PooledVertexRefs VertexRef::vertsAdj(Layers layers) const {
	FATAL("write me");
	return {};
}

// TODO: what if we have multiple edges between two verts?
PooledVertexRefs VertexRef::vertsFrom(Layers layers) const {
	PoolVector<VertexId> out;
	for(auto eid : m_graph->m_verts[m_id])
		if(eid.isSource()) {
			auto vert = m_graph->ref(eid).other(m_id);
			if(layers == all<Layer> || (vert.layers() & layers))
				out.emplace_back(vert);
		}
	return {out, m_graph};
}

// TODO: what if we have multiple edges between two verts?
PooledVertexRefs VertexRef::vertsTo(Layers layers) const {
	PoolVector<VertexId> out;
	for(auto eid : m_graph->m_verts[m_id])
		if(eid.isSource()) {
			auto vert = m_graph->ref(eid).other(m_id);
			if(layers == all<Layer> || (vert.layers() & layers))
				out.emplace_back(vert);
		}
	return {out, m_graph};
}

// -------------------------------------------------------------------------
// -- EdgeRef implementation -----------------------------------------------

VertexRef EdgeRef::from() const { return VertexRef(m_graph, m_graph->m_edges[m_id].from); }
VertexRef EdgeRef::to() const { return VertexRef(m_graph, m_graph->m_edges[m_id].to); }
VertexRef EdgeRef::other(VertexId node) const {
	auto &edge = m_graph->m_edges[m_id];
	return VertexRef(m_graph, edge.from == node ? edge.to : edge.from);
}
GraphLayer EdgeRef::layer() const { return m_graph->m_edge_layers[m_id]; }

Maybe<EdgeRef> EdgeRef::twin(GraphLayers layers) const {
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

}
