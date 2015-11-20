/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef FWK_HALF_TET_MESH_H
#define FWK_HALF_TET_MESH_H

#include "fwk_gfx.h"

namespace fwk {

class TetMesh;

class HalfTetMesh {
  public:
	using TriIndices = Mesh::TriIndices;
	class Vertex;
	class Edge;
	class Face;
	class Tet;

	explicit HalfTetMesh(const TetMesh &);
	explicit operator TetMesh() const;
	HalfTetMesh(const HalfTetMesh &rhs);

	bool empty() const { return m_tets.empty(); }

	Vertex *addVertex(const float3 &pos);
	Vertex *findVertex(const float3 &pos);
	Tet *addTet(Vertex *, Vertex *, Vertex *, Vertex *);
	Tet *addTet(CRange<Vertex *, 4>);
	Tet *findTet(Vertex *, Vertex *, Vertex *, Vertex *);
	pair<Face *, Face *> findFaces(Vertex *, Vertex *, Vertex *);
	bool isValid(Vertex *) const;
	bool hasEdge(Vertex *, Vertex *) const;

	void removeVertex(Vertex *vert);
	void removeTet(Tet *tet);
	vector<Tet *> tets();
	vector<Face *> faces();

	// TODO: proxy class for accessing verts / faces / etc
	vector<Vertex *> verts();
	vector<Edge> edges();

	vector<Face *> edgeFaces(Vertex *, Vertex *);
	vector<Face *> edgeBoundaryFaces(Vertex *, Vertex *);
	vector<Face *> extractSelectedFaces(vector<Tet *>);
	bool haveSharedEdge(Face *, Face *) const;
	Edge sharedEdge(Face *, Face *);

	bool isIntersecting(const Tetrahedron &) const;
	bool isIntersecting(const float3 &) const;

	void subdivideEdge(Vertex *e1, Vertex *e2, Vertex *divisor);
	void subdivideEdge(Vertex *e1, Vertex *e2, vector<Vertex *> divisors);

	// Returned tet at index i contains face #i from original tet
	array<Tet *, 4> subdivideTet(Tet *tet, Vertex *vert);

	// TODO: fix constness of some of the methods

	string stats() const;

	// TODO: for now user have to make sure that thare are no tets between
	// some of the merged points which can cause trouble
	Vertex *mergeVerts(CRange<Vertex *>);
	Vertex *mergeVerts(CRange<Vertex *>, const float3 &new_pos);

	class Vertex {
	  public:
		Vertex(float3 pos, int index) : m_pos(pos), m_index(index), m_temp(0) {}
		~Vertex() = default;
		Vertex(const Vertex &) = delete;
		void operator=(const Vertex &) = delete;

		const float3 &pos() const { return m_pos; }
		const auto &faces() { return m_faces; }
		const auto &tets() { return m_tets; }

		int temp() const { return m_temp; }
		void setTemp(int temp) { m_temp = temp; }

		// It might change when verts are removed from HalfTetMesh
		int index() const { return m_index; }

	  private:
		void removeFace(Face *);
		void addFace(Face *);
		void removeTet(Tet *);
		void addTet(Tet *);

		friend class Face;
		friend class HalfTetMesh;
		vector<Face *> m_faces;
		vector<Tet *> m_tets;
		float3 m_pos;
		int m_index, m_temp;
	};

	class Edge {
	  public:
		// TODO: allow invalid edges
		Edge() : a(nullptr), b(nullptr) {}
		Edge(Vertex *a, Vertex *b) : a(a), b(b) { DASSERT(a && b && a != b); }
		bool operator==(const Edge &rhs) const { return (a == rhs.a && b == rhs.b); }

		Edge inverse() const { return Edge(b, a); }
		bool operator<(const Edge &rhs) const { return std::tie(a, b) < std::tie(rhs.a, rhs.b); }
		Segment segment() const { return Segment(a->pos(), b->pos()); }
		bool isBoundary() const;

		Edge ordered() const { return Edge(a < b ? a : b, a > b ? a : b); }
		vector<Face *> faces();

		bool isValid() const { return a && b && a != b; }
		bool hasSharedEnds(const Edge &other) const {
			return isOneOf(a, other.a, other.b) || isOneOf(b, other.a, other.b);
		}

		Vertex *a, *b;
	};

	class Face {
	  public:
		Face(Tet *tet, Vertex *v1, Vertex *v2, Vertex *v3, int index);
		~Face();

		Face(const Face &) = delete;
		void operator=(const Face &) = delete;

		Tet *tet() const { return m_tet; }
		bool isBoundary() const { return m_opposite == nullptr; }
		const auto &verts() { return m_verts; }
		Face *opposite() { return m_opposite; }
		const auto &triangle() const { return m_tri; }
		void setTemp(int temp) { m_temp = temp; }
		int temp() const { return m_temp; }

		pair<int, float> closestEdgeId(const float3 &);
		int edgeId(Edge) const;
		array<Edge, 3> edges() const;

		// If more than one boundary face is adjacent to some edge,
		// then first one is returned
		array<Face *, 3> boundaryNeighbours() const;

		// It might change when faces are removed from HalfMesh
		int index() const { return m_index; }

		vector<Face *> neighbours() const;
		Vertex *otherVert(CRange<Vertex *, 2> edge) const;

	  private:
		// array<Edge, 3> m_edges;
		array<Vertex *, 3> m_verts;
		Tet *m_tet;
		Face *m_opposite;
		Triangle m_tri;
		int m_index, m_temp;
	};

	class Tet {
	  public:
		Tet(Vertex *, Vertex *, Vertex *, Vertex *, int index);
		~Tet();

		Tet(const Tet &) = delete;
		void operator=(const Tet &) = delete;

		bool isBoundary() const;
		array<Face *, 4> faces();
		const auto &verts() { return m_verts; }
		const auto &neighbours() { return m_neighbours; }
		Tetrahedron tet() const;

		void setTemp(int temp) { m_temp = temp; }
		int temp() const { return m_temp; }

	  private:
		friend class HalfTetMesh;

		array<unique_ptr<Face>, 4> m_faces;
		array<Vertex *, 4> m_verts;
		array<Tet *, 4> m_neighbours;
		int m_index, m_temp;
	};

	vector<unique_ptr<Vertex>> m_verts;
	vector<unique_ptr<Tet>> m_tets;
};

#endif
