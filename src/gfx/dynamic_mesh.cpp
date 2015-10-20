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

	auto out_tris = transform(faces(), [&](auto face) {
		return transform(verts(face), [&](auto vert) { return (uint)vert_map[vert]; });
	});
	return Mesh(std::move(out_verts), {std::move(out_tris)});
}

DynamicMesh DynamicMesh::extract(CRange<FaceId> selection) const {
	auto used_verts = verts(selection);
	vector<int> vert_map(m_verts.size(), -1);
	for(int n = 0; n < (int)used_verts.size(); n++)
		vert_map[used_verts[n].id] = n;

	auto out_verts = transform(used_verts, [this](auto id) { return point(id); });
	vector<array<uint, 3>> out_tris;
	for(auto face : selection) {
		auto tri = transform(verts(face), [&](auto vert) { return (uint)vert_map[vert]; });
		out_tris.emplace_back(tri);
	}
	return DynamicMesh(std::move(out_verts), std::move(out_tris));
}

vector<DynamicMesh> DynamicMesh::separateSurfaces() const {
	vector<DynamicMesh> out;

	vector<char> selection(faceIdCount(), false);
	for(auto face : faces()) {
		if(selection[face])
			continue;
		auto surface = selectSurface(face);
		out.emplace_back(extract(surface));
		for(auto sface : surface)
			selection[sface] = true;
	}
	return out;
}

// More about manifolds: http://www.cs.mtu.edu/~shene/COURSES/cs3621/SLIDES/Mesh.pdf
bool DynamicMesh::isClosedOrientableSurface(CRange<FaceId> subset) const {
	vector<char> selection(faceIdCount(), false);
	for(auto face : subset)
		selection[face] = true;

	vector<FaceId> efaces;
	for(auto face : subset) {
		for(auto edge : edges(face)) {
			efaces.clear();
			for(auto eface : faces(edge))
				if(selection[eface])
					efaces.emplace_back(eface);
			if(efaces.size() != 2)
				return false;
			auto oface = efaces[efaces[0] == face ? 1 : 0];
			if(!isOneOf(edge.inverse(), edges(oface)))
				return false;
		}
	}

	return true;
}

bool DynamicMesh::representsVolume() const {
	vector<char> visited(faceIdCount(), false);

	for(auto face : faces()) {
		if(visited[face])
			continue;
		auto surface = selectSurface(face);
		if(!isClosedOrientableSurface(surface))
			return false;
		for(auto sface : surface)
			visited[sface] = true;
	}

	return true;
}

