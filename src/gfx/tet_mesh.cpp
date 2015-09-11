/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include "fwk_profile.h"
#include <algorithm>
#include <limits>

namespace fwk {

using TriIndices = TetMesh::TriIndices;

namespace {

	class HalfEdge;
	class Face;

	class Vertex {
	  public:
		Vertex(float3 pos, int index) : m_pos(pos), m_index(index), m_temp(0) {}
		~Vertex() { DASSERT(m_edges.empty()); }
		Vertex(const Vertex &) = delete;
		void operator=(const Vertex &) = delete;

		const float3 &pos() const { return m_pos; }
		HalfEdge *first() { return m_edges.empty() ? nullptr : m_edges.front(); }
		const vector<HalfEdge *> &all() { return m_edges; }
		int degree() const { return (int)m_edges.size(); }

		int temp() const { return m_temp; }
		void setTemp(int temp) { m_temp = temp; }

	  private:
		void removeEdge(HalfEdge *edge) {
			for(int e = 0; e < (int)m_edges.size(); e++)
				if(m_edges[e] == edge) {
					m_edges[e] = m_edges.back();
					m_edges.pop_back();
					break;
				}
		}
		void addEdge(HalfEdge *edge) { m_edges.emplace_back(edge); }

		friend class HalfMesh;
		friend class HalfEdge;
		friend class Face;

		vector<HalfEdge *> m_edges;
		float3 m_pos;
		int m_index, m_temp;
	};

	class HalfEdge {
	  public:
		~HalfEdge() {
			m_start->removeEdge(this);
			if(m_opposite) {
				DASSERT(m_opposite->m_opposite == this);
				m_opposite->m_opposite = nullptr;
			}
		}
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
			DASSERT(v1 && v2 && v1 != v2 && face);
			m_start->addEdge(this);
			for(auto *end_edge : m_end->all())
				if(end_edge->end() == m_start) {
					m_opposite = end_edge;
					DASSERT(end_edge->m_opposite == nullptr);
					end_edge->m_opposite = this;
					break;
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
			  m_he2(v3, v1, &m_he0, &m_he1, this), m_index(index), m_temp(0) {
			DASSERT(v1 && v2 && v3);
			DASSERT(v1 != v2 && v2 != v3 && v3 != v1);
			m_tri = Triangle(v1->pos(), v2->pos(), v3->pos());
		}

		~Face() {}

		HalfEdge *halfEdge(int idx) {
			DASSERT(idx >= 0 && idx <= 2);
			return idx == 0 ? &m_he0 : idx == 1 ? &m_he1 : &m_he2;
		}

		array<Vertex *, 3> verts() { return {{m_he0.start(), m_he1.start(), m_he2.start()}}; }
		array<HalfEdge *, 3> halfEdges() { return {{&m_he0, &m_he1, &m_he2}}; }

		const auto &triangle() const { return m_tri; }
		void setTemp(int temp) { m_temp = temp; }
		int temp() const { return m_temp; }

	  private:
		friend class HalfMesh;
		HalfEdge m_he0, m_he1, m_he2;
		Triangle m_tri;
		int m_index, m_temp;
	};

