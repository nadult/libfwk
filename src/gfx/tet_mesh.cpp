/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <algorithm>
#include <limits>

namespace fwk {

using TriIndices = TetMesh::TriIndices;

namespace {

	class HalfEdge;
	class Face;

	class Vertex {
	  public:
		Vertex(float3 pos, int index) : m_pos(pos), m_index(index) {}
		~Vertex() { DASSERT(m_edges.empty()); }
		Vertex(const Vertex &) = delete;
		void operator=(const Vertex &) = delete;

		const float3 &pos() const { return m_pos; }
		HalfEdge *first() { return m_edges.empty() ? nullptr : m_edges.front(); }
		const vector<HalfEdge *> &all() { return m_edges; }
		int degree() const { return (int)m_edges.size(); }

	  private:
		void removeEdge(HalfEdge *edge) {
			for(int e = 0; e < (int)m_edges.size(); e++)
				if(m_edges[e] == edge) {
					m_edges[e] = m_edges.back();
					m_edges.pop_back();
				}
		}
		void addEdge(HalfEdge *edge) { m_edges.emplace_back(edge); }

		friend class HalfMesh;
		friend class HalfEdge;
		friend class Face;

		vector<HalfEdge *> m_edges;
		float3 m_pos;
		int m_index;
	};

	class HalfEdge {
	  public:
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
		HalfEdge(Vertex *v1, Vertex *v2, HalfEdge *next, HalfEdge *prev, Face *face)
			: m_start(v1), m_end(v2), m_opposite(nullptr), m_next(next), m_prev(prev),
			  m_face(face) {
			DASSERT(v1 && v2 && face);
			m_start->addEdge(this);
			for(auto *end_edge : m_end->all())
				if(end_edge->end() == m_start) {
					m_opposite = end_edge;
					DASSERT(end_edge->m_opposite == nullptr);
					end_edge->m_opposite = this;
					break;
				}
		}
		~HalfEdge() {
			m_start->removeEdge(this);
			if(m_opposite) {
				DASSERT(m_opposite->m_opposite == this);
				m_opposite->m_opposite = nullptr;
			}
		}

		friend class Face;
		Vertex *m_start, *m_end;
		HalfEdge *m_opposite, *m_next, *m_prev;
		Face *m_face;
	};

	class Face {
	  public:
		Face(Vertex *v1, Vertex *v2, Vertex *v3, int index)
			: m_he0(v1, v2, &m_he1, &m_he2, this), m_he1(v2, v3, &m_he2, &m_he0, this),
			  m_he2(v3, v1, &m_he0, &m_he1, this), m_index(index) {
			DASSERT(v1 && v2 && v3);
			m_tri = Triangle(v1->pos(), v2->pos(), v3->pos());
		}

		~Face() {}

		HalfEdge *halfEdge(int idx) {
			DASSERT(idx >= 0 && idx <= 2);
			return idx == 0 ? &m_he0 : idx == 1 ? &m_he1 : &m_he2;
		}
		vector<HalfEdge *> halfEdges() { return {&m_he0, &m_he1, &m_he2}; }

		const auto &triangle() const { return m_tri; }

	  private:
		friend class HalfMesh;
		HalfEdge m_he0, m_he1, m_he2;
		Triangle m_tri;
		int m_index;
	};

	class HalfMesh {
	  public:
		HalfMesh(CRange<float3> positions, CRange<TriIndices> tri_indices) {
			for(auto pos : positions)
				addVertex(pos);

			for(int f = 0; f < tri_indices.size(); f++) {
				const auto &ids = tri_indices[f];
				addFace(m_verts[ids[0]].get(), m_verts[ids[1]].get(), m_verts[ids[2]].get());
			}

			for(auto *hedge : halfEdges())
				if(!hedge->opposite())
					printf("Unpaired half-edge found!\n");
		}

