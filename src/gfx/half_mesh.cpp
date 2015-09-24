/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"

namespace fwk {

using Vertex = HalfMesh::Vertex;
using Face = HalfMesh::Face;
using HalfEdge = HalfMesh::HalfEdge;

HalfMesh::Vertex::Vertex(float3 pos, int index) : m_pos(pos), m_index(index), m_temp(0) {}
HalfMesh::Vertex::~Vertex() {
#ifndef NDEBUG
	if(!m_edges.empty()) {
		static bool reported = false;
		if(!reported) {
			reported = true;
			printf("HalfEdges should be destroyed before Vertices\n");
		}
	}
#endif
}

void HalfMesh::Vertex::removeEdge(HalfEdge *edge) {
	for(int e = 0; e < (int)m_edges.size(); e++)
		if(m_edges[e] == edge) {
			m_edges[e] = m_edges.back();
			m_edges.pop_back();
			break;
		}
}
void HalfMesh::Vertex::addEdge(HalfEdge *edge) { m_edges.emplace_back(edge); }

HalfMesh::HalfEdge::~HalfEdge() {
	m_start->removeEdge(this);
	if(m_opposite) {
#ifndef NDEBUG
		if(m_opposite->m_opposite != this) {
			static bool reported = false;
			if(!reported) {
				reported = true;
				printf("Errors in HalfMesh edges relationships");
			}
		}
#endif
		m_opposite->m_opposite = nullptr;
	}
}
HalfMesh::HalfEdge::HalfEdge(Vertex *v1, Vertex *v2, HalfEdge *next, HalfEdge *prev, Face *face)
	: m_start(v1), m_end(v2), m_opposite(nullptr), m_next(next), m_prev(prev), m_face(face) {
	DASSERT(v1 && v2 && v1 != v2 && face);
	m_start->addEdge(this);

	for(auto *end_edge : m_end->all())
		if(end_edge->end() == m_start) {
			m_opposite = end_edge;
			DASSERT(end_edge->m_opposite == nullptr &&
					"One edge shouldn't be shared by more than two triangles");
			end_edge->m_opposite = this;
		}
}

HalfMesh::Face::Face(Vertex *v1, Vertex *v2, Vertex *v3, int index)
	: m_he0(v1, v2, &m_he1, &m_he2, this), m_he1(v2, v3, &m_he2, &m_he0, this),
	  m_he2(v3, v1, &m_he0, &m_he1, this), m_index(index), m_temp(0) {
	DASSERT(v1 && v2 && v3);
	DASSERT(v1 != v2 && v2 != v3 && v3 != v1);
	m_tri = Triangle(v1->pos(), v2->pos(), v3->pos());
}

HalfMesh::HalfMesh(CRange<float3> positions, CRange<TriIndices> tri_indices) {
	// TODO: add more asserts:
	//- edge can be shared by at most two triangles
	// asserts shouldn't be debug?
	for(auto pos : positions)
		addVertex(pos);

	for(int f = 0; f < tri_indices.size(); f++) {
		const auto &ids = tri_indices[f];
		for(int i = 0; i < 3; i++)
			DASSERT(ids[i] >= 0 && ids[i] < (int)m_verts.size());
		addFace(m_verts[ids[0]].get(), m_verts[ids[1]].get(), m_verts[ids[2]].get());
	}
}

bool HalfMesh::is2ManifoldUnion() const {
	for(auto *edge : const_cast<HalfMesh *>(this)->halfEdges())
		if(!edge->opposite())
			return false;
	return true;
}

bool HalfMesh::is2Manifold() const {
	if(!is2ManifoldUnion())
		return false;
	if(m_verts.empty())
		return true;

	vector<char> visited(m_verts.size(), 0);

	vector<Vertex *> list = {m_verts.front().get()};
	while(!list.empty()) {
		auto *vert = list.back();
		list.pop_back();
		if(visited[vert->index()])
			continue;
		visited[vert->index()] = 1;
		for(auto *edge : vert->all())
			list.emplace_back(edge->end());
	}

	for(auto vert_visited : visited)
		if(!vert_visited)
			return false;
	return true;
}

Vertex *HalfMesh::addVertex(const float3 &pos) {
	m_verts.emplace_back(make_unique<Vertex>(pos, (int)m_verts.size()));
	return m_verts.back().get();
}

Face *HalfMesh::addFace(Vertex *a, Vertex *b, Vertex *c) {
	DASSERT(a != b && b != c && c != a);
	DASSERT(!findFace(a, b, c));
	m_faces.emplace_back(make_unique<Face>(a, b, c, (int)m_faces.size()));
	return m_faces.back().get();
}

vector<HalfEdge *> HalfMesh::orderedEdges(Vertex *vert) {
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

void HalfMesh::removeVertex(Vertex *vert) {
	int index = vert->m_index;
	DASSERT(vert && m_verts[index].get() == vert);

	vector<Face *> to_remove;
	for(auto *vedge : vert->all())
		removeFace(vedge->face());

	m_verts[index] = std::move(m_verts.back());
	m_verts[index]->m_index = index;
	m_verts.pop_back();
}

void HalfMesh::removeFace(Face *face) {
	int index = face->m_index;
	DASSERT(face && m_faces[index].get() == face);
	m_faces[index] = std::move(m_faces.back());
	m_faces[index]->m_index = index;
	m_faces.pop_back();
}

vector<Vertex *> HalfMesh::verts() {
	vector<Vertex *> out;
	for(auto &vert : m_verts)
		out.emplace_back(vert.get());
	return out;
}

vector<Face *> HalfMesh::faces() {
	vector<Face *> out;
	for(auto &face : m_faces)
		out.emplace_back(face.get());
	return out;
}

vector<HalfEdge *> HalfMesh::halfEdges() {
	vector<HalfEdge *> out;
	for(auto &face : m_faces)
		insertBack(out, face->halfEdges());
	return out;
}

Face *HalfMesh::findFace(Vertex *a, Vertex *b, Vertex *c) {
	for(HalfEdge *edge : a->all()) {
		if(edge->end() == b && edge->next()->end() == c)
			return edge->face();
		if(edge->end() == c && edge->next()->end() == b)
			return edge->face();
	}
	return nullptr;
}

void HalfMesh::clearTemps(int value) {
	for(auto *vert : verts())
		vert->setTemp(0);
	for(auto *face : faces())
		face->setTemp(0);
}

void HalfMesh::selectConnected(Vertex *vert) {
	if(vert->temp())
		return;
	vert->setTemp(1);
	// TODO: this shouldn't be recursive...
	for(HalfEdge *edge : vert->all()) {
		selectConnected(edge->end());
		edge->face()->setTemp(1);
	}
}

HalfMesh HalfMesh::extractSelection() {
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

void HalfMesh::draw(Renderer &out, float scale) {
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
}
