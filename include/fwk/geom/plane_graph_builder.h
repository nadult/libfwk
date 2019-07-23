// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom_base.h"
#include "fwk/hash_map.h"
#include "fwk/vector_map.h"
#include <map>

namespace fwk {

using NodeMapping = VectorMap<VertexId, VertexId>;

template <class T> class PlaneGraphBuilder {
  public:
	static_assert(is_vec<T>);
	static constexpr bool is_rational = fwk::is_rational<T>;

	PlaneGraphBuilder() = default;
	PlaneGraphBuilder(vector<T> tpoints);
	~PlaneGraphBuilder();

	bool valid(VertexId id) const { return id >= 0 && id < m_points.size(); }
	bool valid(EdgeId id) const { return id >= 0 && id < m_edges.size(); }

	void reservePoints(int);
	void reserveEdges(int);

	int numVerts() const { return m_points.size(); }
	int numEdges() const { return m_edges.size(); }

	const T &operator[](VertexId node_id) const {
		DASSERT(valid(node_id));
		return m_points[node_id];
	}
	const Pair<VertexId> &operator[](EdgeId edge_id) const {
		DASSERT(valid(edge_id));
		return m_edges[edge_id];
	}

	const auto &points() const { return m_points; }
	const auto &edges() const { return m_edges; }

	// TODO: better name
	vector<T> extractPoints();
	vector<Pair<VertexId>> extractEdges();
	void clear();

	template <class U = T, EnableIfVec<U, 2>...> PGraph<T> build() {
		return {extractEdges(), extractPoints()};
	}

	// bool: true if new element was added
	Pair<VertexId, bool> emplace(const T &point);
	Pair<EdgeId, bool> emplace(VertexId from, VertexId to);

	VertexId operator()(const T &point);

	template <class U = T, EnableIf<!fwk::is_rational<U>>...>
	EdgeId operator()(const Segment<T> &seg) {
		return (*this)(seg.from, seg.to);
	}

	EdgeId operator()(const T &from, const T &to);
	EdgeId operator()(VertexId from, VertexId to);

	Maybe<VertexId> find(const T &) const;
	Maybe<EdgeId> find(VertexId, VertexId) const;
	Maybe<EdgeId> find(const T &, const T &) const;

	VectorMap<VertexId, VertexId> mapping(const vector<T> &source_points) const;

  private:
	// TODO: strukturka łącząca wektor i unordered_mapę (tylko z dodawaniem?)
	// coś jak UniqueVector z LLVM
	vector<T> m_points;
	vector<Pair<VertexId>> m_edges;
	using NodeMap = If<is_rational, std::map<T, int>, HashMap<T, int>>;
	using EdgeMap = HashMap<Pair<int>, int>;

	NodeMap m_node_map;
	EdgeMap m_edge_map;
};

template <class T, EnableIfVec<T>...> using PointSet = vector<T>;

template <class T, EnableIfVec<T>...> vector<VertexId> mapPoints(CSpan<T>, CSpan<T>);

template <class T> ImmutableGraph remapGraph(vector<VertexId>);
}
