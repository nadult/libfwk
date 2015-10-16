/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"

namespace fwk {

using VertexId = DynamicMesh::VertexId;
using FaceId = DynamicMesh::FaceId;
using EdgeId = DynamicMesh::EdgeId;

DynamicMesh::DynamicMesh(CRange<float3> verts, CRange<array<uint, 3>> tris) {
	for(auto vert : verts)
		addVertex(vert);
	for(auto tri : tris)
		addFace(tri[0], tri[1], tri[2]);
}

DynamicMesh::operator Mesh() const {
	vector<array<uint, 3>> tris;
	tris.reserve(m_faces.size());
	for(const auto &face : m_faces)
		tris.push_back({{(uint)face[0], (uint)face[1], (uint)face[2]}});

	return Mesh(m_verts, {std::move(tris)});
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
	vector<VertexId> list = {0};
	size_t visited_count = 0;

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

	return visited_count == m_verts.size();
}

bool DynamicMesh::isValid(VertexId id) const { return id.id >= 0 && id.id < (int)m_verts.size(); }
bool DynamicMesh::isValid(FaceId id) const { return id.id >= 0 && id.id < (int)m_faces.size(); }
bool DynamicMesh::isValid(EdgeId id) const {
	return isValid(id.a) && isValid(id.b) && id.a != id.b;
}

VertexId DynamicMesh::addVertex(const float3 &pos) {
	int index = (int)m_verts.size();
	m_verts.emplace_back(pos);
	m_adjacency.emplace_back(vector<int>());
	return index;
}

FaceId DynamicMesh::addFace(CRange<VertexId, 3> indices) {
	for(auto idx : indices)
		DASSERT(isValid(idx));
	DASSERT(indices[0] != indices[1] && indices[1] != indices[2] && indices[2] != indices[0]);

	int index = (int)m_faces.size();
	m_faces.push_back({{indices[0], indices[1], indices[2]}});
	for(auto i : indices)
		m_adjacency[i].emplace_back(index);

	return index;
}

void DynamicMesh::removeVertex(int idx) {
	DASSERT(idx >= 0 && idx < vertexCount());
	DASSERT(m_adjacency[idx].empty());

	int last_idx = vertexCount() - 1;
	for(auto fidx : m_adjacency.back())
		for(auto &iidx : m_faces[fidx])
			if(iidx == last_idx)
				iidx = idx;
	m_verts[idx] = m_verts.back();
	m_adjacency[idx].swap(m_adjacency.back());

	m_adjacency.pop_back();
	m_verts.pop_back();
}

void DynamicMesh::removeFace(int idx) {
	DASSERT(idx >= 0 && idx < faceCount());

	for(auto vidx : m_faces[idx]) {
		auto &adjacency = m_adjacency[vidx];
		auto it = std::find(begin(adjacency), end(adjacency), idx);
		DASSERT(it != adjacency.end());
		int pos = it - adjacency.begin();
		while(pos + 1 < (int)adjacency.size()) {
			adjacency[pos] = adjacency[pos + 1];
			pos++;
		}
		adjacency.pop_back();
	}

	int last_idx = faceCount() - 1;
	for(auto vidx : m_faces.back()) {
		auto &adjacency = m_adjacency[vidx];
		int value = idx;
		for(auto &aidx : adjacency)
			if(aidx > value)
				swap(aidx, value);
		DASSERT(value == last_idx);
	}

	m_faces[idx] = m_faces.back();
	m_faces.pop_back();
}

vector<VertexId> DynamicMesh::verts() const {
	vector<VertexId> out;
	out.reserve(m_verts.size());
	for(int i = 0; i < vertexCount(); i++)
		out.emplace_back(i);
	return out;
}

array<VertexId, 3> DynamicMesh::verts(FaceId id) const {
	DASSERT(isValid(id));
	array<VertexId, 3> out;
	const auto &face = m_faces[id];
	for(int i = 0; i < 3; i++)
		out[i] = face[i];
	return out;
}

array<VertexId, 2> DynamicMesh::verts(EdgeId id) const {
	DASSERT(isValid(id));
	return {{id.a, id.b}};
}

vector<FaceId> DynamicMesh::faces() const {
	vector<FaceId> out;
	out.reserve(m_faces.size());
	for(int i = 0; i < faceCount(); i++)
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
	vector<FaceId> out(min(adj1.size(), adj2.size()));
	auto it = std::set_intersection(begin(adj1), end(adj1), begin(adj2), end(adj2), begin(out));
	out.resize(it - out.begin());
	return out;
}

array<EdgeId, 3> DynamicMesh::edges(FaceId id) const {
	DASSERT(isValid(id));
	array<EdgeId, 3> out;
	const auto &face = m_faces[id];
	for(int i = 0; i < 3; i++)
		out[i] = EdgeId(face[i], face[(i + 1) % 3]);
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
				out.emplace_back(face[i], face[(i + 1) % 3]);
				break;
			}
	}
	return out;
}

Triangle DynamicMesh::triangle(FaceId id) const {
	DASSERT(isValid(id));
	const auto &face = m_faces[id];
	return Triangle(m_verts[face[0]], m_verts[face[1]], m_verts[face[2]]);
}
}
