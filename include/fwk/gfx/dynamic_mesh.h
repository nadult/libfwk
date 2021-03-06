// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/format.h"
#include "fwk/gfx/mesh.h"
#include "fwk/math/segment.h"
#include "fwk/math/triangle.h"

namespace fwk {

// Vertex / Poly indices can have values up to vertexIdCount() / polyIdCount() -1
// Some indices in the middle may be invalid
class DynamicMesh {
  public:
	DynamicMesh(CSpan<float3> verts, CSpan<array<int, 3>> tris, int poly_value = 0);
	DynamicMesh(CSpan<float3> verts, CSpan<vector<int>> polys, int poly_value = 0);
	explicit DynamicMesh(const Mesh &mesh) : DynamicMesh(mesh.positions(), mesh.trisIndices()) {}
	DynamicMesh() : DynamicMesh({}, vector<vector<int>>{}) {}
	explicit operator Mesh() const;

	struct VertexId {
		explicit VertexId(int id = -1) : id(id) {}
		operator int() const { return id; }
		explicit operator bool() const { return isValid(); }
		bool isValid() const { return id >= 0; }
		int id;
	};

	struct PolyId {
		explicit PolyId(int id = -1) : id(id) {}
		operator int() const { return id; }
		explicit operator bool() const { return isValid(); }
		bool isValid() const { return id >= 0; }
		int id;
	};

	struct EdgeId {
		EdgeId() {}
		EdgeId(VertexId a, VertexId b) : a(a), b(b) {}
		bool isValid() const { return a.isValid() && b.isValid() && a != b; }
		explicit operator bool() const { return isValid(); }
		EdgeId inverse() const { return EdgeId(b, a); }
		EdgeId ordered() const { return a < b ? EdgeId(a, b) : EdgeId(b, a); }
		bool operator==(const EdgeId &rhs) const {
			return std::tie(a, b) == std::tie(rhs.a, rhs.b);
		}
		bool operator<(const EdgeId &rhs) const { return std::tie(a, b) < std::tie(rhs.a, rhs.b); }

		bool hasSharedEnds(const EdgeId &other) const {
			return isOneOf(a, other.a, other.b) || isOneOf(b, other.a, other.b);
		}

		VertexId a, b;
	};

	DynamicMesh extract(CSpan<PolyId>) const;
	vector<DynamicMesh> separateSurfaces() const;

	bool isValid(VertexId) const;
	bool isValid(PolyId) const;
	bool isValid(EdgeId) const;

	using Polygon = vector<VertexId>;

	class Simplex {
	  public:
		Simplex() : m_size(0) {}
		Simplex(VertexId vert) : m_size(1) { m_verts[0] = vert; }
		Simplex(EdgeId edge) : m_size(2) {
			m_verts[0] = edge.a;
			m_verts[1] = edge.b;
		}
		Simplex(const array<VertexId, 3> &face) : m_verts(face), m_size(3) {}

		int size() const { return m_size; }
		bool isVertex() const { return m_size == 1; }
		bool isEdge() const { return m_size == 2; }
		bool isFace() const { return m_size == 3; }

		VertexId asVertex() const {
			DASSERT(isVertex());
			return m_verts[0];
		}

		EdgeId asEdge() const {
			DASSERT(isEdge());
			return EdgeId(m_verts[0], m_verts[1]);
		}

		array<VertexId, 3> asFace() const {
			DASSERT(isFace());
			return m_verts;
		}

		bool operator<(const Simplex &rhs) const {
			return std::tie(m_size, m_verts) < std::tie(rhs.m_size, rhs.m_verts);
		}
		bool operator==(const Simplex &rhs) const {
			return std::tie(m_size, m_verts) == std::tie(rhs.m_size, rhs.m_verts);
		}

		VertexId operator[](int id) const {
			DASSERT(id >= 0 && id < m_size);
			return m_verts[id];
		}
		string print(const DynamicMesh &mesh) const;

	  private:
		array<VertexId, 3> m_verts;
		int m_size;
	};

	bool isValid(const Simplex &simplex) const {
		for(int i = 0; i < simplex.size(); i++)
			if(!isValid(simplex[i]))
				return false;
		return true;
	}

	template <class T1, class T2> bool isValid(const Pair<T1, T2> &simplex_pair) const {
		return isValid(simplex_pair.first) && isValid(simplex_pair.second);
	}

	// TODO: we need overlap tests as well
	bool isClosedOrientableSurface(CSpan<PolyId>) const;

	// Basically it means that it is a union of closed orientable surfaces
	bool representsVolume() const;
	int eulerPoincare() const;

	bool isTriangular() const;

	VertexId addVertex(const float3 &pos);
	PolyId addPoly(CSpan<VertexId, 3>, int value = 0);
	PolyId addPoly(VertexId v0, VertexId v1, VertexId v2, int value = 0) {
		return addPoly({v0, v1, v2}, value);
	}

	void remove(VertexId);
	void remove(PolyId);

	static Pair<Simplex> makeSimplexPair(const Simplex &a, const Simplex &b) {
		return a < b ? pair(b, a) : pair(a, b);
	}

	template <class TSimplex> auto nearbyVerts(TSimplex simplex_id, float tolerance) const {
		vector<Pair<Simplex>> out;
		DASSERT(isValid(simplex_id));

		for(auto vert : verts())
			if(!coincident(vert, simplex_id) &&
			   distance(simplex(simplex_id), point(vert)) < tolerance)
				out.emplace_back(makeSimplexPair(simplex_id, vert));
		return out;
	}