	class HalfMesh {
	  public:
		HalfMesh(CRange<float3> positions = CRange<float3>(),
				 CRange<TriIndices> tri_indices = CRange<TriIndices>()) {
			// TODO: add more asserts:
			//- edge can be shared by at most two triangles
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

		bool empty() const { return m_faces.empty(); }

		bool canAddVert(const float3 &new_pos) const {
			for(auto &vert : m_verts)
				if(distance(vert->pos(), new_pos) < constant::epsilon)
					return false;
			return true;
		}

		Vertex *addVertex(const float3 &pos) {
			DASSERT(canAddVert(pos));
			m_verts.emplace_back(make_unique<Vertex>(pos, (int)m_verts.size()));
			return m_verts.back().get();
		}

		Face *addFace(Vertex *a, Vertex *b, Vertex *c) {
			m_faces.emplace_back(make_unique<Face>(a, b, c, (int)m_faces.size()));
			return m_faces.back().get();
		}

		vector<HalfEdge *> orderedEdges(Vertex *vert) {
			vector<HalfEdge *> out;
			DASSERT(vert);
			auto *first = vert->first(), *temp = first;
			do {
				out.emplace_back(temp);
				temp = temp->nextVert();
				DASSERT(temp && "All edges must be paired");
			} while(temp != first);

			return out;
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

		Face *findFace(Vertex *a, Vertex *b, Vertex *c) {
			for(HalfEdge *edge : a->all()) {
				if(edge->end() == b && edge->next()->end() == c)
					return edge->face();
				if(edge->end() == c && edge->next()->end() == b)
					return edge->face();
			}
			return nullptr;
		}

		Tetrahedron makeTet(const Vertex *a, const Vertex *b, const Vertex *c,
							const Vertex *d) const {
			DASSERT(a && b && c && d);
			return Tetrahedron(a->pos(), b->pos(), c->pos(), d->pos());
		}

		Tetrahedron makeTet(HalfEdge *edge) {
			DASSERT(edge && edge->opposite());
			return makeTet(edge->start(), edge->end(), edge->next()->end(),
						   edge->opposite()->next()->end());
		}

		Tetrahedron makeTet(Vertex *vert) {
			DASSERT(vert && vert->degree() == 3);
			auto edges = vert->all();
			return makeTet(vert, edges[0]->end(), edges[1]->end(), edges[2]->end());
		}

		bool isIntersecting(const Tetrahedron &tet) {
			for(auto *face : faces())
				if(!face->temp())
					if(tet.isIntersecting(face->triangle()))
						return true;
			return false;
		}

		bool isIntersecting(const Tetrahedron &tet, const vector<Face *> &exclude) {
			for(auto *face : exclude)
				face->setTemp(1);
			bool out = isIntersecting(tet);
			for(auto *face : exclude)
				face->setTemp(0);
			return out;
		}

		Vertex *findBestVert() {
			Vertex *best = nullptr;
			float best_dot = constant::inf;

			for(auto *vert : verts()) {
				if(vert->degree() != 3)
					continue;
				auto rem_edges = orderedEdges(vert);
				DASSERT((int)rem_edges.size() == 3);

				float3 corners[3] = {rem_edges[0]->end()->pos(), rem_edges[1]->end()->pos(),
									 rem_edges[2]->end()->pos()};
				float3 center = (corners[0] + corners[1] + corners[2]) / 3.0f;
				bool neg_side = true;
				for(auto *edge : rem_edges)
					if(dot(edge->face()->triangle(), center) >= 0.0f)
						neg_side = false;

				if(neg_side)
					best = vert;
			}

			return best;
		}

		HalfEdge *findBestEdge() {
			HalfEdge *best = nullptr;
			float best_dot = constant::inf;

			for(auto *edge : halfEdges()) {
				if(!edge->opposite())
					continue;

				Tetrahedron tet = makeTet(edge);

				Face *f1 = edge->face();
				Face *f2 = edge->opposite()->face();

				float3 corners[3] = {edge->end()->pos(), edge->next()->end()->pos(),
									 edge->opposite()->next()->end()->pos()};
				float3 center = (corners[0] + corners[1] + corners[2]) / 3.0f;

				if(dot(f1->triangle(), center) >= 0.0f || dot(f2->triangle(), center) >= 0.0f)
					continue;
				if(fabs(dot(f1->triangle().normal(), f2->triangle().normal())) >
				   1.0f - constant::epsilon)
					continue;

				float fdot = fabs(dot(f1->triangle().normal(), f2->triangle().normal()));
				if(fdot < best_dot) {
					if(!isIntersecting(tet, {f1, f2})) {
						best = edge;
						best_dot = fdot;
						break;
					}
				}
			}

			return best;
		}

		pair<Face *, float> findBestFace() {
			Face *best = nullptr;
			float best_dist = 0.001f;

			for(auto *face : faces()) {
				const auto &tri = face->triangle();
				float3 center = tri.center();

				float min_dist = 0.0f;
				float max_dist = min(min(distance(tri.a(), tri.b()), distance(tri.b(), tri.c())),
									 distance(tri.c(), tri.a())) *
								 0.7f;

				float ok_dist = 0.0f;

				while(max_dist - min_dist > 0.002f) {
					float mid = (max_dist + min_dist) * 0.5f;
					float3 new_vert = center - tri.normal() * mid;
					Tetrahedron tet(tri[0], tri[1], tri[2], new_vert);

					if(!isIntersecting(tet, {face}) && canAddVert(new_vert)) {
						min_dist = mid;
						ok_dist = mid;
					} else { max_dist = mid; }
				}

				if(ok_dist > best_dist) {
					best_dist = ok_dist;
					best = face;
					break;
				}
			}

			return make_pair(best, best_dist);
		}

		Tetrahedron extractTet() {
			FWK_PROFILE("extractTet");
			Tetrahedron out;
			const char *ext_mode = "";

			if(Vertex *best = findBestVert()) {
				DASSERT(best->degree() == 3);
				auto *edge = best->first();
				Vertex *verts[3] = {edge->prevVert()->end(), edge->end(), edge->nextVert()->end()};
				out = makeTet(best);
				removeVertex(best);

				if(auto *face = findFace(verts[0], verts[1], verts[2]))
					removeFace(face);
				else
					addFace(verts[0], verts[2], verts[1]);
				ext_mode = "vert";
			} else if(HalfEdge *edge = findBestEdge()) {
				Face *f1 = edge->face(), *f2 = edge->opposite()->face();
				Vertex *v0 = edge->start(), *v1 = edge->end();
				Vertex *vf1 = edge->next()->end(), *vf2 = edge->opposite()->next()->end();
				out = makeTet(edge);
				removeFace(f1);
				removeFace(f2);
				addFace(v1, vf1, vf2);
				addFace(v0, vf2, vf1);
				ext_mode = "edge";
			} else {
				auto pair = findBestFace();

				if(pair.first) {
					auto *face = pair.first;
					auto verts = face->verts();
					float3 new_pos =
						face->triangle().center() - face->triangle().normal() * pair.second;
					auto *new_vert = addVertex(new_pos);
					out = makeTet(new_vert, verts[0], verts[1], verts[2]);
					removeFace(face);
					addFace(verts[0], verts[1], new_vert);
					addFace(verts[1], verts[2], new_vert);
					addFace(verts[2], verts[0], new_vert);
				}
				ext_mode = "face";
			}

			if(!out.isValid()) {
				printf("Mode: %s\n", ext_mode);
				for(int i = 0; i < 4; i++) {
					float dist = distance(out[i], out[(i + 1) % 4]);
					printf("%f %f %f (-> %f)\n", out[i].x, out[i].y, out[i].z, dist);
				}

				printf("Volume: %f\n", out.volume());
			}
			DASSERT(out.isValid() && "Couldn't extract valid tetrahedron");
			return out;
		}

		void clearTemps(int value) {
			for(auto *vert : verts())
				vert->setTemp(0);
			for(auto *face : faces())
				face->setTemp(0);
		}

		void selectConnected(Vertex *vert) {
			if(vert->temp())
				return;
			vert->setTemp(1);
			for(HalfEdge *edge : vert->all()) {
				selectConnected(edge->end());
				edge->face()->setTemp(1);
			}
		}

		HalfMesh extractSelection() {
			HalfMesh out;
			std::map<Vertex *, Vertex *> vert_map;
			for(auto *vert : verts())
				if(vert->temp()) {
					auto *new_vert = out.addVertex(vert->pos());
					vert_map[vert] = new_vert;
				}

			for(auto *face : faces())
				if(face->temp()) {
					auto verts = face->verts();
					auto it0 = vert_map.find(verts[0]);
					auto it1 = vert_map.find(verts[1]);
					auto it2 = vert_map.find(verts[2]);
					if(it0 != vert_map.end() && it1 != vert_map.end() && it2 != vert_map.end())
						out.addFace(it0->second, it1->second, it2->second);
					removeFace(face);
				}

			for(auto *vert : verts())
				if(vert->temp())
					removeVertex(vert);

			return out;
		}

		void draw(Renderer &out, float scale) {
			vector<float3> lines;

			auto edge_mat = make_immutable<Material>(Color::blue, Material::flag_ignore_depth);
			for(auto *hedge : halfEdges()) {
				float3 line[2] = {hedge->start()->pos(), hedge->end()->pos()};
				float3 center = hedge->face()->triangle().center();

				line[0] = lerp(line[0], center, 0.02);
				line[1] = lerp(line[1], center, 0.02);

				lines.emplace_back(line[0]);
				lines.emplace_back(line[1]);
			}
			out.addLines(lines, edge_mat);
			lines.clear();

			for(auto *face : faces()) {
				const auto &tri = face->triangle();
				float3 center = tri.center();
				float3 normal = tri.normal() * scale;
				float3 side = normalize(tri.a() - center) * scale;

				insertBack(lines, {center, center + normal * 0.5f});
				insertBack(lines, {center + normal * 0.5f, center + normal * 0.4f + side * 0.1f});
				insertBack(lines, {center + normal * 0.5f, center + normal * 0.4f - side * 0.1f});
			}
			auto normal_mat = make_immutable<Material>(Color::blue, Material::flag_ignore_depth);
			out.addLines(lines, normal_mat);
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

static void draw(const Tetrahedron &tet, Renderer &out) {
	auto material = make_immutable<Material>(Color::red, Material::flag_clear_depth);

	Mesh mesh = Mesh::makeTetrahedron(tet);
	mesh.draw(out, material);
}

TetMesh TetMesh::make(CRange<float3> positions, CRange<TriIndices> tri_indices, int max_steps,
					  Renderer &renderer) {

	HalfMesh mesh(positions, tri_indices);
	vector<HalfMesh> sub_meshes;
	while(!mesh.empty()) {
		mesh.selectConnected(mesh.verts().front());
		sub_meshes.emplace_back(mesh.extractSelection());
		mesh.clearTemps(0);
	}

	vector<Tetrahedron> tets;
	while(max_steps) {
		bool extracted = false;
		for(auto &sub_mesh : sub_meshes) {
			if(!sub_mesh.empty()) {
				auto tet = sub_mesh.extractTet();
				tets.emplace_back(tet);
				extracted = true;
				max_steps--;
				break;
			}
		}
		if(!extracted)
			break;
	}

	for(auto &sub_mesh : sub_meshes)
		sub_mesh.draw(renderer, distance(FBox(positions).min, FBox(positions).max) * 0.05f);
	for(const auto &tet : tets)
		fwk::draw(tet, renderer);

	vector<float3> verts;
	return TetMesh(std::move(verts), std::move(vector<TetIndices>()));
}
}
