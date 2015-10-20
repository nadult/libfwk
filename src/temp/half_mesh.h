/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef FWK_HALF_MESH_H
#define FWK_HALF_MESH_H

#include "fwk_gfx.h"

namespace fwk {

class HalfMesh {
  public:
	using TriIndices = Mesh::TriIndices;
	class Vertex;
	class HalfEdge;
	class Face;

	HalfMesh(CRange<float3> positions = CRange<float3>(),
			 CRange<TriIndices> tri_indices = CRange<TriIndices>());
	HalfMesh(const Mesh &mesh) : HalfMesh(mesh.positions(), mesh.trisIndices()) {}

	bool is2ManifoldUnion() const;
	bool is2Manifold() const;

	bool empty() const { return m_faces.empty(); }

	Vertex *addVertex(const float3 &pos);
	Face *addFace(Vertex *a, Vertex *b, Vertex *c);
	vector<HalfEdge *> orderedEdges(Vertex *vert);
	void removeVertex(Vertex *vert);
	void removeFace(Face *face);
	vector<Vertex *> verts();
	vector<Face *> faces();
	vector<HalfEdge *> halfEdges();

	Face *findFace(Vertex *a, Vertex *b, Vertex *c);

	void clearTemps(int value);
	void selectConnected(Vertex *vert);
	HalfMesh extractSelection();

	void draw(Renderer &out, float scale);

	class Vertex {
	  public:
		Vertex(float3 pos, int index);
		~Vertex();
		Vertex(const Vertex &) = delete;
		void operator=(const Vertex &) = delete;

		const float3 &pos() const { return m_pos; }
		HalfEdge *first() { return m_edges.empty() ? nullptr : m_edges.front(); }
		const vector<HalfEdge *> &all() { return m_edges; }
		int degree() const { return (int)m_edges.size(); }

		int temp() const { return m_temp; }
		void setTemp(int temp) { m_temp = temp; }

		// It might change when verts are removed from HalfMesh
		int index() const { return m_index; }

	  private:
		void removeEdge(HalfEdge *edge);
		void addEdge(HalfEdge *edge);

		friend class HalfMesh;
		friend class HalfEdge;
		friend class Face;

		vector<HalfEdge *> m_edges;
		float3 m_pos;
		int m_index, m_temp;
	};

	class HalfEdge {
	  public:
		~HalfEdge();
		HalfEdge(const HalfEdge &) = delete;
		void operator=(const HalfEdge &) = delete;

		Vertex *start() { return m_start; }
		Vertex *end() { return m_end; }

		HalfEdge *opposite() { return m_opposite; }
		HalfEdge *next() { return m_next; }
		HalfEdge *prev() { return m_prev; }

		HalfEdge *prevVert() { return m_prev->m_opposite; }
		HalfEdge *nextVert() { return m_opposite ? m_opposite->m_next : nullptr; }
		Face *face() { return m_face; }

	  private:
		HalfEdge(Vertex *v1, Vertex *v2, HalfEdge *next, HalfEdge *prev, Face *face);

		friend class Face;
		Vertex *m_start, *m_end;
		HalfEdge *m_opposite, *m_next, *m_prev;
		Face *m_face;
	};

	class Face {
	  public:
		Face(Vertex *v1, Vertex *v2, Vertex *v3, int index);

		HalfEdge *halfEdge(int idx) {
			DASSERT(idx >= 0 && idx <= 2);
			return idx == 0 ? &m_he0 : idx == 1 ? &m_he1 : &m_he2;
		}

		array<Vertex *, 3> verts() { return {{m_he0.start(), m_he1.start(), m_he2.start()}}; }
		array<HalfEdge *, 3> halfEdges() { return {{&m_he0, &m_he1, &m_he2}}; }

		const auto &triangle() const { return m_tri; }
		void setTemp(int temp) { m_temp = temp; }
		int temp() const { return m_temp; }

		// It might change when faces are removed from HalfMesh
		int index() const { return m_index; }

	  private:
		friend class HalfMesh;
		HalfEdge m_he0, m_he1, m_he2;
		Triangle m_tri;
		int m_index, m_temp;
	};

	vector<unique_ptr<Vertex>> m_verts;
	vector<unique_ptr<Face>> m_faces;
};
}

#endif
