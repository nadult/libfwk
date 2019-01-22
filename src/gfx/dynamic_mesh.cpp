// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/dynamic_mesh.h"

#include "fwk/enum_flags.h"
#include "fwk/format.h"
#include "fwk/math/projection.h"
#include "fwk/math/ray.h"
#include "fwk_index_range.h"

namespace fwk {

using VertexId = DynamicMesh::VertexId;
using PolyId = DynamicMesh::PolyId;
using EdgeId = DynamicMesh::EdgeId;

string DynamicMesh::Simplex::print(const DynamicMesh &mesh) const {
	TextFormatter out;
	out("(");
	for(int i = 0; i < m_size; i++) {
		auto pt = mesh.point(m_verts[i]);
		out("%:%:%%", pt.x, pt.y, pt.z, i + 1 == m_size ? ')' : ' ');
	}
	return (string)out.text();
}

DynamicMesh::DynamicMesh(CSpan<float3> verts, CSpan<vector<int>> polys, int poly_value)
	: m_num_verts(0), m_num_polys(0) {
	for(auto vert : verts)
		addVertex(vert);
	for(auto poly : polys)
		addPoly(transform<VertexId>(poly), poly_value);
}

DynamicMesh::DynamicMesh(CSpan<float3> verts, CSpan<array<int, 3>> tris, int poly_value)
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

	vector<array<int, 3>> out_tris;
	for(auto poly : polys()) {
		auto pverts = verts(poly);
		// TODO: fixme
		if(pverts.size() != 3)
			continue;
		array<int, 3> tri;
		for(int i = 0; i < 3; i++)
			tri[i] = vert_map[pverts[i]];
		out_tris.emplace_back(tri);
	}

	return Mesh(std::move(out_verts), {std::move(out_tris)});
}

