/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef FWK_DYNAMIC_MESH_CSG_H
#define FWK_DYNAMIC_MESH_CSG_H

#include "fwk_gfx.h"

namespace fwk {


// Vertex / Poly indices can have values up to vertexIdCount() / polyIdCount() -1
// Some indices in the middle may be invalid
class DynamicMesh {
  public:
	DynamicMesh(CRange<float3> verts, CRange<array<uint, 3>> tris, int poly_value = 0);
	DynamicMesh(CRange<float3> verts, CRange<vector<uint>> polys, int poly_value = 0);
	explicit DynamicMesh(const Mesh &mesh) : DynamicMesh(mesh.positions(), mesh.trisIndices()) {}
	DynamicMesh() : DynamicMesh({}, vector<vector<uint>>{}) {}
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

	DynamicMesh extract(CRange<PolyId>) const;
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
		string print(const DynamicMesh &mesh) const {
			TextFormatter out;
			out("(");
			for(int i = 0; i < m_size; i++) {
				auto pt = mesh.point(m_verts[i]);
				out("%f:%f:%f%c", pt.x, pt.y, pt.z, i + 1 == m_size ? ')' : ' ');
			}
			return (string)out.text();
		}

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

	template <class T1, class T2> bool isValid(const pair<T1, T2> &simplex_pair) const {
		return isValid(simplex_pair.first) && isValid(simplex_pair.second);
	}

	// TODO: we need overlap tests as well
	bool isClosedOrientableSurface(CRange<PolyId>) const;

	// Basically it means that it is a union of closed orientable surfaces
	bool representsVolume() const;
	int eulerPoincare() const;

	bool isTriangular() const;

	VertexId addVertex(const float3 &pos);
	PolyId addPoly(CRange<VertexId, 3>, int value = 0);
	PolyId addPoly(VertexId v0, VertexId v1, VertexId v2, int value = 0) {
		return addPoly({v0, v1, v2}, value);
	}

	void remove(VertexId);
	void remove(PolyId);

	static pair<Simplex, Simplex> makeSimplexPair(const Simplex &a, const Simplex &b) {
		return a < b ? make_pair(b, a) : make_pair(a, b);
	}

	template <class TSimplex> auto nearbyVerts(TSimplex simplex_id, float tolerance) const {
		vector<pair<Simplex, Simplex>> out;
		DASSERT(isValid(simplex_id));

		for(auto vert : verts())
			if(!coincident(vert, simplex_id) &&
			   distance(simplex(simplex_id), point(vert)) < tolerance)
				out.emplace_back(makeSimplexPair(simplex_id, vert));
		return out;
	}

	template <class TSimplex> auto nearbyEdges(TSimplex simplex_id, float tolerance) const {
		vector<pair<Simplex, Simplex>> out;
		DASSERT(isValid(simplex_id));

		for(auto edge : edges())
			if(!coincident(simplex_id, edge) &&
			   distance(simplex(simplex_id), segment(edge)) < tolerance)
				out.emplace_back(makeSimplexPair(simplex_id, edge));
		return out;
	}

	template <class TSimplex> auto nearbyPairs(TSimplex simplex_id, float tolerance) const {
		DASSERT(isValid(simplex_id));

		vector<pair<Simplex, Simplex>> out;
		insertBack(out, nearbyVerts(simplex_id, tolerance));
		insertBack(out, nearbyEdges(simplex_id, tolerance));
		return out;
	}

	struct CSGVisualData {
		CSGVisualData() : max_steps(0), phase(0) {}

		vector<pair<Color, vector<Triangle>>> poly_soups;
		vector<pair<Color, vector<Segment>>> segment_groups;
		vector<pair<Color, vector<Segment>>> segment_groups_trans;
		vector<pair<Color, vector<float3>>> point_sets;
		int max_steps, phase;
		enum { max_phases = 6 };
	};

	static DynamicMesh csgDifference(DynamicMesh a, DynamicMesh b, CSGVisualData *data = nullptr);

	static DynamicMesh merge(CRange<DynamicMesh>);

	VertexId merge(CRange<VertexId>);
	VertexId merge(CRange<VertexId>, const float3 &target_pos);

	void split(EdgeId, VertexId);
	void move(VertexId, const float3 &new_pos);

	vector<PolyId> inverse(CRange<PolyId>) const;
	vector<VertexId> inverse(CRange<VertexId>) const;

	vector<VertexId> verts() const;
	vector<VertexId> verts(CRange<PolyId>) const;
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
	Segment segment(EdgeId) const;
	Triangle triangle(PolyId) const;

	float3 simplex(VertexId id) const { return point(id); }
	Segment simplex(EdgeId id) const { return segment(id); }
	Triangle simplex(PolyId id) const { return triangle(id); }

	template <class TSimplex> float sdistance(const Simplex &gsimplex, TSimplex tsimplex) const {
		if(gsimplex.isVertex())
			return distance(point(gsimplex.asVertex()), simplex(tsimplex));
		else if(gsimplex.isEdge())
			return distance(segment(gsimplex.asEdge()), simplex(tsimplex));
		else
			THROW("simplex type not supported");
		return constant::inf;
	}

	float sdistance(const Simplex &a, const Simplex &b) const {
		if(b.isVertex())
			return sdistance(a, b.asVertex());
		else if(b.isEdge())
			return sdistance(a, b.asEdge());
		else
			THROW("simplex type not supported");
		return constant::inf;
	}

	Projection edgeProjection(EdgeId, PolyId) const;

	template <class Simplex, class MeshSimplex = VertexId>
	auto closestVertex(const Simplex &simplex, MeshSimplex exclude = VertexId()) const {
		VertexId out;
		float min_dist = constant::inf;

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
		float min_dist = constant::inf;

		for(auto edge : edges()) {
			if(coincident(exclude, edge))
				continue;
			if(distance(point(edge.a), point(edge.b)) < constant::epsilon)
				xmlPrint("Invalid edge: % - % (%) (%)\n", edge.a.id, edge.b.id, point(edge.a),
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

	using EdgeLoop = vector<pair<PolyId, EdgeId>>;
	pair<EdgeLoop, EdgeLoop> findIntersections(DynamicMesh &, float tolerance);
	void triangulateFaces(EdgeLoop, float tolerance);

	// How these faces will be interpreted depends on the CSG operation
	// So, maybe in some situations it makes no sense to make faces compatible:
	// for example: in case of subtraction: opposite shared faces don't have to be compatible
	// because tets behind them won't be merged
	enum class FaceType { unclassified, inside, outside, shared, shared_opposite };

	// Indexed by PolyId
	vector<FaceType> classifyFaces(const DynamicMesh &mesh2, const EdgeLoop &loop1,
								   const EdgeLoop &loop2) const;

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

#endif
