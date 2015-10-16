/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"

namespace fwk {

using VertexId = DynamicMesh::VertexId;
using FaceId = DynamicMesh::FaceId;
using EdgeId = DynamicMesh::EdgeId;
using EdgeLoop = DynamicMesh::EdgeLoop;

DynamicMesh::DynamicMesh(CRange<float3> verts, CRange<array<uint, 3>> tris)
	: m_num_verts(0), m_num_faces(0) {
	for(auto vert : verts)
		addVertex(vert);
	for(auto tri : tris)
		addFace(VertexId(tri[0]), VertexId(tri[1]), VertexId(tri[2]));
}

DynamicMesh::operator Mesh() const {
	vector<int> vert_map(m_verts.size());
	vector<float3> out_verts;

	for(auto vert : verts()) {
		vert_map[vert] = (int)out_verts.size();
		out_verts.emplace_back(point(vert));
	}

	vector<array<uint, 3>> out_tris;
	for(auto face : faces()) {
		array<uint, 3> new_tri;
		auto fverts = verts(face);
		for(int i = 0; i < 3; i++)
			new_tri[i] = vert_map[fverts[i]];
		out_tris.emplace_back(new_tri);
	}

	return Mesh(std::move(out_verts), {std::move(out_tris)});
}

bool DynamicMesh::isManifoldUnion() const {
	for(auto face : faces())
		for(auto edge : edges(face))
			if(faces(edge).size() != 2)
				return false;
	return true;
}

bool DynamicMesh::isManifold() const {
	if(!isManifoldUnion())
		return false;
	if(m_verts.empty())
		return true;

	vector<int> visited(m_verts.size(), 0);
	vector<VertexId> list = {VertexId(0)};
	int visited_count = 0;

	while(!list.empty()) {
		auto vert = list.back();
		list.pop_back();
		if(visited[vert])
			continue;
		visited[vert] = 1;
		visited_count++;
		for(auto edge : edges(vert))
			list.emplace_back(edge.b);
	}

	return visited_count == m_num_verts;
}

bool DynamicMesh::isValid(VertexId id) const {
	return id.id >= 0 && id.id < (int)m_verts.size() && !isnan(m_verts[id].x);
}
bool DynamicMesh::isValid(FaceId id) const {
	return id.id >= 0 && id.id < (int)m_faces.size() && m_faces[id][0] != -1;
}
bool DynamicMesh::isValid(EdgeId id) const {
	return isValid(id.a) && isValid(id.b) && id.a != id.b;
}

VertexId DynamicMesh::addVertex(const float3 &pos) {
	DASSERT(!isnan(pos));

	int index = -1;
	if(m_free_verts.empty()) {
		index = (int)m_verts.size();
		m_verts.emplace_back(pos);
		m_adjacency.emplace_back(vector<int>());
	} else {
		index = m_free_verts.back();
		m_free_verts.pop_back();
		m_verts[index] = pos;
		DASSERT(m_adjacency[index].empty());
	}

	m_num_verts++;
	return VertexId(index);
}

FaceId DynamicMesh::addFace(CRange<VertexId, 3> indices) {
	for(auto idx : indices)
		DASSERT(isValid(idx));
	DASSERT(indices[0] != indices[1] && indices[1] != indices[2] && indices[2] != indices[0]);

	int index = -1;
	if(m_free_faces.empty()) {
		index = (int)m_faces.size();
		m_faces.push_back({{indices[0], indices[1], indices[2]}});
	} else {
		index = m_free_faces.back();
		m_free_faces.pop_back();
		DASSERT(m_faces[index][0] == -1);
	}

	for(auto i : indices) {
		auto &adjacency = m_adjacency[i];
		int value = index;
		for(auto &adj : adjacency)
			if(adj > value)
				swap(adj, value);
		adjacency.emplace_back(value);
	}

	m_num_faces++;
	return FaceId(index);
}

void DynamicMesh::remove(VertexId id) {
	DASSERT(isValid(id));

	while(!m_adjacency[id].empty())
		remove(FaceId(m_adjacency[id].back()));

	m_verts[id].x = NAN;
	m_free_verts.emplace_back(id);
	m_num_verts--;
}

void DynamicMesh::remove(FaceId id) {
	DASSERT(isValid(id));

	for(auto vidx : m_faces[id]) {
		auto &adjacency = m_adjacency[vidx];
		auto it = std::find(begin(adjacency), end(adjacency), id.id);
		DASSERT(it != adjacency.end());
		int pos = it - adjacency.begin();
		while(pos + 1 < (int)adjacency.size()) {
			adjacency[pos] = adjacency[pos + 1];
			pos++;
		}
		adjacency.pop_back();
	}

	m_faces[id][0] = -1;
	m_free_faces.emplace_back(id);
	m_num_faces--;
}

vector<VertexId> DynamicMesh::verts() const {
	vector<VertexId> out;
	out.reserve(m_num_verts);
	for(int i = 0; i < vertexIdCount(); i++)
		if(!isnan(m_verts[i].x))
			out.emplace_back(i);
	return out;
}

array<VertexId, 3> DynamicMesh::verts(FaceId id) const {
	DASSERT(isValid(id));
	array<VertexId, 3> out;
	const auto &face = m_faces[id];
	for(int i = 0; i < 3; i++)
		out[i] = VertexId(face[i]);
	return out;
}

array<VertexId, 2> DynamicMesh::verts(EdgeId id) const {
	DASSERT(isValid(id));
	return {{id.a, id.b}};
}

vector<FaceId> DynamicMesh::faces() const {
	vector<FaceId> out;
	out.reserve(m_num_faces);
	for(int i = 0; i < faceIdCount(); i++)
		if(m_faces[i][0] != -1)
			out.emplace_back(i);
	return out;
}

vector<FaceId> DynamicMesh::faces(FaceId id) const {
	DASSERT(isValid(id));
	const auto &face = m_faces[id];
	const auto &adj1 = m_adjacency[face[0]];
	const auto &adj2 = m_adjacency[face[1]];
	const auto &adj3 = m_adjacency[face[2]];

	auto out = setUnion(adj1, setUnion(adj2, adj3));
	auto it = std::find(begin(out), end(out), id.id);
	out.erase(it);
	return vector<FaceId>(begin(out), end(out));
}

vector<FaceId> DynamicMesh::faces(VertexId id) const {
	DASSERT(isValid(id));
	return vector<FaceId>(begin(m_adjacency[id]), end(m_adjacency[id]));
}

vector<FaceId> DynamicMesh::faces(EdgeId id) const {
	DASSERT(isValid(id));
	const auto &adj1 = m_adjacency[id.a];
	const auto &adj2 = m_adjacency[id.b];
	vector<int> out(min(adj1.size(), adj2.size()));
	auto it = std::set_intersection(begin(adj1), end(adj1), begin(adj2), end(adj2), begin(out));
	return vector<FaceId>(out.begin(), it);
}

array<EdgeId, 3> DynamicMesh::edges(FaceId id) const {
	DASSERT(isValid(id));
	array<EdgeId, 3> out;
	const auto &face = m_faces[id];
	for(int i = 0; i < 3; i++)
		out[i] = EdgeId(VertexId(face[i]), VertexId(face[(i + 1) % 3]));
	return out;
}

vector<EdgeId> DynamicMesh::edges(VertexId id) const {
	DASSERT(isValid(id));
	vector<EdgeId> out;
	out.reserve(m_adjacency[id].size());
	for(auto face_idx : m_adjacency[id]) {
		const auto &face = m_faces[face_idx];
		for(int i = 0; i < 3; i++)
			if(face[i] == id) {
				out.emplace_back(VertexId(face[i]), VertexId(face[(i + 1) % 3]));
				break;
			}
	}
	return out;
}

int DynamicMesh::faceEdgeIndex(FaceId face_id, EdgeId edge) const {
	DASSERT(isValid(edge) && isValid(face_id));
	const auto &face = m_faces[face_id];
	for(int i = 0; i < 3; i++)
		if(edge.a == face[i] && edge.b == face[(i + 1) % 3])
			return i;
	return -1;
}

EdgeId DynamicMesh::faceEdge(FaceId face_id, int sub_id) const {
	DASSERT(isValid(face_id));
	DASSERT(sub_id >= 0 && sub_id < 3);
	const auto &face = m_faces[face_id];
	return EdgeId(VertexId(face[sub_id]), VertexId(face[(sub_id + 1) % 3]));
}

VertexId DynamicMesh::closestVertex(const float3 &pos) const {
	VertexId out;
	float min_dist = constant::inf;

	for(auto vert : verts()) {
		float dist = distanceSq(pos, point(vert));
		if(dist < min_dist) {
			out = vert;
			min_dist = dist;
		}
	}

	return out;
}

Segment DynamicMesh::segment(EdgeId id) const { return Segment(point(id.a), point(id.b)); }

Triangle DynamicMesh::triangle(FaceId id) const {
	DASSERT(isValid(id));
	const auto &face = m_faces[id];
	return Triangle(m_verts[face[0]], m_verts[face[1]], m_verts[face[2]]);
}
}