DynamicMesh DynamicMesh::extract(CSpan<PolyId> selection) const {
	auto used_verts = verts(selection);
	vector<int> vert_map(m_verts.size(), -1);
	for(int n = 0; n < (int)used_verts.size(); n++)
		vert_map[used_verts[n].id] = n;

	vector<float3> out_verts;
	for(auto id : used_verts)
		out_verts.emplace_back(point(id));
	vector<vector<int>> out_polys;
	for(auto poly : selection)
		out_polys.emplace_back(transform(verts(poly), [&](auto vert) { return vert_map[vert]; }));
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
bool DynamicMesh::isClosedOrientableSurface(CSpan<PolyId> subset) const {
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
	makeSortedUnique(all_edges);

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
	return id.id >= 0 && id.id < (int)m_polys.size() && m_polys[id].verts;
}
bool DynamicMesh::isValid(EdgeId id) const {
	return isValid(id.a) && isValid(id.b) && id.a != id.b;
}

VertexId DynamicMesh::addVertex(const float3 &pos) {
	DASSERT(!isnan(pos));

	int index = -1;
	if(m_free_verts) {
		index = m_free_verts.back();
		m_free_verts.pop_back();
		m_verts[index] = pos;
		DASSERT(!m_adjacency[index]);
	} else {
		index = (int)m_verts.size();
		m_verts.emplace_back(pos);
		m_adjacency.emplace_back(vector<int>());
	}

	m_num_verts++;
	return VertexId(index);
}

PolyId DynamicMesh::addPoly(CSpan<VertexId, 3> indices, int value) {
	for(int i1 = 0; i1 < (int)indices.size(); i1++) {
		DASSERT(isValid(indices[i1]));
		for(int i2 = i1 + 1; i2 < (int)indices.size(); i2++)
			DASSERT(indices[i1] != indices[i2]);
	}

	int index = -1;
	if(m_free_polys) {
		index = m_free_polys.back();
		m_free_polys.pop_back();
		DASSERT(!m_polys[index].verts);
		m_polys[index] = Poly{vector<int>(begin(indices), end(indices)), value};
	} else {
		index = (int)m_polys.size();
		m_polys.emplace_back(Poly{vector<int>(begin(indices), end(indices)), value});
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

	while(m_adjacency[id])
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

VertexId DynamicMesh::merge(CSpan<VertexId> range) {
	float3 sum;
	for(auto vert : range)
		sum += point(vert);
	return merge(range, sum / float(range.size()));
}

VertexId DynamicMesh::merge(CSpan<VertexId> range, const float3 &target_pos) {
	VertexId new_vert = addVertex(target_pos);

	vector<PolyId> sel_polys;
	for(auto vert : range)
		insertBack(sel_polys, polys(vert));
	makeSortedUnique(sel_polys);

	for(auto poly : sel_polys) {
		auto untouched_verts = setDifference(verts(poly), range);

		auto pverts = verts(poly);
		auto pvalue = value(poly);
		remove(poly);

		int count = 0;
		for(auto &pvert : pverts)
			if(isOneOf(pvert, range)) {
				pvert = new_vert;
				count++;
			}
		DASSERT(count > 0);

		std::rotate(begin(pverts), std::find(begin(pverts), end(pverts), new_vert), end(pverts));
		auto it = begin(pverts);
		DASSERT(*it == new_vert);

		while(it != end(pverts)) {
			auto next = std::find(it + 1, end(pverts), new_vert);
			if(next - it >= 3)
				addPoly(vector<VertexId>(it, next), pvalue);
			it = next;
		}
	}

	for(auto vert : range)
		remove(vert);

	return new_vert;
}

void DynamicMesh::split(EdgeId edge, VertexId vert) {
	DASSERT(isValid(edge) && isValid(vert));
	DASSERT(!coincident(vert, edge));

	auto epolys = polys(edge);
	auto &vadjacency = m_adjacency[vert];
	print("split %-%: v:%\n", (int)edge.a, (int)edge.b, (int)vert);

	for(auto poly_id : epolys) {
		auto pverts = m_polys[poly_id].verts;
		if(isOneOf<int>(vert, pverts)) {
			remove(poly_id);
			continue;
		}
		print("Splitting: %\n", transform<int>(pverts));

		int idx = -1;

		for(int i = 0, size = (int)pverts.size(); i < size; i++) {
			int cur = pverts[i], next = pverts[(i + 1) % size];
			if(isOneOf(cur, edge.a, edge.b) && isOneOf(next, edge.a, edge.b)) {
				idx = i + 1;
				break;
			}
		}

		if(idx == -1) // TODO: make sure it doesn't happen
			continue;
		DASSERT(idx != -1);

		pverts.insert(begin(pverts) + idx, vert);
		vadjacency.emplace_back(poly_id);
	}
}

void DynamicMesh::move(VertexId vertex_id, const float3 &new_pos) {
	DASSERT(isValid(vertex_id));
	m_verts[vertex_id] = new_pos;
}

vector<PolyId> DynamicMesh::inverse(CSpan<PolyId> filter) const {
	return setDifference(polys(), filter);
}

vector<VertexId> DynamicMesh::inverse(CSpan<VertexId> filter) const {
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

vector<VertexId> DynamicMesh::verts(CSpan<PolyId> polys) const {
	vector<VertexId> out;
	for(auto poly : polys)
		insertBack(out, verts(poly));
	makeSortedUnique(out);
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
		if(m_polys[i].verts)
			out.emplace_back(i);
	return out;
}

vector<PolyId> DynamicMesh::coincidentPolys(PolyId id) const {
	DASSERT(isValid(id));
	vector<PolyId> out;
	for(auto vert : verts(id))
		insertBack(out, polys(vert));
	makeSortedUnique(out);
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
	vector<PolyId> out;
	for(auto poly : polys(id.a)) {
		auto pedges = edges(poly);
		if(isOneOf(id, pedges) || isOneOf(id.inverse(), pedges))
			out.emplace_back(poly);
	}
	return out;
}

vector<PolyId> DynamicMesh::selectSurface(PolyId representative) const {
	vector<PolyId> out;

	vector<char> visited(polyIdCount(), 0);

	vector<PolyId> list = {representative};
	while(list) {
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
			float min_angle = fconstant::inf;
			auto proj = edgeProjection(edge, face);
			auto normal = proj.projectVector(triangle(face).normal()).xz();
			DASSERT(fabs(normal.y) > 1.0f - fconstant::epsilon);

			for(auto eface : epolys) {
				if(eface == face || isOneOf(edge, edges(eface)))
					continue;

				auto vector1 = normalize(proj.project(point(otherVertex(face, edge))).xz());
				auto vector2 = normalize(proj.project(point(otherVertex(eface, edge))).xz());
				if(normal.y < 0.0f)
					swap(vector1, vector2);

				float angle = angleTowards(vector1, float2(0, 0), vector2);
				//				print("% - %: ang:% nrm:%\n", vector1, vector2, angle, normal);
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
	//	print("%\n", vector<int>(begin(out), end(out)));

	return out;
}

vector<EdgeId> DynamicMesh::edges() const {
	vector<EdgeId> out;
	out.reserve(polyCount() * 3);
	for(auto poly : polys())
		for(auto edge : edges(poly))
			out.emplace_back(edge.ordered());
	makeSortedUnique(out);
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
	makeSortedUnique(out);
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

	vector<char> duplicates(pverts.size(), 0);
	int first_duplicate = -1;
	for(int i = 0; i < (int)pverts.size(); i++)
		for(int j = i + 1; j < (int)pverts.size(); j++)
			if(pverts[i] == pverts[j]) {
				if(first_duplicate == -1)
					first_duplicate = i;
				duplicates[i] = 1;
				duplicates[j] = 1;
			}

	if(first_duplicate != -1) {
		std::rotate(begin(pverts), begin(pverts) + first_duplicate, end(pverts));
		std::rotate(begin(duplicates), begin(duplicates) + first_duplicate, end(duplicates));
	}

	int first_vert = 0;
	for(int i = 1, size = pverts.size(); i + 1 < size; i++) {
		if(duplicates[i])
			first_vert = i;
		array<VertexId, 3> verts = {{pverts[first_vert], pverts[i], pverts[i + 1]}};
		if(distinct(verts))
			out.emplace_back(addPoly(verts, pvalue));
	}
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

Segment3<float> DynamicMesh::segment(EdgeId id) const { return {point(id.a), point(id.b)}; }

FBox DynamicMesh::box(EdgeId id) const {
	auto p1 = point(id.a), p2 = point(id.b);
	return FBox(vmin(p1, p2), vmax(p1, p2));
}

Triangle3F DynamicMesh::triangle(PolyId id) const {
	DASSERT(isValid(id) && vertexCount(id) == 3);
	const auto &vids = m_polys[id].verts;
	return Triangle3F(m_verts[vids[0]], m_verts[vids[1]], m_verts[vids[2]]);
}

Projection DynamicMesh::edgeProjection(EdgeId edge, PolyId poly) const {
	DASSERT(!segment(edge).empty());
	auto ray = *segment(edge).asRay();
	float3 far_point = point(otherVertex(poly, edge));
	float3 edge_point = ray.closestPoint(far_point);
	return Projection(point(edge.a), normalize(edge_point - far_point), ray.dir());
}

int DynamicMesh::polyCount(VertexId vertex_id) const {
	DASSERT(isValid(vertex_id));
	return (int)m_adjacency[vertex_id].size();
}

DynamicMesh DynamicMesh::merge(CSpan<DynamicMesh> meshes) {
	// TODO: values are ignored...
	return DynamicMesh(Mesh::merge(vector<Mesh>(begin(meshes), end(meshes))));
}

float3 closestPoint(const DynamicMesh &mesh, const float3 &point) {
	float min_dist_sq = fconstant::inf;
	float3 out;

	for(auto poly : mesh.polys()) {
		auto ppoint = mesh.triangle(poly).closestPoint(point);
		auto dist_sq = distanceSq(point, ppoint);
		if(dist_sq < min_dist_sq) {
			min_dist_sq = dist_sq;
			out = ppoint;
		}
	}

	return out;
}
}
