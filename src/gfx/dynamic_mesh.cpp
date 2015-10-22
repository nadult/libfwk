/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"

namespace fwk {

using VertexId = DynamicMesh::VertexId;
using PolyId = DynamicMesh::PolyId;
using EdgeId = DynamicMesh::EdgeId;
using EdgeLoop = DynamicMesh::EdgeLoop;

DynamicMesh::DynamicMesh(CRange<float3> verts, CRange<vector<uint>> polys, int poly_value)
	: m_num_verts(0), m_num_polys(0) {
	for(auto vert : verts)
		addVertex(vert);
	for(auto poly : polys)
		addPoly(transform<VertexId>(poly), poly_value);
}

DynamicMesh::DynamicMesh(CRange<float3> verts, CRange<array<uint, 3>> tris, int poly_value)
	: m_num_verts(0), m_num_polys(0) {
	for(auto vert : verts)
		addVertex(vert);
	for(auto tri : tris)
		addPoly(VertexId(tri[0]), VertexId(tri[1]), VertexId(tri[2]), poly_value);
}

DynamicMesh::operator Mesh() const {
	vector<int> vert_map(m_verts.size());
	vector<float3> out_verts;

	for(auto vert : verts()) {
		vert_map[vert] = (int)out_verts.size();
		out_verts.emplace_back(point(vert));
	}

	vector<array<uint, 3>> out_tris;
	for(auto poly : polys()) {
		auto pverts = verts(poly);
		// TODO: fixme
		if(pverts.size() != 3)
			continue;
		array<uint, 3> tri;
		for(int i = 0; i < 3; i++)
			tri[i] = vert_map[pverts[i]];
		out_tris.emplace_back(tri);
	}

	return Mesh(std::move(out_verts), {std::move(out_tris)});
}

DynamicMesh DynamicMesh::extract(CRange<PolyId> selection) const {
	auto used_verts = verts(selection);
	vector<int> vert_map(m_verts.size(), -1);
	for(int n = 0; n < (int)used_verts.size(); n++)
		vert_map[used_verts[n].id] = n;

	auto out_verts = transform(used_verts, [this](auto id) { return point(id); });
	vector<vector<uint>> out_polys;
	for(auto poly : selection)
		out_polys.emplace_back(
			transform(verts(poly), [&](auto vert) { return (uint)vert_map[vert]; }));
	return DynamicMesh(std::move(out_verts), std::move(out_polys));
}

vector<DynamicMesh> DynamicMesh::separateSurfaces() const {
	vector<DynamicMesh> out;

	vector<char> selection(polyIdCount(), false);
	for(auto poly : polys()) {
		if(selection[poly])
			continue;
		auto surface = selectSurface(poly);
		out.emplace_back(extract(surface));
		for(auto spoly : surface)
			selection[spoly] = true;
	}
	return out;
}

// More about manifolds: http://www.cs.mtu.edu/~shene/COURSES/cs3621/SLIDES/Mesh.pdf
bool DynamicMesh::isClosedOrientableSurface(CRange<PolyId> subset) const {
	vector<char> selection(polyIdCount(), false);
	for(auto poly : subset)
		selection[poly] = true;

	vector<PolyId> epolys;
	for(auto poly : subset) {
		for(auto edge : edges(poly)) {
			epolys.clear();
			for(auto epoly : polys(edge))
				if(selection[epoly])
					epolys.emplace_back(epoly);
			if(epolys.size() != 2)
				return false;
			auto opoly = epolys[epolys[0] == poly ? 1 : 0];
			if(!isOneOf(edge.inverse(), edges(opoly)))
				return false;
		}
	}

	return true;
}

bool DynamicMesh::representsVolume() const {
	vector<char> visited(polyIdCount(), false);

	for(auto poly : polys()) {
		if(visited[poly])
			continue;
		auto surface = selectSurface(poly);
		if(!isClosedOrientableSurface(surface))
			return false;
		for(auto spoly : surface)
			visited[spoly] = true;
	}

	return true;
}

int DynamicMesh::eulerPoincare() const {
	vector<EdgeId> all_edges;
	for(auto poly : polys())
		insertBack(all_edges, edges(poly));
	for(auto &edge : all_edges)
		edge = edge.ordered();
	makeUnique(all_edges);

	return vertexCount() - (int)all_edges.size() + polyCount();
}

bool DynamicMesh::isTriangular() const {
	for(auto poly : polys())
		if(vertexCount(poly) != 3)
			return false;
	return true;
}

bool DynamicMesh::isValid(VertexId id) const {
	return id.id >= 0 && id.id < (int)m_verts.size() && !isnan(m_verts[id].x);
}
bool DynamicMesh::isValid(PolyId id) const {
	return id.id >= 0 && id.id < (int)m_polys.size() && !m_polys[id].verts.empty();
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

PolyId DynamicMesh::addPoly(CRange<VertexId, 3> indices, int value) {
	for(auto idx : indices)
		DASSERT(isValid(idx));
	DASSERT(indices[0] != indices[1] && indices[1] != indices[2] && indices[2] != indices[0]);

	int index = -1;
	if(m_free_polys.empty()) {
		index = (int)m_polys.size();
		m_polys.emplace_back(Poly{{indices[0], indices[1], indices[2]}, value});
	} else {
		index = m_free_polys.back();
		m_free_polys.pop_back();
		DASSERT(m_polys[index].verts.empty());
		m_polys[index] = Poly{{indices[0], indices[1], indices[2]}, value};
	}

	for(auto i : indices) {
		auto &adjacency = m_adjacency[i];
		int value = index;
		for(auto &adj : adjacency)
			if(adj > value)
				swap(adj, value);
		adjacency.emplace_back(value);
	}

	m_num_polys++;
	return PolyId(index);
}

void DynamicMesh::remove(VertexId id) {
	DASSERT(isValid(id));

	while(!m_adjacency[id].empty())
		remove(PolyId(m_adjacency[id].back()));

	m_verts[id].x = NAN;
	m_free_verts.emplace_back(id);
	m_num_verts--;
}

void DynamicMesh::remove(PolyId id) {
	DASSERT(isValid(id));

	for(auto vidx : m_polys[id].verts) {
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

	m_polys[id].verts.clear();
	m_free_polys.emplace_back(id);
	m_num_polys--;
}

VertexId DynamicMesh::merge(CRange<VertexId> range) {
	float3 sum;
	for(auto vert : range)
		sum += point(vert);
	return merge(range, sum / float(range.size()));
}

VertexId DynamicMesh::merge(CRange<VertexId> range, const float3 &target_pos) {
	VertexId new_vert = addVertex(target_pos);

	vector<PolyId> sel_faces;
	for(auto vert : range)
		insertBack(sel_faces, polys(vert));
	makeUnique(sel_faces);

	for(auto face : sel_faces) {
		auto untouched_verts = setDifference(verts(face), range);

		auto fverts = verts(face);
		auto fvalue = value(face);
		remove(face);

		int count = 0;
		for(int i = 0; i < 3; i++)
			if(isOneOf(fverts[i], range)) {
				fverts[i] = new_vert;
				count++;
			}
		if(count == 1)
			addPoly(fverts, fvalue);
	}

	for(auto vert : range)
		remove(vert);

	return new_vert;
}

void DynamicMesh::split(EdgeId edge, VertexId vert) {
	DASSERT(isValid(edge) && isValid(vert));
	auto efaces = polys(edge);

	for(auto face : polys(edge)) {
		auto fverts = verts(face);
		auto fvalue = value(face);
		int end1id = -1, end2id = -1;

		for(int i = 0; i < 3; i++) {
			if(fverts[i] == edge.a)
				end1id = i;
			if(fverts[i] == edge.b)
				end2id = i;
		}
		DASSERT(end1id != -1 && end2id != -1);

		remove(face);

		fverts[end1id] = vert;
		addPoly(fverts, fvalue);

		fverts[end1id] = edge.a;
		fverts[end2id] = vert;
		addPoly(fverts, fvalue);
	}
}

vector<PolyId> DynamicMesh::inverse(CRange<PolyId> filter) const {
	return setDifference(polys(), filter);
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

vector<VertexId> DynamicMesh::verts(CRange<PolyId> polys) const {
	vector<VertexId> out;
	for(auto poly : polys)
		insertBack(out, verts(poly));
	makeUnique(out);
	return out;
}

vector<VertexId> DynamicMesh::verts(PolyId id) const {
	DASSERT(isValid(id));
	return transform<VertexId>(m_polys[id].verts);
}

array<VertexId, 2> DynamicMesh::verts(EdgeId id) const {
	DASSERT(isValid(id));
	return {{id.a, id.b}};
}

vector<PolyId> DynamicMesh::polys() const {
	vector<PolyId> out;
	out.reserve(m_num_polys);
	for(int i = 0; i < polyIdCount(); i++)
		if(!m_polys[i].verts.empty())
			out.emplace_back(i);
	return out;
}

vector<PolyId> DynamicMesh::coincidentPolys(PolyId id) const {
	DASSERT(isValid(id));
	vector<PolyId> out;
	for(auto vert : verts(id))
		insertBack(out, polys(vert));
	makeUnique(out);
	auto it = std::find(begin(out), end(out), id);
	out.erase(it);
	return out;
}

vector<PolyId> DynamicMesh::polys(VertexId id) const {
	DASSERT(isValid(id));
	return vector<PolyId>(begin(m_adjacency[id]), end(m_adjacency[id]));
}

vector<PolyId> DynamicMesh::polys(EdgeId id) const {
	DASSERT(isValid(id));
	const auto &adj1 = m_adjacency[id.a];
	const auto &adj2 = m_adjacency[id.b];
	vector<int> out(min(adj1.size(), adj2.size()));
	auto it = std::set_intersection(begin(adj1), end(adj1), begin(adj2), end(adj2), begin(out));
	return vector<PolyId>(out.begin(), it);
}

vector<PolyId> DynamicMesh::selectSurface(PolyId representative) const {
	vector<PolyId> out;

	vector<char> visited(polyIdCount(), 0);

	vector<PolyId> list = {representative};
	while(!list.empty()) {
		PolyId face = list.back();
		list.pop_back();

		if(visited[face])
			continue;
		visited[face] = true;
		out.emplace_back(face);

		for(auto edge : edges(face)) {
			auto epolys = polys(edge);
			if(epolys.size() == 2) {
				auto other_face = epolys[epolys[0] == face ? 1 : 0];
				if(isOneOf(edge.inverse(), edges(other_face)))
					list.emplace_back(other_face);
				continue;
			}
			//			if(epolys.size() == 1)
			//				continue;

			PolyId best_face;
			float min_angle = constant::inf;
			auto proj = edgeProjection(edge, face);
			auto normal = proj.projectVector(triangle(face).normal()).xz();
			DASSERT(fabs(normal.y) > 1.0f - constant::epsilon);

			for(auto eface : epolys) {
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
	//	printf("Extracted: %d/%d\n", (int)out.size(), (int)polys().size());
	//	xmlPrint("%\n", vector<int>(begin(out), end(out)));

	return out;
}

vector<EdgeId> DynamicMesh::edges() const {
	vector<EdgeId> out;
	out.reserve(polyCount() * 3);
	for(auto poly : polys())
		for(auto edge : edges(poly))
			out.emplace_back(edge.ordered());
	makeUnique(out);
	return out;
}

vector<EdgeId> DynamicMesh::edges(PolyId id) const {
	DASSERT(isValid(id));
	const auto &poly = m_polys[id].verts;

	vector<EdgeId> out;
	out.reserve(poly.size());
	for(int i = 0, size = (int)poly.size(); i < size; i++)
		out.emplace_back(EdgeId(VertexId(poly[i]), VertexId(poly[(i + 1) % size])));
	return out;
}

vector<EdgeId> DynamicMesh::edges(VertexId id) const {
	DASSERT(isValid(id));
	vector<EdgeId> out;
	for(auto poly : polys(id))
		insertBack(out, edges(poly));
	makeUnique(out);
	return out;
}

EdgeId DynamicMesh::polyEdge(PolyId poly_id, int sub_id) const {
	DASSERT(isValid(poly_id));
	DASSERT(sub_id < vertexCount(poly_id));
	const auto &poly = m_polys[poly_id].verts;
	return EdgeId(VertexId(poly[sub_id]), VertexId(poly[(sub_id + 1) % poly.size()]));
}

int DynamicMesh::polyEdgeIndex(PolyId poly_id, EdgeId edge) const {
	DASSERT(isValid(edge) && isValid(poly_id));
	const auto &poly = m_polys[poly_id].verts;

	for(int i = 0, size = (int)poly.size(); i < size; i++)
		if(edge.a == poly[i] && edge.b == poly[(i + 1) % size])
			return i;
	return -1;
}

VertexId DynamicMesh::otherVertex(PolyId poly_id, EdgeId edge_id) const {
	DASSERT(isValid(poly_id));
	DASSERT(isValid(edge_id));
	const auto &poly = m_polys[poly_id].verts;
	DASSERT(vertexCount(poly_id) == 3);

	for(int i = 0; i < 3; i++)
		if(!isOneOf(poly[i], edge_id.a, edge_id.b))
			return VertexId(poly[i]);

	return VertexId(-1);
}

bool DynamicMesh::coincident(VertexId vert, EdgeId edge) const {
	return isOneOf(vert, edge.a, edge.b);
}

bool DynamicMesh::coincident(EdgeId edge1, EdgeId edge2) const {
	return coincident(edge1.a, edge2) || coincident(edge1.b, edge2);
}

bool DynamicMesh::coincident(VertexId vert, PolyId poly) const {
	DASSERT(isValid(poly));
	return isOneOf(vert, verts(poly));
}

bool DynamicMesh::coincident(EdgeId edge, PolyId poly) const {
	DASSERT(isValid(poly));
	auto fverts = verts(poly);
	return isOneOf(edge.a, fverts) || isOneOf(edge.b, fverts);
}

bool DynamicMesh::coincident(PolyId poly1, PolyId poly2) const {
	DASSERT(isValid(poly1) && isValid(poly2));
	auto fverts2 = verts(poly2);
	for(auto fvert1 : verts(poly1))
		if(isOneOf(fvert1, fverts2))
			return true;
	return false;
}

int DynamicMesh::vertexCount(PolyId poly) const {
	DASSERT(isValid(poly));
	return (int)m_polys[poly].verts.size();
}

vector<PolyId> DynamicMesh::triangulate(PolyId poly_id) {
	DASSERT(isValid(poly_id));
	if(vertexCount(poly_id) == 3)
		return {poly_id};

	auto pverts = verts(poly_id);
	vector<PolyId> out;
	out.reserve((int)pverts.size() - 2);
	auto pvalue = value(poly_id);
	remove(poly_id);

	for(int i = 1, size = pverts.size(); i + 1 < size; i++)
		out.emplace_back(addPoly(pverts[0], pverts[i], pverts[i + 1], pvalue));
	return out;
}

int DynamicMesh::value(PolyId poly) const {
	DASSERT(isValid(poly));
	return m_polys[poly].value;
}

void DynamicMesh::setValue(PolyId poly, int value) {
	DASSERT(isValid(poly));
	m_polys[poly].value = value;
}

Segment DynamicMesh::segment(EdgeId id) const { return Segment(point(id.a), point(id.b)); }

Triangle DynamicMesh::triangle(PolyId id) const {
	DASSERT(isValid(id) && vertexCount(id) == 3);
	const auto &vids = m_polys[id].verts;
	return Triangle(m_verts[vids[0]], m_verts[vids[1]], m_verts[vids[2]]);
}

Projection DynamicMesh::edgeProjection(EdgeId edge, PolyId poly) const {
	Ray ray(segment(edge));
	float3 far_point = point(otherVertex(poly, edge));
	float3 edge_point = closestPoint(ray, far_point);
	return Projection(point(edge.a), normalize(edge_point - far_point), ray.dir());
}

int DynamicMesh::polyCount(VertexId vertex_id) const {
	DASSERT(isValid(vertex_id));
	return (int)m_adjacency[vertex_id].size();
}

DynamicMesh DynamicMesh::merge(CRange<DynamicMesh> meshes) {
	// TODO: values are ignored...
	return DynamicMesh(Mesh::merge(vector<Mesh>(begin(meshes), end(meshes))));
}
}
