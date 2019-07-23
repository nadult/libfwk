// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom/plane_graph.h"
#include "fwk/geom/plane_graph_builder.h"

namespace fwk {

template <class T> PlaneGraphBuilder<T>::~PlaneGraphBuilder() = default;

template <class T>
PlaneGraphBuilder<T>::PlaneGraphBuilder(vector<T> tpoints) : m_points(move(tpoints)) {
	if constexpr(!is_rational)
		m_node_map.reserve(m_points.size() * 2);

	for(auto id : indexRange<VertexId>(m_points)) {
		auto it = m_node_map.find(m_points[id]);
		if(it == m_node_map.end())
			m_node_map.emplace(m_points[id], id);
	}
}

template <class T> pair<VertexId, bool> PlaneGraphBuilder<T>::emplace(const T &point) {
	auto ret = m_node_map.emplace(point, m_points.size());
	if(ret.second)
		m_points.emplace_back(point);
	return {VertexId(ret.first->second), ret.second};
}

template <class T> pair<EdgeId, bool> PlaneGraphBuilder<T>::emplace(VertexId from, VertexId to) {
	auto ret = m_edge_map.emplace({from, to}, m_edges.size());
	if(ret.second)
		m_edges.emplace_back(from, to);
	return {EdgeId(ret.first->second), ret.second};
}

template <class T> VertexId PlaneGraphBuilder<T>::operator()(const T &point) {
	return emplace(point).first;
}

template <class T> EdgeId PlaneGraphBuilder<T>::operator()(VertexId from, VertexId to) {
	return emplace(from, to).first;
}

template <class T> EdgeId PlaneGraphBuilder<T>::operator()(const T &p1, const T &p2) {
	auto n1 = (*this)(p1);
	auto n2 = (*this)(p2);
	return (*this)(n1, n2);
}

template <class T> void PlaneGraphBuilder<T>::reservePoints(int count) {
	m_points.reserve(count);
	if constexpr(!is_rational)
		m_node_map.reserve(count * 2);
}

template <class T> void PlaneGraphBuilder<T>::reserveEdges(int count) {
	m_edges.reserve(count);
	if constexpr(!is_rational)
		m_edge_map.reserve(count * 2);
}

template <class T> vector<T> PlaneGraphBuilder<T>::extractPoints() {
	m_node_map.clear();
	return move(m_points);
}

template <class T> vector<Pair<VertexId>> PlaneGraphBuilder<T>::extractEdges() {
	m_edge_map.clear();
	return move(m_edges);
}

template <class T> void PlaneGraphBuilder<T>::clear() {
	m_points.clear();
	m_edges.clear();
	m_node_map.clear();
	m_edge_map.clear();
}

template <class T> Maybe<VertexId> PlaneGraphBuilder<T>::find(const T &point) const {
	auto it = m_node_map.find(point);
	return it == m_node_map.end() ? Maybe<VertexId>() : VertexId(it->second);
}

template <class T> Maybe<EdgeId> PlaneGraphBuilder<T>::find(VertexId from, VertexId to) const {
	auto it = m_edge_map.find(Pair<int>(from, to));
	return it == m_edge_map.end() ? Maybe<EdgeId>() : EdgeId(it->second);
}

template <class T> Maybe<EdgeId> PlaneGraphBuilder<T>::find(const T &from, const T &to) const {
	if(auto n1 = find(from))
		if(auto n2 = find(to))
			return find(*n1, *n2);
	return none;
}

template <class T>
VectorMap<VertexId, VertexId> PlaneGraphBuilder<T>::mapping(const vector<T> &source_points) const {
	vector<Pair<VertexId>> out;
	out.reserve(source_points.size());
	for(auto id : indexRange<VertexId>(source_points))
		if(auto target = find(source_points[id]))
			out.emplace_back(id, *target);
	return out;
}

template class PlaneGraphBuilder<float2>;
template class PlaneGraphBuilder<double2>;
template class PlaneGraphBuilder<int2>;
template class PlaneGraphBuilder<short2>;

template class PlaneGraphBuilder<Rational2<llint>>;

template class PlaneGraphBuilder<int3>;
template class PlaneGraphBuilder<float3>;
template class PlaneGraphBuilder<double3>;
}
