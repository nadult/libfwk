/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"

namespace fwk {

using Tet = HalfTetMesh::Tet;
using Vertex = HalfTetMesh::Vertex;
using Face = HalfTetMesh::Face;

void HalfTetMesh::Vertex::addFace(Face *face) { m_faces.emplace_back(face); }
void HalfTetMesh::Vertex::removeFace(Face *face) {
	for(auto &vface : m_faces)
		if(vface == face) {
			vface = m_faces.back();
			m_faces.pop_back();
			break;
		}
}

void HalfTetMesh::Vertex::addTet(Tet *tet) { m_tets.emplace_back(tet); }
void HalfTetMesh::Vertex::removeTet(Tet *tet) {
	for(auto &vtet : m_tets)
		if(vtet == tet) {
			vtet = m_tets.back();
			m_tets.pop_back();
			break;
		}
}
HalfTetMesh::Face::Face(Tet *tet, Vertex *a, Vertex *b, Vertex *c, int index)
	: m_verts({{a, b, c}}), m_tet(tet), m_opposite(nullptr), m_index(index), m_temp(0) {
	DASSERT(a && b && c);
	DASSERT(a != b && a != c && b != c);
	m_tri = Triangle(a->pos(), b->pos(), c->pos());
	for(auto *face : a->faces()) {
		const auto &verts = face->verts();
		if(isOneOf(a, verts) && isOneOf(b, verts) && isOneOf(c, verts)) {
			DASSERT(m_opposite == nullptr && face->m_opposite == nullptr &&
					"Multiple faces sharing same vertices detected");
			m_opposite = face;
			face->m_opposite = this;
		}
	}

	for(auto *vert : m_verts)
		vert->addFace(this);
}

HalfTetMesh::Face::~Face() {
	for(auto *vert : m_verts)
		vert->removeFace(this);
	if(m_opposite) {
		DASSERT(m_opposite->m_opposite = this);
		m_opposite->m_opposite = nullptr;
	}
}

HalfTetMesh::Tet::Tet(Vertex *v0, Vertex *v1, Vertex *v2, Vertex *v3, int index)
	: m_faces({{make_unique<Face>(this, v0, v1, v2, 0), make_unique<Face>(this, v1, v3, v2, 1),
				make_unique<Face>(this, v2, v3, v0, 2), make_unique<Face>(this, v3, v1, v0, 3)}}),
	  m_verts({{v0, v1, v2, v3}}), m_neighbours({{nullptr, nullptr, nullptr, nullptr}}),
	  m_index(index), m_temp(0) {
	for(auto *vert : m_verts)
		vert->addTet(this);
	for(int n = 0; n < 4; n++) {
		auto *opp_face = m_faces[n]->opposite();
		if(!opp_face)
			continue;

		auto *opp_tet = opp_face->tet();
		m_neighbours[n] = opp_tet;

		for(int i = 0; i < 4; i++)
			if(opp_tet->m_faces[i].get() == opp_face) {
				DASSERT(opp_tet->m_neighbours[i] == nullptr);
				opp_tet->m_neighbours[i] = this;
			}
	}
}

bool HalfTetMesh::Tet::isBoundary() const {
	for(const auto &face : m_faces)
		if(face->isBoundary())
			return true;
	return false;
}

HalfTetMesh::Tet::~Tet() {
	for(auto *neighbour : m_neighbours)
		if(neighbour)
			for(int i = 0; i < 4; i++)
				if(neighbour->m_neighbours[i] == this)
					neighbour->m_neighbours[i] = nullptr;
	for(auto *vert : m_verts)
		vert->removeTet(this);
}

array<Face *, 4> HalfTetMesh::Tet::faces() {
	return {{m_faces[0].get(), m_faces[1].get(), m_faces[2].get(), m_faces[3].get()}};
}

Tetrahedron HalfTetMesh::Tet::tet() const {
	return Tetrahedron(m_verts[0]->pos(), m_verts[1]->pos(), m_verts[2]->pos(), m_verts[3]->pos());
}

HalfTetMesh::HalfTetMesh(const TetMesh &mesh) {
	const auto &verts = mesh.verts();
	const auto &tverts = mesh.tetVerts();

	for(auto pos : mesh.verts())
		addVertex(pos);
	for(const auto &tet : mesh.tetVerts())
		addTet(m_verts[tet[0]].get(), m_verts[tet[1]].get(), m_verts[tet[2]].get(),
			   m_verts[tet[3]].get());
}

HalfTetMesh::operator TetMesh() const {
	vector<float3> verts;
	vector<array<int, 4>> indices;

	for(auto &vert : m_verts)
		verts.emplace_back(vert->pos());
	for(auto &tet : m_tets) {
		auto verts = tet->verts();
		array<int, 4> tet_inds = {
			{verts[0]->index(), verts[1]->index(), verts[2]->index(), verts[3]->index()}};
		indices.emplace_back(tet_inds);
	}

	return TetMesh(verts, indices);
}

Vertex *HalfTetMesh::addVertex(const float3 &pos) {
	m_verts.emplace_back(make_unique<Vertex>(pos, (int)m_verts.size()));
	return m_verts.back().get();
}

Tet *HalfTetMesh::addTet(Vertex *a, Vertex *b, Vertex *c, Vertex *d) {
	DASSERT(a != b && b != c && c != a);
	DASSERT(a != d && b != d && c != d);
	DASSERT(!findTet(a, b, c, d));

	// TODO: maybe don't do that
	if(Tetrahedron(a->pos(), b->pos(), c->pos(), d->pos()).volume() < 0.0f)
		swap(c, d);
	m_tets.emplace_back(make_unique<Tet>(a, b, c, d, (int)m_tets.size()));
	return m_tets.back().get();
}

Tet *HalfTetMesh::findTet(Vertex *a, Vertex *b, Vertex *c, Vertex *d) {
	DASSERT(a && b && c && d);
	DASSERT(a != b && b != c && c != a);
	DASSERT(a != d && b != d && c != d);

	for(auto *tet : a->tets()) {
		auto &tverts = tet->verts();
		if(isOneOf(b, tverts) && isOneOf(c, tverts) && isOneOf(d, tverts))
			return tet;
	}
	return nullptr;
}

pair<Face *, Face *> HalfTetMesh::findFaces(Vertex *a, Vertex *b, Vertex *c) {
	DASSERT(a && b && c);
	DASSERT(a != b && a != c && b != c);

	Face *out = nullptr;

	for(auto *face : a->faces()) {
		auto &fverts = face->verts();
		if(isOneOf(b, fverts) && isOneOf(c, fverts)) {
			if(out)
				return make_pair(out, face);
			out = face;
		}
	}

	return make_pair(out, nullptr);
}

void HalfTetMesh::removeVertex(Vertex *vert) {
	DASSERT(vert && m_verts[vert->m_index].get() == vert);

	vector<Tet *> tets = vert->tets();
	for(auto *tet : tets)
		removeTet(tet);

	int index = vert->m_index;
	m_verts[index] = std::move(m_verts.back());
	m_verts[index]->m_index = index;
	m_verts.pop_back();
}

void HalfTetMesh::removeTet(Tet *tet) {
	DASSERT(tet && m_tets[tet->m_index].get() == tet);
	int index = tet->m_index;
	m_tets[index] = std::move(m_tets.back());
	m_tets[index]->m_index = index;
	m_tets.pop_back();
}

vector<Tet *> HalfTetMesh::tets() {
	vector<Tet *> out;
	for(const auto &tet : m_tets)
		out.emplace_back(tet.get());
	return out;
}

vector<Face *> HalfTetMesh::faces() {
	vector<Face *> out;
	for(const auto &tet : m_tets)
		insertBack(out, tet->faces());
	return out;
}

vector<Vertex *> HalfTetMesh::verts() {
	vector<Vertex *> out;
	for(const auto &vertex : m_verts)
		out.emplace_back(vertex.get());
	return out;
}

vector<Face *> HalfTetMesh::extractSelectedFaces(vector<Tet *> tets) {
	for(auto &tet : m_tets)
		tet->setTemp(0);
	for(auto *tet : tets)
		tet->setTemp(1);

	vector<Face *> out;
	for(auto *face : faces())
		if(face->tet()->temp() && (!face->opposite() || !face->opposite()->tet()->temp()))
			out.emplace_back(face);

	for(auto &tet : m_tets)
		tet->setTemp(0);
	return out;
}
}