int DynamicMesh::eulerPoincare() const {
	vector<EdgeId> all_edges;
	for(auto face : faces())
		insertBack(all_edges, edges(face));
	for(auto &edge : all_edges)
		edge = edge.ordered();
	makeUnique(all_edges);

	return vertexCount() - (int)all_edges.size() + faceCount();
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
		m_faces[index] = {{indices[0], indices[1], indices[2]}};
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

VertexId DynamicMesh::merge(CRange<VertexId> range) {
	float3 sum;
	for(auto vert : range)
		sum += point(vert);
	return merge(range, sum / float(range.size()));
}

VertexId DynamicMesh::merge(CRange<VertexId> range, const float3 &target_pos) {
	VertexId new_vert = addVertex(target_pos);

	vector<FaceId> sel_faces;
	for(auto vert : range)
		insertBack(sel_faces, faces(vert));
	makeUnique(sel_faces);

	for(auto face : sel_faces) {
		auto untouched_verts = setDifference(verts(face), range);

		auto fverts = verts(face);
		remove(face);

		int count = 0;
		for(int i = 0; i < 3; i++)
			if(isOneOf(fverts[i], range)) {
				fverts[i] = new_vert;
				count++;
			}
		if(count == 1)
			addFace(fverts);
	}

	for(auto vert : range)
		remove(vert);

	return VertexId();
}

vector<FaceId> DynamicMesh::inverse(CRange<FaceId> filter) const {
	return setDifference(faces(), filter);
}

vector<VertexId> DynamicMesh::inverse(CRange<VertexId> filter) const {
	return setDifference(verts(), filter);
}

vector<VertexId> DynamicMesh::verts() const {
	vector<VertexId> out;
	out.reserve(m_num_verts);
	for(int i = 0; i < vertexIdCount(); i++)
		if(!isnan(m_verts[i].x))
			out.emplace_back(i);
	return out;
}

vector<VertexId> DynamicMesh::verts(CRange<FaceId> faces) const {
	vector<VertexId> out;
	for(auto face : faces)
		insertBack(out, verts(face));
	makeUnique(out);
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

vector<FaceId> DynamicMesh::selectSurface(FaceId representative) const {
	vector<FaceId> out;

	vector<char> visited(faceIdCount(), 0);

	vector<FaceId> list = {representative};
	while(!list.empty()) {
		FaceId face = list.back();
		list.pop_back();

		if(visited[face])
			continue;
		visited[face] = true;
		out.emplace_back(face);

		for(auto edge : edges(face)) {
			auto efaces = faces(edge);
			if(efaces.size() == 2) {
				auto other_face = efaces[efaces[0] == face ? 1 : 0];
				if(isOneOf(edge.inverse(), edges(other_face)))
					list.emplace_back(other_face);
				continue;
			}
			//			if(efaces.size() == 1)
			//				continue;

			FaceId best_face;
			float min_angle = constant::inf;
			auto proj = edgeProjection(edge, face);
			auto normal = proj.projectVector(triangle(face).normal()).xz();
			DASSERT(fabs(normal.y) > 1.0f - constant::epsilon);

			for(auto eface : efaces) {
				if(eface == face || isOneOf(edge, edges(eface)))
					continue;

				auto vector1 = normalize(proj.project(point(otherVertex(face, edge))).xz());
				auto vector2 = normalize(proj.project(point(otherVertex(eface, edge))).xz());
				if(normal.y < 0.0f)
					swap(vector1, vector2);

				float angle = angleBetween(vector1, float2(0, 0), vector2);
				//				xmlPrint("% - %: ang:% nrm:%\n", vector1, vector2, angle, normal);
				if(angle < min_angle) {
					min_angle = angle;
					best_face = eface;
				}
			}
			if(best_face)
				list.emplace_back(best_face);
		}
	}
	//	printf("Extracted: %d/%d\n", (int)out.size(), (int)faces().size());
	//	xmlPrint("%\n", vector<int>(begin(out), end(out)));

	return out;
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

EdgeId DynamicMesh::faceEdge(FaceId face_id, int sub_id) const {
	DASSERT(isValid(face_id));
	DASSERT(sub_id >= 0 && sub_id < 3);
	const auto &face = m_faces[face_id];
	return EdgeId(VertexId(face[sub_id]), VertexId(face[(sub_id + 1) % 3]));
}

int DynamicMesh::faceEdgeIndex(FaceId face_id, EdgeId edge) const {
	DASSERT(isValid(edge) && isValid(face_id));
	const auto &face = m_faces[face_id];
	for(int i = 0; i < 3; i++)
		if(edge.a == face[i] && edge.b == face[(i + 1) % 3])
			return i;
	return -1;
}

VertexId DynamicMesh::otherVertex(FaceId face_id, EdgeId edge_id) const {
	DASSERT(isValid(face_id));
	DASSERT(isValid(edge_id));
	const auto &face = m_faces[face_id];
	for(int i = 0; i < 3; i++)
		if(!isOneOf(face[i], edge_id.a, edge_id.b))
			return VertexId(face[i]);

	return VertexId(-1);
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

Projection DynamicMesh::edgeProjection(EdgeId edge, FaceId face) const {
	Ray ray(segment(edge));
	float3 far_point = point(otherVertex(face, edge));
	float3 edge_point = closestPoint(ray, far_point);
	return Projection(point(edge.a), normalize(edge_point - far_point), ray.dir());
}

int DynamicMesh::faceCount(VertexId vertex_id) const {
	DASSERT(isValid(vertex_id));
	return (int)m_adjacency[vertex_id].size();
}

DynamicMesh DynamicMesh::merge(CRange<DynamicMesh> meshes) {
	return DynamicMesh(Mesh::merge(vector<Mesh>(begin(meshes), end(meshes))));
}
}
