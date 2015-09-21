/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include "fwk_profile.h"
#include "fwk_cache.h"
#include <algorithm>
#include <limits>

#include <CGAL/Exact_predicates_inexact_constructions_kernel.h>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/Triangulation_face_base_with_info_2.h>
#include <CGAL/Polygon_2.h>

namespace fwk {

namespace {

	struct K : CGAL::Exact_predicates_inexact_constructions_kernel {};

	typedef CGAL::Triangulation_vertex_base_2<K> Vb;
	typedef CGAL::Constrained_triangulation_face_base_2<K> Fb;
	typedef CGAL::Triangulation_data_structure_2<Vb, Fb> TDS;
	typedef CGAL::Exact_predicates_tag Itag;
	typedef CGAL::Constrained_Delaunay_triangulation_2<K, TDS, Itag> CDT;
	typedef CDT::Point Point;
}

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

template <class T> void makeUnique(vector<T> &vec) {
	std::sort(begin(vec), end(vec));
	vec.resize(std::unique(begin(vec), end(vec)) - vec.begin());
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

struct ChangeBase {
	ChangeBase(float3 vec_x, float3 vec_y, float3 offset)
		: m_vec_x(vec_x), m_vec_y(vec_y), m_vec_z(cross(m_vec_x, m_vec_y)), m_offset(offset),
		  m_base(Matrix3(m_vec_x, m_vec_y, m_vec_z)), m_ibase(transpose(m_base)) {}

	float3 to(const float3 &vec) const { return m_ibase * (vec - m_offset); }
	float3 from(const float3 &vec) const {
		return (m_vec_x * vec.x + m_vec_y * vec.y + m_vec_z * vec.z) + m_offset;
	}

	float3 m_vec_x, m_vec_y, m_vec_z, m_offset;
	Matrix3 m_base, m_ibase;
};

float3 fromCGAL(const Point &p) {
	return float3(CGAL::to_double(p.x()), 0.0f, CGAL::to_double(p.y()));
}

void triangulateFace(Face *face, vector<pair<Vertex *, Vertex *>> &edges) {
	Plane plane(face->triangle());
	ChangeBase chbase(normalize(face->triangle().edge1()), face->triangle().normal(),
					  face->triangle().a());

	vector<Segment> segs;
	vector<Vertex *> verts;
	insertBack(verts, face->verts());

	for(auto &edge : edges) {
		Segment seg(chbase.to(edge.first->pos()), chbase.to(edge.second->pos()));
		DASSERT(distance(chbase.from(seg.origin()), edge.first->pos()) < constant::epsilon);
		verts.emplace_back(edge.first);
		verts.emplace_back(edge.second);
		segs.emplace_back(seg);
	}
	for(int i = 0; i < 3; i++) {
		Segment seg(chbase.to(face->verts()[i]->pos()),
					chbase.to(face->verts()[(i + 1) % 3]->pos()));
		segs.emplace_back(seg);
	}
	makeUnique(verts);

	//	printf("From:\n");
	CDT cdt;
	for(auto &seg : segs) {
		//		printf("%f %f -> %f %f\n", seg.origin().x, seg.origin().z, seg.end().x,
		// seg.end().z);
		cdt.insert_constraint(Point(seg.origin().x, seg.origin().z),
							  Point(seg.end().x, seg.end().z));
	}
	DASSERT(cdt.is_valid());

	vector<Segment> out;
	// printf("Triangulation:\n");
	for(auto it = cdt.finite_edges_begin(); it != cdt.finite_edges_end(); ++it) {
		auto edge = cdt.segment(it);
		float3 start = fromCGAL(edge.start()), end = fromCGAL(edge.end());
		//	printf("%f %f -> %f %f\n", start.x, start.z, end.x, end.z);
		if(distance(start, end) > constant::epsilon) {
			start = chbase.from(start);
			end = chbase.from(end);

			Vertex *vert1 = nullptr, *vert2 = nullptr;
			float mdist1 = constant::inf, mdist2 = constant::inf;
			for(auto *vert : verts) {
				float dist1 = distance(vert->pos(), start);
				float dist2 = distance(vert->pos(), end);
				if(dist1 < mdist1) {
					vert1 = vert;
					mdist1 = dist1;
				}
				if(dist2 < mdist2) {
					vert2 = vert;
					mdist2 = dist2;
				}
			}

			if(vert1 && vert2) {
				edges.emplace_back(vert1, vert2);
			}
		}
	}
}

vector<Segment> triangulateMesh(HalfTetMesh &mesh, const vector<Loop> &loops) {
	// TODO: edge may already exist
	using NewEdge = pair<Vertex *, Vertex *>;
	std::map<Face *, vector<NewEdge>> new_edge_map;

	for(const auto &loop : loops) {
		for(int n = 0; n < (int)loop.size(); n++) {
			const auto &cur = loop[n];
			const auto &next = loop[(n + 1) % loop.size()];

			NewEdge new_edge(cur.vertex, next.vertex);
			new_edge_map[cur.face].emplace_back(new_edge);
		}
	}

	vector<Segment> triangulations;
	for(auto &pair : new_edge_map) {
		triangulateFace(pair.first, pair.second);
		for(auto &edge : pair.second)
			triangulations.emplace_back(edge.first->pos(), edge.second->pos());
	}
	return triangulations;
}

TetMesh TetMesh::boundaryIsect(const TetMesh &a, const TetMesh &b, vector<Segment> &segments,
							   vector<Triangle> &tris, vector<Tetrahedron> &ttets) {
	HalfTetMesh ha(a), hb(b);

	vector<pair<Tet *, Tet *>> tet_isects;

	for(auto *tet_a : ha.tets())
		if(tet_a->isBoundary())
			for(auto *tet_b : hb.tets())
				if(tet_b->isBoundary())
					if(areIntersecting(tet_a->tet(), tet_b->tet()))
						tet_isects.emplace_back(tet_a, tet_b);

	if(0)
		for(auto *tet_a : ha.tets())
			for(auto *tet_b : hb.tets())
				if(!tet_a->isBoundary())
					if(areIntersecting(tet_a->tet(), tet_b->tet()))
						ttets.emplace_back(tet_a->tet());

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

	insertBack(segments, triangulateMesh(ha, a_loops));
	triangulateMesh(hb, b_loops);

	return TetMesh();
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
