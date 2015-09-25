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
using Edge = HalfTetMesh::Edge;

struct Isect {
	Face *face_a, *face_b;
	Segment segment;
};

struct LoopElem {
	Face *face;
	Vertex *origin;
};

using Loop = vector<LoopElem>;

pair<Loop, Loop> extractLoop(vector<Isect> &isects, HalfTetMesh &ha, HalfTetMesh &hb) {
	// TODO: what if loop touches 3 faces of single tetrahedron?
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

// When more than one triangulated face belong to single tetrahedron,
// then this tet should be subdivided
void prepareMesh(HalfTetMesh &mesh, vector<Loop> &loops) {
	vector<Tet *> split_tets;

	for(auto *face : mesh.faces())
		face->setTemp(0);
	for(const auto &loop : loops)
		for(const auto &elem : loop)
			elem.face->setTemp(1);

	std::map<Face *, Face *> face_map;

	for(auto *tet : mesh.tets()) {
		int count = 0;
		for(auto *face : tet->faces())
			count += face->temp();
		if(count > 1) {
			Vertex *center = mesh.addVertex(tet->tet().center());

			auto old_faces = tet->faces();
			array<array<Vertex *, 3>, 4> old_verts;
			for(int i = 0; i < 4; i++)
				old_verts[i] = old_faces[i]->verts();
			auto new_tets = mesh.subdivideTet(tet, center);
			array<Face *, 4> new_faces = {{nullptr, nullptr, nullptr, nullptr}};
			for(int i = 0; i < 4; i++) {
				auto *new_tet = new_tets[i];
				for(auto *new_face : new_tet->faces())
					if(setIntersection(new_face->verts(), old_verts[i]).size() == 3)
						new_faces[i] = new_face;
			}

			for(int i = 0; i < 4; i++)
				face_map[old_faces[i]] = new_faces[i];
		}
	}

	for(auto &loop : loops)
		for(auto &elem : loop) {
			auto it = face_map.find(elem.face);
			if(it != face_map.end())
				elem.face = it->second;
		}
}

vector<Vertex *> sortEdgeVerts(Edge edge, vector<Vertex *> splits) {
	struct Comparator {
		Comparator(float3 orig) : orig(orig) {}

		bool operator()(const Vertex *a, const Vertex *b) const {
			return distanceSq(a->pos(), orig) < distanceSq(b->pos(), orig);
		}
		float3 orig;
	};

	std::sort(begin(splits), end(splits), Comparator(edge.a->pos()));
	return splits;
}

vector<Edge> triangulateMesh(HalfTetMesh &mesh, vector<Loop> &loops,
							 TetMesh::CSGVisualData *vis_data, int mesh_id) {
	// printf("Triangulation:\n");
	// TODO: edge may already exist
	vector<Edge> processed;

	prepareMesh(mesh, loops);

	struct FaceEdge {
		Vertex *v1, *v2;
		Face *prev, *next;
	};

	std::map<Face *, vector<FaceEdge>> face_edges;

	for(const auto &loop : loops) {
		if(loop.empty())
			continue;
		DASSERT(loop.size() >= 2);
		LoopElem prev = loop.back();

		for(int n = 0; n < (int)loop.size(); n++) {
			const auto &cur = loop[n];
			const auto &prev = loop[(n + loop.size() - 1) % loop.size()];
			const auto &next = loop[(n + 1) % loop.size()];
			face_edges[cur.face].emplace_back(
				FaceEdge{cur.origin, next.origin, prev.face, next.face});
		}
	}

	struct SplitInfo {
		Edge edge;
		vector<Vertex *> splits;
	};

	vector<pair<Vertex *, vector<array<Vertex *, 3>>>> face_triangulations;
	vector<SplitInfo> edge_splits;
	vector<Tet *> rem_tets;

	for(auto pair : face_edges) {
		auto *face = pair.first;
		auto verts = pair.first->verts();
		array<vector<Vertex *>, 3> edge_verts;
		rem_tets.emplace_back(face->tet());

		vector<Edge> tri_edges;
		for(auto edge : pair.second) {
			if(edge.prev != face)
				edge_verts[face->edgeId(mesh.sharedEdge(face, edge.prev))].emplace_back(edge.v1);
			if(edge.next != face)
				edge_verts[face->edgeId(mesh.sharedEdge(face, edge.next))].emplace_back(edge.v2);
			tri_edges.emplace_back(Edge(edge.v1, edge.v2));
			processed.emplace_back(tri_edges.back());
		}

		for(int i = 0; i < 3; i++)
			if(!edge_verts[i].empty()) {
				auto edge = face->edges()[i];
				auto splits = sortEdgeVerts(edge, edge_verts[i]);
				edge_splits.emplace_back(SplitInfo{face->edges()[i], splits});

				tri_edges.emplace_back(edge.a, splits.front());
				for(int i = 0; i + 1 < (int)splits.size(); i++)
					tri_edges.emplace_back(splits[i], splits[i + 1]);
				tri_edges.emplace_back(splits.back(), edge.b);
			}

		auto other_vert = setDifference(face->tet()->verts(), face->verts());
		DASSERT(other_vert.size() == 1);

		auto triangles = triangulateFace(face, tri_edges);
		if(vis_data && vis_data->phase == 1) {
			Color col = mesh_id % 2 ? Color::yellow : Color::blue;
			vector<Segment> edges;
			for(auto tri : triangles)
				for(int i = 0; i < 3; i++)
					edges.emplace_back(tri[i]->pos(), tri[(i + 1) % 3]->pos());
			vis_data->segment_groups.emplace_back(col, edges);
		}

		face_triangulations.emplace_back(other_vert.front(), std::move(triangles));
	}

	makeUnique(rem_tets);
	for(auto *tet : rem_tets)
		mesh.removeTet(tet);

	for(const auto &split : edge_splits)
		mesh.subdivideEdge(split.edge.a, split.edge.b, split.splits);

	for(const auto &tring : face_triangulations)
		for(auto face_verts : tring.second)
			mesh.addTet(face_verts[0], face_verts[1], face_verts[2], tring.first);

	if(vis_data && vis_data->phase == 1 && mesh_id == 1) {
		vector<Triangle> tris;
		for(auto tring : face_triangulations)
			for(auto verts : tring.second)
				tris.emplace_back(verts[0]->pos(), verts[1]->pos(), verts[2]->pos());

		vis_data->poly_soups.emplace_back(Color::red, tris);
	}
	// TetMesh fill_tets = TetMesh::make(fill_mesh);

	return processed;
}

// Each face gets a number
void genSegments(HalfTetMesh &mesh, const HalfTetMesh &other, vector<Edge> &edges) {
	auto faces = mesh.faces();
	std::sort(begin(faces), end(faces),
			  [](auto *a, auto *b) { return a->triangle().area() > b->triangle().area(); });
	for(auto *face : faces)
		face->setTemp(0);

	int cur_ident = 1;
	while(true) {
		Face *start_face = nullptr;
		for(auto *face : faces)
			if(face->temp() == 0 && face->isBoundary()) {
				start_face = face;
				break;
			}
		if(!start_face)
			break;

		bool is_inside_computed = false;
		bool is_inside = false;

		vector<Face *> list = {start_face};
		while(!list.empty()) {
			auto *face = list.back();
			list.pop_back();
			if(face->temp() != 0)
				continue;

			if(!is_inside_computed) {
				is_inside_computed = true;
				is_inside = other.isIntersecting(face->triangle().center());
			}
			face->setTemp(is_inside ? -cur_ident : cur_ident);

			for(auto edge : face->edges()) {
				bool is_seg_boundary = false;
				for(auto tedge : edges)
					if(edge == tedge || edge == tedge.inverse()) {
						is_seg_boundary = true;
						break;
					}

				if(!is_seg_boundary)
					for(auto *nface : mesh.edgeFaces(edge.a, edge.b))
						if(nface->temp() == 0 && nface->isBoundary())
							list.emplace_back(nface);
			}
		}
		cur_ident++;
	}
}

TetMesh finalCuts(HalfTetMesh h1, HalfTetMesh h2, TetMesh::CSGVisualData *vis_data) {
	vector<Triangle> tris1, tris2;
	vector<Face *> faces1, faces2;

	for(auto *tet : h1.tets())
		tet->setTemp(h2.isIntersecting(tet->tet()) ? 1 : 0);
	for(auto *tet : h1.tets()) {
		if(tet->temp()) {
			auto faces = tet->faces();
			auto neighbours = tet->neighbours();

			for(int i = 0; i < 4; i++)
				if(faces[i]->temp() > 0 || (neighbours[i] && !neighbours[i]->temp()))
					faces1.emplace_back(faces[i]);
		}
	}

	for(auto *face : h2.faces())
		if(face->temp() < 0)
			faces2.emplace_back(face);

	for(auto *face1 : faces1)
		tris1.emplace_back(face1->triangle());
	for(auto *face2 : faces2)
		tris2.emplace_back(face2->verts()[0]->pos(), face2->verts()[2]->pos(),
						   face2->verts()[1]->pos());

	auto all_tris = tris1;
	insertBack(all_tris, tris2);
	Mesh fill_mesh = Mesh::makePolySoup(all_tris);
	if(vis_data && vis_data->phase == 3) {
		vis_data->poly_soups.emplace_back(Color::red, tris1);
		vis_data->poly_soups.emplace_back(Color::green, tris2);
	}
	// printf("is manifold: %s\n", HalfMesh(fill_mesh).is2Manifold() ? "yes" : "no");

	return TetMesh(h1);
}

TetMesh TetMesh::csg(const TetMesh &a, const TetMesh &b, CSGMode mode, CSGVisualData *vis_data) {
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

	vector<Segment> boundary_segs;
	vector<Triangle> boundary_tris[2];

	while(true) {
		auto new_loop = extractLoop(isects, ha, hb);
		if(new_loop.first.empty())
			break;

		for(int n = 0, count = (int)new_loop.first.size(); n < count; n++) {
			const auto &prev = new_loop.first[(n - 1 + count) % count];
			const auto &cur = new_loop.first[n];

			boundary_segs.emplace_back(Segment(prev.origin->pos(), cur.origin->pos()));
			boundary_tris[0].emplace_back(cur.face->triangle());
		}
		for(int n = 0, count = (int)new_loop.second.size(); n < count; n++) {
			const auto &cur = new_loop.second[n];
			boundary_tris[1].emplace_back(cur.face->triangle());
		}

		a_loops.emplace_back(new_loop.first);
		b_loops.emplace_back(new_loop.second);
	}

	if(vis_data) {
		vis_data->segment_groups.emplace_back(Color::black, boundary_segs);
		if(vis_data->phase == 0) {
			vis_data->poly_soups.emplace_back(Color::red, boundary_tris[0]);
			vis_data->poly_soups.emplace_back(Color::green, boundary_tris[1]);
		}
	}

	try {
		auto edges1 = triangulateMesh(ha, a_loops, vis_data, 1);
		genSegments(ha, hb, edges1);

		vector<Color> colors = {Color::green, Color::red, Color::blue, Color::yellow, Color::white};

		if(vis_data->phase == 2) {
			vector<vector<Triangle>> segs;
			segs.clear();
			for(auto *face : ha.faces()) {
				int seg_id = abs(face->temp());
				if(seg_id != 0) {
					if((int)segs.size() < seg_id + 1)
						segs.resize(seg_id + 1);
					segs[seg_id].emplace_back(face->triangle());
				}
			}
			for(int n = 0; n < (int)segs.size(); n++)
				vis_data->poly_soups.emplace_back(colors[n % colors.size()], segs[n]);
		}

		//	for(auto edge : edges1)
		//		segments.emplace_back(edge.a->pos(), edge.b->pos());

		auto edges2 = triangulateMesh(hb, b_loops, vis_data, 2);
		genSegments(hb, ha, edges2);

		auto final = finalCuts(ha, hb, vis_data);

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
