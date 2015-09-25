/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include "fwk_profile.h"
#include "fwk_cache.h"
#include <algorithm>
#include <limits>
#include <set>

namespace fwk {

using TriIndices = TetMesh::TriIndices;

namespace {
	array<int, 3> sortFace(array<int, 3> face) {
		if(face[0] > face[1])
			swap(face[0], face[1]);
		if(face[1] > face[2])
			swap(face[1], face[2]);
		if(face[0] > face[1])
			swap(face[0], face[1]);

		return face;
	}
}

TetMesh::TetMesh(vector<float3> positions, CRange<TetIndices> tet_indices)
	: m_verts(std::move(positions)), m_tet_verts(begin(tet_indices), end(tet_indices)) {
	for(int i = 0; i < (int)m_tet_verts.size(); i++)
		if(makeTet(i).volume() < 0.0f)
			swap(m_tet_verts[i][2], m_tet_verts[i][3]);

	std::map<array<int, 3>, int> faces;
	m_tet_tets.resize(m_tet_verts.size(), {{-1, -1, -1, -1}});
	for(int t = 0; t < (int)m_tet_verts.size(); t++) {
		const auto &tvert = m_tet_verts[t];
		for(int i = 0; i < 4; i++) {
			auto face = sortFace(tetFace(t, i));
			auto it = faces.find(face);
			if(it == faces.end()) {
				faces[face] = t;
			} else {
				m_tet_tets[t][i] = it->second;
				for(int j = 0; j < 4; j++)
					if(sortFace(tetFace(it->second, j)) == face) {
						DASSERT(m_tet_tets[it->second][j] == -1);
						m_tet_tets[it->second][j] = t;
						break;
					}
			}
		}
	}
}

Tetrahedron TetMesh::makeTet(int tet) const {
	const auto &tverts = m_tet_verts[tet];
	return Tetrahedron(m_verts[tverts[0]], m_verts[tverts[1]], m_verts[tverts[2]],
					   m_verts[tverts[3]]);
}

void TetMesh::drawLines(Renderer &out, PMaterial material, const Matrix4 &matrix) const {
	out.pushViewMatrix();
	out.mulViewMatrix(matrix);

	vector<float3> lines;
	for(auto tet : m_tet_verts) {
		float3 tlines[12];
		int pairs[12] = {0, 1, 0, 2, 2, 1, 3, 0, 3, 1, 3, 2};
		for(int i = 0; i < arraySize(pairs); i++)
			tlines[i] = m_verts[tet[pairs[i]]];
		lines.insert(end(lines), begin(tlines), end(tlines));
	}
	out.addLines(lines, material);
	out.popViewMatrix();
}

void TetMesh::drawTets(Renderer &out, PMaterial material, const Matrix4 &matrix) const {
	auto key = Cache::makeKey(get_immutable_ptr()); // TODO: what about null?

	PMesh mesh;
	if(!(mesh = Cache::access<Mesh>(key))) {
		vector<Mesh> meshes;

		for(int t = 0; t < size(); t++) {
			Tetrahedron tet = makeTet(t);
			float3 verts[4];
			for(int i = 0; i < 4; i++)
				verts[i] = lerp(tet[i], tet.center(), 0.05f);
			meshes.emplace_back(Mesh::makeTetrahedron(Tetrahedron(verts)));
		}

		mesh = make_immutable<Mesh>(Mesh::merge(meshes));
		Cache::add(key, mesh);
	}

	mesh->draw(out, material, matrix);
}

TetMesh TetMesh::makeUnion(const vector<TetMesh> &sub_tets) {
	vector<float3> positions;
	vector<TetIndices> indices;

	for(const auto &sub_tet : sub_tets) {
		int off = (int)positions.size();
		insertBack(positions, sub_tet.verts());
		for(auto tidx : sub_tet.tetVerts()) {
			TetIndices inds{{tidx[0] + off, tidx[1] + off, tidx[2] + off, tidx[3] + off}};
			indices.emplace_back(inds);
		}
	}

	return TetMesh(std::move(positions), std::move(indices));
}

TetMesh TetMesh::transform(const Matrix4 &matrix, TetMesh mesh) {
	// TODO: how can we modify tetmesh in place?
	return TetMesh(MeshBuffers::transform(matrix, MeshBuffers(mesh.verts())).positions,
				   mesh.tetVerts());
}

TetMesh TetMesh::selectTets(const TetMesh &mesh, const vector<int> &indices) {
	// TODO: select vertices
	auto indexer = indexWith(mesh.tetVerts(), indices);
	return TetMesh(mesh.verts(), vector<TetIndices>(begin(indexer), end(indexer)));
}

using Tet = HalfTetMesh::Tet;
using Face = HalfTetMesh::Face;
using Vertex = HalfTetMesh::Vertex;

struct Isect {
	Face *face_a, *face_b;
	Segment segment;
};

struct LoopElem {
	Face *face;
	Vertex *vertex;
};

using Loop = vector<LoopElem>;
using Edge = HalfTetMesh::Edge;

pair<Loop, Loop> extractLoop(vector<Isect> &isects, HalfTetMesh &ha, HalfTetMesh &hb) {
	if(isects.empty())
		return make_pair(Loop(), Loop());

	vector<Isect> current = {isects.back()};
	isects.pop_back();

	while(true) {
		float3 cur_end = current.back().segment.end();
		if(distance(cur_end, current.front().segment.origin()) < constant::epsilon)
			break;

		bool found = false;
		for(int n = 0; n < (int)isects.size(); n++) {
			Segment segment = isects[n].segment;
			bool connects = distance(cur_end, segment.origin()) < constant::epsilon;
			if(!connects && distance(cur_end, segment.end()) < constant::epsilon) {
				connects = true;
				segment = Segment(segment.end(), segment.origin());
			}

			if(connects) {
				current.emplace_back(Isect{isects[n].face_a, isects[n].face_b, segment});
				isects[n] = isects.back();
				isects.pop_back();
				found = true;
				break;
			}
		}
		if(!found)
			return make_pair(Loop(), Loop());
	}

	// TODO: proper neighbour tracing for robust intersection finding
	DASSERT(distance(current.back().segment.end(), current.front().segment.origin()) <
			constant::epsilon);

	Loop outa, outb;
	for(const auto &isect : current) {
		// TODO: vertex may already exist
		Vertex *va = ha.addVertex(isect.segment.origin());
		Vertex *vb = hb.addVertex(isect.segment.origin());
		outa.emplace_back(LoopElem{isect.face_a, va});
		outb.emplace_back(LoopElem{isect.face_b, vb});
	}
	return make_pair(outa, outb);
}

vector<array<Vertex *, 3>> triangulateFace(Face *face, const vector<Edge> &edges) {
	Plane plane(face->triangle());
	Projection proj(face->triangle());

	vector<Segment2D> segs;
	vector<Vertex *> verts;
	insertBack(verts, face->verts());

	for(auto &edge : edges) {
		Segment seg(proj * edge.a->pos(), proj * edge.b->pos());
		DASSERT(distance(proj / seg.origin(), edge.a->pos()) < constant::epsilon);
		verts.emplace_back(edge.a);
		verts.emplace_back(edge.b);
		segs.emplace_back(seg.xz());
	}

	// TODO: this is not correct, fix it (original face edges will overlap with newly created ones)
	for(int i = 0; i < 3; i++) {
		Segment seg(proj * face->verts()[i]->pos(), proj * face->verts()[(i + 1) % 3]->pos());
		segs.emplace_back(seg.xz());
	}
	makeUnique(verts);

	vector<array<Vertex *, 3>> out;
	auto triangulation = triangulate(segs);
	for(auto tri : triangulation) {
		array<Vertex *, 3> otri = {{nullptr, nullptr, nullptr}};

		for(int i = 0; i < 3; i++) {
			float3 pos = proj / float3(tri[i].x, 0.0f, tri[i].y);
			float min_dist = constant::inf;
			for(auto *vert : verts) {
				float dist = distance(vert->pos(), pos);
				if(dist < min_dist) {
					min_dist = dist;
					otri[i] = vert;
				}
			}
		}
		if(dot(face->triangle().normal(),
			   Triangle(otri[0]->pos(), otri[1]->pos(), otri[2]->pos()).normal()) < 0.0f)
			swap(otri[1], otri[2]);

		if(otri[0] && otri[1] && otri[2]) {
			bool wrong = otri[0] == otri[1] || otri[1] == otri[2] || otri[0] == otri[2];
			for(const auto &tri : out) {
				bool same = true;
				for(int i = 0; i < 3; i++)
					if(!isOneOf(otri[i], tri))
						same = false;
				if(same)
					wrong = true;
			}
			if(Triangle(otri[0]->pos(), otri[1]->pos(), otri[2]->pos()).area() < constant::epsilon)
				wrong = true;
			// TODO: wrong shouldnt happen
			if(!wrong)
				out.emplace_back(otri);
		}
	}

	std::sort(begin(out), end(out));
	auto uniq = out;
	makeUnique(uniq);
	DASSERT(uniq == out);
	return out;
}

struct Split {
	Vertex *e1, *e2;
	vector<Vertex *> splits;
};

vector<Split> findEdgeSplits(const HalfTetMesh &mesh, array<Vertex *, 3> corners,
							 const vector<array<Vertex *, 3>> &tris) {
	std::set<Edge> edges;
	for(auto tri : tris) {
		for(int i = 0; i < 3; i++) {
			Edge edge(tri[i], tri[(i + 1) % 3]);
			if(edge.a > edge.b)
				swap(edge.a, edge.b);

			auto it = edges.find(edge);
			if(it == edges.end())
				edges.insert(edge);
			else
				edges.erase(it);
		}
	}

	array<Split, 3> splits;
	float3 dir[3];

	for(int i = 0; i < 3; i++) {
		splits[i].e1 = corners[i];
		splits[i].e2 = corners[(i + 1) % 3];
		dir[i] = normalize(splits[i].e1->pos() - splits[i].e2->pos());
	}

	for(auto edge : edges) {
		int best_split = 0;
		float best_dot = 0.0f;
		float3 vec = normalize(edge.a->pos() - edge.b->pos());

		for(int i = 0; i < 3; i++) {
			float edot = fabs(dot(vec, dir[i]));
			if(edot > best_dot) {
				best_dot = edot;
				best_split = i;
			}
		}

		if(best_dot < 0.99f) {
			printf("findEdgeSplits: dot: %f\n", best_dot);
		}

		if(best_dot > 0.99f) {
			// DASSERT(best_dot > 0.99f);
			auto &split = splits[best_split];

			if(!isOneOf(edge.a, split.e1, split.e2)) {
				if(fabs(dot(normalize(edge.a->pos() - split.e1->pos()), vec)) >
				   1.0f - constant::epsilon)
					split.splits.emplace_back(edge.a);
			}
			if(!isOneOf(edge.b, split.e1, split.e2)) {
				if(fabs(dot(normalize(edge.b->pos() - split.e1->pos()), vec)) >
				   1.0f - constant::epsilon)
					split.splits.emplace_back(edge.b);
			}
		}
	}

	vector<Split> out;
	for(auto split : splits)
		if(!split.splits.empty())
			out.emplace_back(split);
	return out;
}

vector<Edge> triangulateMesh(HalfTetMesh &mesh, const vector<Loop> &loops, int max_steps) {
	// printf("Triangulation:\n");
	// TODO: edge may already exist
	vector<Edge> edges;
	for(const auto &loop : loops)
		for(int n = 0; n < (int)loop.size(); n++)
			edges.emplace_back(loop[n].vertex, loop[(n + 1) % loop.size()].vertex);

	vector<Edge> processed;

	int step = 0;
	while(!edges.empty()) {
		if(step++ >= max_steps)
			break;

		for(int n = 0; n < (int)edges.size(); n++) {
			if(mesh.hasEdge(edges[n].a, edges[n].b)) {
				processed.emplace_back(edges[n]);
				edges[n] = edges.back();
				edges.pop_back();
			}
		}

		if(edges.empty())
			break;

		// Finding face that requires triangulation
		Face *cur_face = nullptr;
		for(auto *face : mesh.faces()) {
			auto edge = edges.back();
			Projection proj(face->triangle());
			Triangle2D tri = (proj * face->triangle()).xz();
			auto proj_seg = proj * Segment(edge.a->pos(), edge.b->pos());
			if(fabs(proj_seg.origin().y) > constant::epsilon ||
			   fabs(proj_seg.end().y) > constant::epsilon)
				continue;

			auto result = clip(tri, proj_seg.xz());
			if(!result.inside.empty()) {
				cur_face = face;
				break;
			}
		}

		if(!cur_face) {
			printf("Cannot find face for edges:\n");
			for(auto edge : edges)
				xmlPrint("% -> % (len: %)\n", edge.a->pos(), edge.b->pos(),
						 distance(edge.a->pos(), edge.b->pos()));
			printf("\n");
		}
		DASSERT(cur_face);

		// Finding all edges lying on that face (clipping if necessary)
		Projection proj(cur_face->triangle());
		Triangle2D tri2d = (proj * cur_face->triangle()).xz();
		vector<Edge> cur_edges;

		for(int n = 0; n < (int)edges.size(); n++) {
			auto edge = edges[n];
			auto proj_seg = proj * Segment(edge.a->pos(), edge.b->pos());
			if(fabs(proj_seg.origin().y) > constant::epsilon ||
			   fabs(proj_seg.end().y) > constant::epsilon)
				continue;

			Segment2D seg2d = proj_seg.xz();
			auto result = clip(tri2d, seg2d);
			if(result.inside.empty())
				continue;

			edges[n--] = edges.back();
			edges.pop_back();

			if(!result.outside_front.empty()) {
				float3 pos(result.inside.start.x, 0.0f, result.inside.start.y);
				auto vert = mesh.addVertex(proj / pos);
				edges.push_back(Edge(edge.a, vert));
				edge = Edge(vert, edge.b);
			}
			if(!result.outside_back.empty()) {
				float3 pos(result.inside.end.x, 0.0f, result.inside.end.y);
				auto vert = mesh.addVertex(proj / pos);
				edges.push_back(Edge(vert, edge.b));
				edge = Edge(edge.a, vert);
			}

			cur_edges.emplace_back(edge);
		}

		// Triangulating tet-face
		auto triangles = triangulateFace(cur_face, cur_edges);
		auto tet = cur_face->tet();
		auto face_corners = cur_face->verts();
		insertBack(processed, cur_edges);

		Vertex *other_vertex = nullptr;
		for(auto *vert : tet->verts())
			if(!isOneOf(vert, cur_face->verts()))
				other_vertex = vert;
		mesh.removeTet(tet);

		auto splits = findEdgeSplits(mesh, face_corners, triangles);
		for(auto split : splits) {
			// xmlPrint("Splits for (%) (%):\n", split.e1->pos(), split.e2->pos());
			// for(auto s : split.splits)
			//	xmlPrint("(%) ", s->pos());
			// xmlPrint("\n");
			mesh.subdivideEdge(split.e1, split.e2, split.splits);
		}

		for(auto tri : triangles) {
			// TODO: check for negative volume
			mesh.addTet(tri[0], tri[1], tri[2], other_vertex);
		}
	}

	// TetMesh fill_tets = TetMesh::make(fill_mesh);

	return processed;
}

// Each face gets a number
void genSegments(HalfTetMesh &mesh, vector<Edge> &edges) {
	auto faces = mesh.faces();
	std::sort(begin(faces), end(faces),
			  [](auto *a, auto *b) { return a->triangle().area() > b->triangle().area(); });
	for(auto *face : faces)
		face->setTemp(-1);

	int cur_ident = 0;
	while(true) {
		Face *start_face = nullptr;
		for(auto *face : faces)
			if(face->temp() == -1 && face->isBoundary()) {
				start_face = face;
				break;
			}
		if(!start_face)
			break;

		vector<Face *> list = {start_face};
		while(!list.empty()) {
			auto *face = list.back();
			list.pop_back();
			if(face->temp() != -1)
				continue;
			face->setTemp(cur_ident);

			for(auto edge : face->edges()) {
				bool is_seg_boundary = false;
				for(auto tedge : edges)
					if(edge == tedge || edge == tedge.inverse()) {
						is_seg_boundary = true;
						break;
					}

				if(!is_seg_boundary)
					for(auto *nface : mesh.edgeFaces(edge.a, edge.b))
						if(nface->temp() == -1 && nface->isBoundary())
							list.emplace_back(nface);
			}
		}
		cur_ident++;
	}
}

TetMesh finalCuts(HalfTetMesh ha, HalfTetMesh hb, TetMesh &rem) {
	HalfTetMesh removed = ha;

	for(auto *tet : ha.tets()) {
		Tetrahedron tetra1(tet->tet());
		bool isect = false;
		for(auto *other : hb.tets())
			if(areIntersecting(tetra1, other->tet())) {
				isect = true;
				break;
			}
		if(isect)
			ha.removeTet(tet);
	}
	for(auto *tet : removed.tets()) {
		Tetrahedron tetra1(tet->tet());
		bool isect = false;
		for(auto *other : hb.tets())
			if(areIntersecting(tetra1, other->tet())) {
				isect = true;
				break;
			}
		if(!isect)
			removed.removeTet(tet);
	}

	rem = TetMesh(removed);
	return TetMesh(ha);
}

TetMesh TetMesh::boundaryIsect(const TetMesh &a, const TetMesh &b, vector<Segment> &segments,
							   vector<Triangle> &tris, vector<Tetrahedron> &ttets,
							   vector<vector<Triangle>> &segs, TetMesh &final, TetMesh &final_rem,
							   int max_steps) {
	HalfTetMesh ha(a), hb(b);

	vector<pair<Tet *, Tet *>> tet_isects;

	for(auto *tet_a : ha.tets())
		if(tet_a->isBoundary())
			for(auto *tet_b : hb.tets())
				if(tet_b->isBoundary())
					if(areIntersecting(tet_a->tet(), tet_b->tet()))
						tet_isects.emplace_back(tet_a, tet_b);

	vector<Isect> isects;
	for(auto pair : tet_isects)
		for(auto *face_a : pair.first->faces())
			if(face_a->isBoundary())
				for(auto *face_b : pair.second->faces())
					if(face_b->isBoundary()) {
						auto isect = intersectionSegment(face_a->triangle(), face_b->triangle());
						if(isect.second)
							isects.emplace_back(Isect{face_a, face_b, isect.first});
					}

	vector<Loop> a_loops, b_loops;

	while(true) {
		auto new_loop = extractLoop(isects, ha, hb);
		if(new_loop.first.empty())
			break;

		for(int n = 0, count = (int)new_loop.first.size(); n < count; n++) {
			const auto &prev = new_loop.first[(n - 1 + count) % count];
			const auto &cur = new_loop.first[n];

			segments.emplace_back(Segment(prev.vertex->pos(), cur.vertex->pos()));
			tris.emplace_back(cur.face->triangle());
		}
		for(int n = 0, count = (int)new_loop.second.size(); n < count; n++) {
			const auto &cur = new_loop.second[n];
			tris.emplace_back(cur.face->triangle());
		}

		a_loops.emplace_back(new_loop.first);
		b_loops.emplace_back(new_loop.second);
	}

	try {
		auto edges1 = triangulateMesh(ha, a_loops, max_steps);
		genSegments(ha, edges1);

		segs.clear();
		for(auto *face : ha.faces()) {
			int seg_id = face->temp();
			if(seg_id != -1) {
				if((int)segs.size() < seg_id + 1)
					segs.resize(seg_id + 1);
				segs[seg_id].emplace_back(face->triangle());
			}
		}

		//	for(auto edge : edges1)
		//		segments.emplace_back(edge.a->pos(), edge.b->pos());

		auto edges2 = triangulateMesh(hb, b_loops, max_steps);
		genSegments(hb, edges2);

		final = finalCuts(ha, hb, final_rem);

		DASSERT(HalfMesh(TetMesh(ha).toMesh()).is2Manifold());
		return TetMesh(ha);
	} catch(const Exception &ex) {
		printf("Error: %s\n", ex.what());
		return {};
	}
}

Mesh TetMesh::toMesh() const {
	vector<uint> faces;
	for(int t = 0; t < size(); t++)
		for(int i = 0; i < 4; i++)
			if(m_tet_tets[t][i] == -1) {
				auto face = tetFace(t, i);
				insertBack(faces, {(uint)face[0], (uint)face[1], (uint)face[2]});
			}

	return Mesh(MeshBuffers(m_verts), {{faces}});
}
}