	template <class TSimplex> auto nearbyEdges(TSimplex simplex_id, float tolerance) const {
		vector<Pair<Simplex>> out;
		DASSERT(isValid(simplex_id));

		for(auto edge : edges())
			if(!coincident(simplex_id, edge) &&
			   distance(simplex(simplex_id), segment(edge)) < tolerance)
				out.emplace_back(makeSimplexPair(simplex_id, edge));
		return out;
	}

	template <class TSimplex> auto nearbyPairs(TSimplex simplex_id, float tolerance) const {
		DASSERT(isValid(simplex_id));

		vector<Pair<Simplex>> out;
		insertBack(out, nearbyVerts(simplex_id, tolerance));
		insertBack(out, nearbyEdges(simplex_id, tolerance));
		return out;
	}

	static DynamicMesh merge(CSpan<DynamicMesh>);

	VertexId merge(CSpan<VertexId>);
	VertexId merge(CSpan<VertexId>, const float3 &target_pos);

	void split(EdgeId, VertexId);
	void move(VertexId, const float3 &new_pos);

	vector<PolyId> inverse(CSpan<PolyId>) const;
	vector<VertexId> inverse(CSpan<VertexId>) const;

	vector<VertexId> verts() const;
	vector<VertexId> verts(CSpan<PolyId>) const;
	vector<VertexId> verts(PolyId) const;
	array<VertexId, 2> verts(EdgeId) const;

	vector<PolyId> polys() const;
	vector<PolyId> polys(VertexId) const;
	vector<PolyId> polys(EdgeId) const;
	vector<PolyId> coincidentPolys(PolyId) const;

	template <class Filter> vector<PolyId> polys(VertexId vertex, const Filter &filter) const {
		return fwk::filter(polys(vertex), filter);
	}

	template <class Filter> vector<PolyId> polys(EdgeId edge, const Filter &filter) const {
		return fwk::filter(polys(edge), filter);
	}

	bool coincident(VertexId vert1, VertexId vert2) const { return vert1 == vert2; }
	bool coincident(VertexId vert, EdgeId edge) const;
	bool coincident(EdgeId edge1, EdgeId edge2) const;
	bool coincident(VertexId vert, PolyId face) const;
	bool coincident(EdgeId, PolyId) const;
	bool coincident(PolyId, PolyId) const;

	vector<PolyId> selectSurface(PolyId representative) const;

	vector<EdgeId> edges() const;
	vector<EdgeId> edges(PolyId) const;

	EdgeId polyEdge(PolyId face_id, int sub_id) const;
	int polyEdgeIndex(PolyId, EdgeId) const;
	VertexId otherVertex(PolyId, EdgeId) const;

	// All edges starting from current vertex
	vector<EdgeId> edges(VertexId) const;

	float3 point(VertexId id) const {
		DASSERT(isValid(id));
		return m_verts[id];
	}

	FBox box(EdgeId) const;
	Segment3<float> segment(EdgeId) const;
	Triangle3F triangle(PolyId) const;

	float3 simplex(VertexId id) const { return point(id); }
	Segment3<float> simplex(EdgeId id) const { return segment(id); }
	Triangle3F simplex(PolyId id) const { return triangle(id); }

	Projection edgeProjection(EdgeId, PolyId) const;

	template <class Simplex, class MeshSimplex = VertexId>
	auto closestVertex(const Simplex &simplex, MeshSimplex exclude = VertexId()) const {
		VertexId out;
		float min_dist = inf;

		for(auto vert : verts()) {
			if(coincident(exclude, vert))
				continue;
			float dist = distance(simplex, point(vert));
			if(dist < min_dist) {
				out = vert;
				min_dist = dist;
			}
		}

		return out;
	}

	template <class Simplex, class MeshSimplex = EdgeId>
	auto closestEdge(const Simplex &simplex, MeshSimplex exclude = EdgeId()) const {
		EdgeId out;
		float min_dist = inf;

		for(auto edge : edges()) {
			if(coincident(exclude, edge))
				continue;
			if(distance(point(edge.a), point(edge.b)) < epsilon<float>)
				print("Invalid edge: % - % | % %\n", edge.a.id, edge.b.id, point(edge.a),
					  point(edge.b));
			float dist = distance(simplex, segment(edge));
			if(dist < min_dist) {
				out = edge;
				min_dist = dist;
			}
		}

		return out;
	}

	vector<PolyId> triangulate(PolyId);

	// When faces are modified, or divided, their values are propagated
	int value(PolyId) const;
	void setValue(PolyId, int value);

	int polyCount(VertexId) const;
	int polyCount() const { return m_num_polys; }

	int vertexCount() const { return m_num_verts; }
	int vertexCount(PolyId) const;

	int vertexIdCount() const { return (int)m_verts.size(); }
	int polyIdCount() const { return (int)m_polys.size(); }

  private:
	struct Poly {
		vector<int> verts;
		int value;
	};
	vector<float3> m_verts;
	vector<Poly> m_polys;
	vector<vector<int>> m_adjacency;
	vector<int> m_free_verts, m_free_polys;
	int m_num_verts, m_num_polys;
};

float3 closestPoint(const DynamicMesh &, const float3 &point);
}
