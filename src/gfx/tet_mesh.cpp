/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include "fwk_profile.h"
#include "fwk_cache.h"
#include <algorithm>
#include <limits>

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

template <class T> void makeUnique(vector<T> &vec) {
	std::sort(begin(vec), end(vec));
	vec.resize(std::unique(begin(vec), end(vec)) - vec.begin());
}

using Tet = HalfTetMesh::Tet;
using Face = HalfTetMesh::Face;

struct Isect {
	Face *face_a, *face_b;
	Segment segment;
};

struct Loop {
	vector<Isect> segments;

	bool empty() const { return segments.empty(); }
};

Loop extractLoop(vector<Isect> &isects) {
	if(isects.empty())
		return Loop();

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
			return Loop();
	}

	// TODO: proper neighbour tracing for robust intersection finding
	DASSERT(distance(current.back().segment.end(), current.front().segment.origin()) <
			constant::epsilon);
	return Loop{std::move(current)};
}

TetMesh TetMesh::boundaryIsect(const TetMesh &a, const TetMesh &b, vector<Segment> &segments,
							   vector<Triangle> &tris) {
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

	vector<Loop> loops;
	while(true) {
		Loop new_loop = extractLoop(isects);
		if(new_loop.empty())
			break;
		for(auto &isect : new_loop.segments) {
			segments.emplace_back(isect.segment);
			tris.emplace_back(isect.face_a->triangle());
			tris.emplace_back(isect.face_b->triangle());
		}
		loops.emplace_back(std::move(new_loop));
	}

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