		Vertex *addVertex(const float3 &pos) {
			m_verts.emplace_back(make_unique<Vertex>(pos, (int)m_verts.size()));
			return m_verts.back().get();
		}

		Face *addFace(Vertex *a, Vertex *b, Vertex *c) {
			m_faces.emplace_back(make_unique<Face>(a, b, c, (int)m_faces.size()));
			return m_faces.back().get();
		}

		void removeVertex(Vertex *vert) {
			int index = vert->m_index;
			DASSERT(vert && m_verts[index].get() == vert);

			vector<Face *> to_remove;
			for(auto *vedge : vert->all())
				removeFace(vedge->face());

			m_verts[index] = std::move(m_verts.back());
			m_verts[index]->m_index = index;
			m_verts.pop_back();
		}

		void removeFace(Face *face) {
			int index = face->m_index;
			DASSERT(face && m_faces[index].get() == face);
			m_faces[index] = std::move(m_faces.back());
			m_faces[index]->m_index = index;
			m_faces.pop_back();
		}

		Vertex *findBestVert() {
			Vertex *best = nullptr;
			float best_dot = constant::inf;

			for(auto *vert : verts()) {
				if(vert->degree() == 3)
					return vert;
			}
			/*for(int v = 0; v < (int)verts.size(); v++) {
				int deg = 1;
				auto *sedge = hedge(vertex(v)->hedge), *tedge = nextVert(sedge);
				while(tedge != sedge) {
					tedge = nextVert(tedge);
					deg++;
				}

				if(deg == 3) {
					HalfEdge *rem_edge[3] = {sedge, nextVert(sedge), prevVert(sedge)};
					Face *vfaces[3] = {face(rem_edge[0]), face(rem_edge[1]), face(rem_edge[2])};
					HalfEdge *new_edge[3] = {next(rem_edge[0]), next(rem_edge[1]),
											 next(rem_edge[2])};
					best = v;
				}
			}*/

			return best;
		}

		vector<Vertex *> verts() {
			vector<Vertex *> out;
			for(auto &vert : m_verts)
				out.emplace_back(vert.get());
			return out;
		}
		vector<Face *> faces() {
			vector<Face *> out;
			for(auto &face : m_faces)
				out.emplace_back(face.get());
			return out;
		}
		vector<HalfEdge *> halfEdges() {
			vector<HalfEdge *> out;
			for(auto &face : m_faces)
				insertBack(out, face->halfEdges());
			return out;
		}

		void draw(Renderer &out) {
			vector<float3> lines;

			for(auto *hedge : halfEdges()) {
				float3 line[2] = {hedge->start()->pos(), hedge->end()->pos()};
				float3 center = hedge->face()->triangle().center();

				line[0] = lerp(line[0], center, 0.02);
				line[1] = lerp(line[1], center, 0.02);

				lines.emplace_back(line[0]);
				lines.emplace_back(line[1]);
			}

			out.addLines(lines,
						 make_immutable<Material>(Color::white, Material::flag_ignore_depth));
		}

		vector<unique_ptr<Vertex>> m_verts;
		vector<unique_ptr<Face>> m_faces;
	};
}

TetMesh::TetMesh(vector<float3> positions, vector<TetIndices> tets)
	: m_positions(std::move(positions)), m_indices(std::move(tets)) {
	for(const auto &tet : m_indices)
		for(auto i : tet)
			DASSERT(i >= 0 && i < (int)m_positions.size());
}

TetMesh TetMesh::make(CRange<float3> positions, CRange<TriIndices> tri_indices, int max_steps,
					  Renderer &renderer) {
	vector<float3> verts;
	vector<TetIndices> tets;

	HalfMesh mesh(positions, tri_indices);

	for(int s = 0; s < max_steps; s++) {
		Vertex *best = mesh.findBestVert();

		if(best) {
			mesh.removeVertex(best);
		}
	}

	mesh.draw(renderer);

	return TetMesh(std::move(verts), std::move(tets));
}
}
