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

TetMesh TetMesh::extract(CRange<int> indices) const {
	DASSERT(isValidSelection(indices));

	vector<int> vert_map(m_verts.size(), -1);
	vector<float3> new_verts;

	for(auto idx : indices) {
		for(auto i : m_tet_verts[idx])
			if(vert_map[i] == -1) {
				vert_map[i] = (int)new_verts.size();
				new_verts.emplace_back(m_verts[i]);
			}
	}

	vector<TetIndices> new_tets;
	new_tets.reserve(indices.size());
	for(auto idx : indices) {
		TetIndices new_tet;
		for(int i = 0; i < 4; i++)
			new_tet[i] = vert_map[m_tet_verts[idx][i]];
		new_tets.emplace_back(new_tet);
	}

	return TetMesh(vector<float3>(begin(new_verts), end(new_verts)), std::move(new_tets));
}

vector<int> TetMesh::selection(const FBox &box) const {
	vector<int> out;
	for(int t = 0; t < size(); t++)
		if(areIntersecting(makeTet(t), box))
			out.emplace_back(t);
	return out;
}

vector<int> TetMesh::invertSelection(CRange<int> range) const {
	DASSERT(isValidSelection(range));

	vector<int> all(size());
	std::iota(begin(all), end(all), 0);
	return setDifference(all, range);
}

bool TetMesh::isValidSelection(CRange<int> range) const {
	int prev = -1;
	for(auto idx : range) {
		if(idx == prev || idx < 0 || idx >= size())
			return false;
		prev = idx;
	}
	return true;
}

FBox TetMesh::computeBBox() const { return FBox(m_verts); }

using Tet = HalfTetMesh::Tet;
using Face = HalfTetMesh::Face;
using Vertex = HalfTetMesh::Vertex;
using Edge = HalfTetMesh::Edge;

struct Isect {
	Face *face_a, *face_b;
	Segment segment;
};

struct FaceEdgeInfo {
	vector<Edge> edges;
	std::map<Vertex *, int> border_verts;
};

using Loop = vector<Edge>;
using FaceEdgeMap = std::map<Face *, FaceEdgeInfo>;

int findClosestEdge(Face *face, Vertex *vert) {
	if(isOneOf(vert, face->verts()))
		return -1;

	int closest = -1;
	float closest_dist = constant::inf;
	auto edges = face->edges();

	for(int i = 0; i < 3; i++) {
		auto edge = edges[i];
		float dist = distance(edge.segment(), vert->pos());
		if(dist < closest_dist) {
			closest = i;
			closest_dist = dist;
		}
	}

	if(closest_dist < constant::epsilon)
		return closest;
	return -1;
}

void updateBorderVert(FaceEdgeMap &map, Face *face, Vertex *vert) {
	int edge_id = findClosestEdge(face, vert);

	if(edge_id != -1) {
		map[face].border_verts[vert] = edge_id;
		Face *other = face->boundaryNeighbours()[edge_id];
		DASSERT(other);

		int other_id = other->edgeId(face->edges()[edge_id]);
		DASSERT(other_id != -1);
		map[other].border_verts[vert] = other_id;
	}
}

vector<Edge> findVerts(const vector<Isect> &isects, HalfTetMesh &mesh) {
	vector<Edge> out;
	for(const auto &isect : isects) {
		DASSERT(distance(isect.segment.origin(), isect.segment.end()) > constant::epsilon);
		Vertex *f1 = mesh.findVertex(isect.segment.origin());
		Vertex *f2 = mesh.findVertex(isect.segment.end());
		Vertex *v1 = f1 ? f1 : mesh.addVertex(isect.segment.origin());
		Vertex *v2 = f2 ? f2 : mesh.addVertex(isect.segment.end());
		out.emplace_back(v1, v2);
	}
	return out;
}

void makeEdgesUnique(vector<Edge> &edges) {
	for(int e1 = 0; e1 < (int)edges.size(); e1++)
		for(int e2 = e1 + 1; e2 < (int)edges.size(); e2++)
			if(edges[e2] == edges[e1] || edges[e2].inverse() == edges[e1]) {
				edges[e2--] = edges.back();
				edges.pop_back();
			}
}

void extractLoops(const vector<Isect> &isects, HalfTetMesh &hmesh1, HalfTetMesh &hmesh2,
				  vector<Edge> &loops1, vector<Edge> &loops2, FaceEdgeMap &map1,
				  FaceEdgeMap &map2) {
	loops1 = findVerts(isects, hmesh1);
	loops2 = findVerts(isects, hmesh2);

	for(int i = 0; i < (int)isects.size(); i++) {
		const auto &cur = isects[i];
		map1[cur.face_a].edges.emplace_back(loops1[i].a, loops1[i].b);
		map2[cur.face_b].edges.emplace_back(loops2[i].a, loops2[i].b);
		updateBorderVert(map1, cur.face_a, loops1[i].a);
		updateBorderVert(map1, cur.face_a, loops1[i].b);
		updateBorderVert(map2, cur.face_b, loops2[i].a);
		updateBorderVert(map2, cur.face_b, loops2[i].b);
	}

	for(auto &it : map1)
		makeEdgesUnique(it.second.edges);
	for(auto &it : map2)
		makeEdgesUnique(it.second.edges);
}

void saveSvg(vector<float2> points, vector<Segment2D> segs, vector<Triangle2D> tris, int id,
			 float scale) {
	XMLDocument doc;
	auto svg_node = doc.addChild("svg");
	auto cnode = svg_node.addChild("g");
	cnode.addAttrib("render-order", -1);
	cnode.addAttrib("style", "stroke-width:3;stroke:black");
	float2 tmin, tmax;
	for(auto tri : tris)
		for(int i = 0; i < 3; i++) {
			tmin = min(tmin, tri[i]);
			tmax = max(tmax, tri[i]);
		}
	/*	for(auto pt : points) {
			tmin = min(tmin, pt);
			tmax = max(tmax, pt);
		}
		for(auto seg : segs) {
			tmin = min(tmin, min(seg.start, seg.end));
			tmax = max(tmax, max(seg.start, seg.end));
		}*/

	tmin *= scale;
	tmax *= scale;

	svg_node.addAttrib("width", tmax.x - tmin.x + 100.0f);
	svg_node.addAttrib("height", tmax.y - tmin.y + 100.0f);

	float2 offset = -tmin + float2(50, 50);

	for(auto pt : points) {
		auto vert = cnode.addChild("circle");
		vert.addAttrib("cx", pt.x * scale + offset.x);
		vert.addAttrib("cy", pt.y * scale + offset.y);
		vert.addAttrib("r", 4);
	}

	auto lnode = svg_node.addChild("g");
	lnode.addAttrib("style", "stroke-width:1.5;stroke:black;"
							 "stroke-linecap:square;"
							 "stroke-linejoin:miter;"
							 "stroke-miterlimit:10;"
							 "stroke-dasharray:none;"
							 "stroke-dashoffset:0");

	for(int s = 0; s < (int)segs.size(); s++) {
		const auto &seg = segs[s];
		auto line = lnode.addChild("line");
		line.addAttrib("x1", seg.start.x * scale + offset.x);
		line.addAttrib("y1", seg.start.y * scale + offset.y);
		line.addAttrib("x2", seg.end.x * scale + offset.x);
		line.addAttrib("y2", seg.end.y * scale + offset.y);
		auto center = (seg.start + seg.end) * 0.5f * scale + offset;
		string value = format("seg %d", s);
		auto text = lnode.addChild("text", lnode.own(value));
		text.addAttrib("x", center.x);
		text.addAttrib("y", center.y);
		text.addAttrib("text-anchor", "middle");
		text.addAttrib("font-size", "16px");
		text.addAttrib("stroke-width", "1");
	}

	auto tnode = svg_node.addChild("g");
	tnode.addAttrib("render-order", 1);

	for(auto tri : tris) {
		float2 p[3] = {tri[0] * scale + offset, tri[1] * scale + offset, tri[2] * scale + offset};
		float2 center = (p[0] + p[1] + p[2]) / 3.0f;
		//		for(auto &pt : p)
		//			pt = lerp(pt, center, 0.01f);
		Triangle t3{float3(p[0], 0.0f), float3(p[1], 0.0f), float3(p[2], 0.0f)};
		float area = t3.area();

		auto poly = tnode.addChild("polygon");
		string points = format("%f,%f %f,%f %f,%f", p[0].x, p[0].y, p[1].x, p[1].y, p[2].x, p[2].y);
		poly.addAttrib("points", points);
		poly.addAttrib("style", "stroke-width:2.5;fill:red;stroke:blue;fill-opacity:0.4");
	}

	Saver(format("temp/file%d.svg", id)) << doc;
}

float angleBetween(Vertex *vprev, Vertex *vcur, Vertex *vnext, const Projection &proj) {
	DASSERT(vprev && vcur && vnext);
	DASSERT(vprev != vcur && vcur != vnext);

	float2 cur = (proj * vcur->pos()).xz();
	float2 prev = (proj * vprev->pos()).xz();
	float2 next = (proj * vnext->pos()).xz();
	return angleBetween(prev, cur, next);
}

vector<float> computeAngles(const vector<Vertex *> &verts, const Projection &proj) {
	vector<float> out(verts.size());

	for(int n = 0, count = (int)verts.size(); n < count; n++) {
		auto *prev = verts[(n + count - 1) % count];
		auto *next = verts[(n + 1) % count];
		out[n] = angleBetween(prev, verts[n], next, proj);
	}
	return out;
}

// bedges: border edges
// iedges: inside edges
vector<vector<Edge>> findSimplePolygons(const vector<Edge> &bedges, const vector<Edge> &iedges,
										const Projection &proj, bool do_print = false) {
	if(do_print) {
		xmlPrint("bedges: ");
		for(auto e : bedges)
			xmlPrint("(%-%)[%:%] ", (long long)e.a % 1337, (long long)e.b % 1337,
					 (proj * e.a->pos()).xz(), (proj * e.b->pos()).xz());
		xmlPrint("\n");
		xmlPrint("iedges: ");
		for(auto e : iedges)
			xmlPrint("(%-%)[%:%] ", (long long)e.a % 1337, (long long)e.b % 1337,
					 (proj * e.a->pos()).xz(), (proj * e.b->pos()).xz());
		xmlPrint("\n");
	}

	std::map<Vertex *, vector<Edge>> map;
	for(auto bedge : bedges)
		map[bedge.a].emplace_back(bedge);
	for(auto iedge : iedges) {
		map[iedge.a].emplace_back(iedge);
		map[iedge.b].emplace_back(iedge.inverse());
	}

	// Creating bridges between unconnected edges
	while(true) {
		vector<Vertex *> stack = {map.begin()->first};
		for(auto &elem : map)
			elem.first->setTemp(0);

		int count = 0;
		while(!stack.empty()) {
			Vertex *vert = stack.back();
			stack.pop_back();
			if(vert->temp())
				continue;

			vert->setTemp(1);
			count++;

			for(auto edge : map[vert])
				stack.push_back(edge.a == vert ? edge.b : edge.a);
		}

		if(count == (int)map.size())
			break;

		Edge best_edge;
		float max_distance = 0.0f; // TODO: is it a good idea?

		for(const auto &it1 : map)
			if(it1.first->temp() == 1)
				for(const auto &it2 : map)
					if(it2.first->temp() == 0) {
						Edge edge(it1.first, it2.first);

						float dist = constant::inf;
						for(const auto &it3 : map)
							for(auto tedge : it3.second)
								if(!tedge.hasSharedEnds(edge))
									dist = min(dist, distance(edge.segment(), tedge.segment()));
						if(dist > max_distance) {
							best_edge = edge;
							max_distance = dist;
						}
					}

		if(do_print)
			xmlPrint("Adding bridge: %-%\n", (long long)best_edge.a % 1337,
					 (long long)best_edge.b % 1337);
		DASSERT(best_edge.isValid());
		DASSERT(max_distance >= constant::epsilon); // TODO: check if this could happen
		map[best_edge.a].emplace_back(best_edge);
		map[best_edge.b].emplace_back(best_edge.inverse());
	}

	vector<vector<Edge>> out;

	while(!map.empty()) {
		auto it = map.begin();
		DASSERT(!it->second.empty());
		Edge start = it->second.back();
		it->second.pop_back();
		if(it->second.empty())
			map.erase(it);

		vector<Edge> loop = {start};
		if(do_print)
			xmlPrint("Start: %-%\n", (long long)start.a % 1337, (long long)start.b % 1337);

		while(true) {
			Vertex *current = loop.back().b;
			auto it = map.find(current);

			if(it == map.end()) {
				DASSERT(current == start.a);
				break;
			}

			float min_angle = constant::inf;
			Edge best_edge;

			for(auto edge : it->second) {
				if(edge.b == loop.back().a)
					continue;
				float angle = angleBetween(loop.back().a, current, edge.b, proj);

				if(do_print)
					xmlPrint("Consider: %-% (%)\n", (long long)edge.a % 1337,
							 (long long)edge.b % 1337, radToDeg(angle));
				if(angle < min_angle) {
					min_angle = angle;
					best_edge = edge;
				}
			}

			if(current == start.a && loop.back().a != start.b) {
				float start_angle = angleBetween(loop.back().a, current, start.b, proj);
				if(do_print)
					xmlPrint("Consider start: (%)\n", radToDeg(start_angle));
				if(start_angle < min_angle)
					break;
			}
			if(do_print)
				xmlPrint("Added: %-%\n", (long long)best_edge.a % 1337,
						 (long long)best_edge.b % 1337);

			if(!best_edge.isValid()) {
				if(!do_print)
					findSimplePolygons(bedges, iedges, proj, true);
				THROW("Invalid topology");
			}
			DASSERT(best_edge.isValid());
			for(int i = 0; i < (int)it->second.size(); i++)
				if(it->second[i] == best_edge) {
					it->second[i] = it->second.back();
					it->second.pop_back();
					break;
				}
			if(it->second.empty())
				map.erase(it);

			loop.emplace_back(best_edge);
		}
		out.emplace_back(loop);
	}

	if(do_print) {
		printf("Simple polygons: %d\n", (int)out.size());
		for(auto poly : out) {
			printf("Poly: ");
			for(auto e : poly)
				xmlPrint("%-% ", (long long)e.a % 1337, (long long)e.b % 1337);
			printf("\n");
		}
		printf("\n");
	}

	return out;
}

// Simple ear-clipping algorithm from: http://arxiv.org/pdf/1212.6038.pdf
// TODO: avoid slivers
// TODO: add support for holes (when loop is contained in single triangle)
vector<array<Vertex *, 3>> triangulateSimplePolygon(const vector<Edge> &edges,
													const Projection &proj) {
	vector<array<Vertex *, 3>> out;

	vector<Vertex *> verts(edges.size(), nullptr);
	for(int e = 0; e < (int)edges.size(); e++) {
		DASSERT(edges[e].b == edges[(e + 1) % edges.size()].a);
		verts[e] = edges[e].a;
	}

	vector<float> angles = computeAngles(verts, proj);
	float angle_sum = std::accumulate(begin(angles), end(angles), 0.0f);

	// Making sure, verts are in CW order
	if(fabs(angle_sum - constant::pi * ((int)verts.size() - 2)) > 0.01f) {
		std::reverse(begin(verts), end(verts));
		std::reverse(begin(angles), end(angles));
		for(auto &ang : angles)
			ang = constant::pi * 2.0 - ang;
		angle_sum = std::accumulate(begin(angles), end(angles), 0.0f);
	}

	/*	printf("Verts: (%f exp:%f)\n", radToDeg(angle_sum),
			   radToDeg(constant::pi * (verts.size() - 2)));
		for(int n = 0; n < (int)verts.size(); n++)
			xmlPrint("%: %\n", (proj * verts[n]->pos()).xz(), radToDeg(angles[n]));*/
	DASSERT(fabs(angle_sum - constant::pi * ((int)verts.size() - 2)) < 0.01f);

	while(out.size() + 2 < edges.size()) {
		vector<float> angles = computeAngles(verts, proj);

		int best_vert = -1;
		float best_angle = constant::pi;
		std::array<Vertex *, 3> best_ear;

		for(int n = 0, count = (int)verts.size(); n < count; n++) {
			if(angles[n] > best_angle)
				continue;

			Vertex *cur = verts[n];
			Vertex *prev = verts[(n + count - 1) % count];
			Vertex *next = verts[(n + 1) % count];
			Triangle2D new_tri((proj * prev->pos()).xz(), (proj * cur->pos()).xz(),
							   (proj * next->pos()).xz());

			float min_dist = constant::inf;
			for(int i = 0; i < count; i++)
				if(!isOneOf(verts[i], cur, prev, next))
					min_dist = min(min_dist, distance(new_tri, (proj * verts[i]->pos()).xz()));

			// TODO: try to minimize min_dist?
			if(min_dist > constant::epsilon) {
				best_vert = n;
				best_angle = angles[n];
				best_ear = {{prev, cur, next}};
			}
		}
		DASSERT(best_vert != -1);

		out.emplace_back(best_ear);
		verts.erase(verts.begin() + best_vert);
	}

	return out;
}

vector<array<Vertex *, 3>> triangulateFace(Face *face, const vector<Edge> &bedges,
										   const vector<Edge> &iedges) {
	Projection proj(face->triangle());

	vector<vector<Edge>> simple_polys = findSimplePolygons(bedges, iedges, proj);

	vector<array<Vertex *, 3>> out;
	for(const auto &poly : simple_polys)
		insertBack(out, triangulateSimplePolygon(poly, proj));
	return out;
}

// When more than one triangulated face belong to single tetrahedron,
// then this tet should be subdivided
void prepareMesh(HalfTetMesh &mesh, FaceEdgeMap &map) {
	vector<Tet *> split_tets;

	int edge_count = 0;
	for(auto *face : mesh.faces())
		face->setTemp(0);
	for(const auto &it : map) {
		it.first->setTemp(1);
		edge_count += (int)it.second.edges.size();
	}

	std::map<Face *, Face *> face_map;

	for(auto *tet : mesh.tets()) {
		int count = 0;
		for(auto *face : tet->faces())
			count += face->temp();

		// TODO: this shouldn't be required; in general count > 1 should be sufficient
		if(count >= 1) {
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

	FaceEdgeMap new_map;
	for(auto old_elem : map) {
		Face *face = old_elem.first;
		auto remap_it = face_map.find(face);
		if(remap_it != face_map.end())
			face = remap_it->second;
		new_map[face] = old_elem.second;
	}
	new_map.swap(map);
	// TODO: this should work...
	/*
		for(auto it : face_map) {
			auto mit = map.find(it.first);
			if(mit != map.end()) {
				map[it.second] = mit->second;
				map.erase(mit);
			}
		}*/

	int after_edge_count = 0;
	for(auto it : map) {
		after_edge_count += (int)it.second.edges.size();
		for(auto bv : it.second.border_verts)
			DASSERT(distance(it.first->edges()[bv.second].segment(), bv.first->pos()) <
					constant::epsilon);
	}
	DASSERT(edge_count == after_edge_count);
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

void triangulateMesh(HalfTetMesh &mesh, FaceEdgeMap &map, TetMesh::CSGVisualData *vis_data,
					 int mesh_id) {
	prepareMesh(mesh, map);

	/*	if(vis_data && vis_data->phase == 1 && mesh_id == 1) {
			vector<Triangle> tris;
			for(auto loop : loops)
				for(auto elem : loop) {
					auto verts = elem.face->verts();
					tris.emplace_back(verts[0]->pos(), verts[1]->pos(), verts[2]->pos());
				}
			vis_data->poly_soups.emplace_back(Color::red, tris);
		}*/

	struct SplitInfo {
		Edge edge;
		vector<Vertex *> splits;
	};

	vector<pair<Vertex *, vector<array<Vertex *, 3>>>> face_triangulations;
	vector<SplitInfo> edge_splits;
	vector<Tet *> rem_tets;

	for(auto pair : map) {
		auto *face = pair.first;
		auto verts = pair.first->verts();
		auto edges = face->edges();
		auto &border_verts = pair.second.border_verts;

		array<vector<Vertex *>, 3> edge_verts;
		rem_tets.emplace_back(face->tet());

		for(auto bvert : border_verts) {
			DASSERT(bvert.second >= 0 && bvert.second < 3);
			float dist = distance(edges[bvert.second].segment(), bvert.first->pos());
			DASSERT(dist < constant::epsilon);
			edge_verts[bvert.second].emplace_back(bvert.first);
		}

		vector<Edge> tri_boundary_edges, tri_inside_edges;
		for(int i = 0; i < 3; i++) {
			auto edge = face->edges()[i];
			if(edge_verts[i].empty()) {
				tri_boundary_edges.emplace_back(edge.a, edge.b);
				continue;
			}

			auto splits = sortEdgeVerts(edge, edge_verts[i]);
			edge_splits.emplace_back(SplitInfo{face->edges()[i], splits});

			tri_boundary_edges.emplace_back(edge.a, splits.front());
			//			DASSERT(distance(edge.segment(), splits.front()->pos()) <
			// constant::epsilon);
			for(int i = 0; i + 1 < (int)splits.size(); i++) {
				tri_boundary_edges.emplace_back(splits[i], splits[i + 1]);
				DASSERT(distance(edge.segment(), splits[i]->pos()) < constant::epsilon);
			}
			tri_boundary_edges.emplace_back(splits.back(), edge.b);
		}

		for(int i = 0; i < 3; i++)
			insertBack(edge_verts[i], {face->edges()[i].a, face->edges()[i].b});

		for(auto edge : pair.second.edges) {
			bool on_border = false;
			for(int i = 0; i < 3; i++)
				if(isOneOf(edge.a, edge_verts[i]) && isOneOf(edge.b, edge_verts[i]))
					on_border = true;
			if(!on_border)
				tri_inside_edges.emplace_back(edge.ordered());
		}

		auto other_vert = setDifference(face->tet()->verts(), face->verts());
		DASSERT(other_vert.size() == 1);

		//		printf("triangulating face (i:%d b:%d)\n", (int)tri_inside_edges.size(),
		//			   (int)tri_boundary_edges.size());
		auto triangles = triangulateFace(face, tri_boundary_edges, tri_inside_edges);
		//		printf("new tris: %d\n", (int)triangles.size());

		if(vis_data && vis_data->phase == 1) {
			Color col = mesh_id % 2 ? Color::yellow : Color::blue;
			vector<Segment> edges;
			for(auto tri : triangles)
				for(int i = 0; i < 3; i++)
					edges.emplace_back(tri[i]->pos(), tri[(i + 1) % 3]->pos());
			vis_data->segment_groups.emplace_back(col, edges);
		}

		float area_sum = 0.0f;
		for(auto t : triangles)
			area_sum += Triangle(t[0]->pos(), t[1]->pos(), t[2]->pos()).area();
		DASSERT(fabs(area_sum - face->triangle().area()) < constant::epsilon);

		face_triangulations.emplace_back(other_vert.front(), std::move(triangles));
	}

	float old_volume = 0.0f;
	makeUnique(rem_tets);
	for(auto *tet : rem_tets) {
		old_volume += tet->tet().volume();
		mesh.removeTet(tet);
	}

	// TODO: better ordering
	std::sort(begin(edge_splits), end(edge_splits), [](const auto &s1, const auto &s2) {
		float3 c1 = s1.edge.a->pos() + s1.edge.b->pos();
		float3 c2 = s2.edge.a->pos() + s2.edge.b->pos();
		return c1.x + c1.y + c1.z < c2.x + c2.y + c2.z;
	});

	for(const auto &split : edge_splits)
		mesh.subdivideEdge(split.edge.a, split.edge.b, split.splits);

	float new_volume = 0.0f;
	for(const auto &tring : face_triangulations)
		for(auto face_verts : tring.second) {
			auto *tet = mesh.addTet(face_verts[0], face_verts[1], face_verts[2], tring.first);
			new_volume += tet->tet().volume();
		}
	DASSERT(fabs(new_volume - old_volume) < constant::epsilon);

	if(vis_data && vis_data->phase == 1 && mesh_id == 1 && 0) {
		vector<Triangle> tris;
		vector<Segment> segs;

		for(auto *face : mesh.faces()) {
			auto verts = face->verts();
			if(face->isBoundary()) {
				for(auto e : face->edges())
					segs.emplace_back(e.segment());
				tris.emplace_back(verts[0]->pos(), verts[1]->pos(), verts[2]->pos());
			}
		}

		vis_data->segment_groups.emplace_back(Color::magneta, segs);
		vis_data->poly_soups.emplace_back(Color(Color::brown, 255), tris);
	}
}

Projection edgeProjection(Edge edge, Face *face) {
	Ray ray(edge.segment());
	float3 far_point = face->otherVert({edge.a, edge.b})->pos();
	float3 edge_point = closestPoint(Ray(edge.segment()), far_point);
	return Projection(edge.a->pos(), normalize(edge_point - far_point), ray.dir());
}

// How these faces will be interpreted depends on the CSG operation
// So, maybe in some situations it makes no sense to make faces compatible:
// for example: in case of subtraction: opposite shared faces don't have to be compatible
// because tets behind them won't be merged
enum FaceType { face_unclassified, face_inside, face_outside, face_shared, face_shared_opposite };

void floodFill(HalfTetMesh &mesh, vector<Face *> list, vector<Edge> limits) {
	while(!list.empty()) {
		Face *face = list.back();
		list.pop_back();

		for(auto edge : face->edges()) {
			bool is_seg_boundary = false;
			for(auto tedge : limits)
				if(edge == tedge || edge == tedge.inverse()) {
					is_seg_boundary = true;
					break;
				}

			if(!is_seg_boundary)
				for(auto *nface : mesh.edgeFaces(edge.a, edge.b))
					if(nface->temp() == face_unclassified && nface->isBoundary()) {
						list.emplace_back(nface);
						nface->setTemp(face->temp());
					}
		}
	}
}

void divideIntoSegments(HalfTetMesh &mesh1, HalfTetMesh &mesh2, const vector<Edge> &loops1,
						const vector<Edge> &loops2) {
	//	for(auto *face1 : mesh1.faces())
	//		face1->setTemp(0);
	for(auto *face2 : mesh2.faces())
		face2->setTemp(face_unclassified);

	vector<Face *> list1, list2;

	DASSERT(loops1.size() == loops2.size());
	for(int i = 0; i < (int)loops1.size(); i++) {
		std::array<Vertex *, 2> edge1{{loops1[i].a, loops1[i].b}};
		std::array<Vertex *, 2> edge2{{loops2[i].a, loops2[i].b}};

		auto faces1 = mesh1.edgeBoundaryFaces(edge1[0], edge1[1]);
		auto faces2 = mesh2.edgeBoundaryFaces(edge2[0], edge2[1]);
		DASSERT(faces1.size() == 2 && faces2.size() == 2);
		DASSERT(distance(edge1[0]->pos(), edge2[0]->pos()) < constant::epsilon);
		DASSERT(distance(edge1[1]->pos(), edge2[1]->pos()) < constant::epsilon);

		Projection proj = edgeProjection(loops1[i], faces1[0]);

		float2 vectors1[2] = {(proj * faces1[0]->otherVert(edge1)->pos()).xz(),
							  (proj * faces1[1]->otherVert(edge1)->pos()).xz()};
		float2 vectors2[2] = {(proj * faces2[0]->otherVert(edge2)->pos()).xz(),
							  (proj * faces2[1]->otherVert(edge2)->pos()).xz()};

		float2 normals1[2] = {(proj.projectVector(faces1[0]->triangle().normal())).xz(),
							  (proj.projectVector(faces1[1]->triangle().normal())).xz()};
		float2 normals2[2] = {(proj.projectVector(faces2[0]->triangle().normal())).xz(),
							  (proj.projectVector(faces2[1]->triangle().normal())).xz()};

		for(int n = 0; n < 2; n++) {
			vectors1[n] = normalize(vectors1[n]);
			vectors2[n] = normalize(vectors2[n]);
			DASSERT(isNormalized(normals1[n]));
			DASSERT(isNormalized(normals2[n]));
		}

		float dir1[2], dir2[2];
		for(int n = 0; n < 2; n++) {
			float2 mvector1 = normalize(vectors1[n] + normals1[n]);
			dir1[n] = angleBetween(vectors1[n], float2(0, 0), mvector1);
			if(dir1[n] > constant::pi)
				dir1[n] -= constant::pi * 2.0f;
			float2 mvector2 = normalize(vectors2[n] + normals2[n]);
			dir2[n] = angleBetween(vectors2[n], float2(0, 0), mvector2);
			if(dir2[n] > constant::pi)
				dir2[n] -= constant::pi * 2.0f;
		}

		//		xmlPrint("vectors1: (%) (%) % %\n", vectors1[0], vectors1[1], dir1[0], dir1[1]);
		//		xmlPrint("vectors2: (%) (%) % %\n", vectors2[0], vectors2[1], dir2[0], dir2[1]);

		DASSERT((dir1[0] < 0.0f) != (dir1[1] < 0.0f));
		DASSERT((dir2[0] < 0.0f) != (dir2[1] < 0.0f));

		if(dir1[0] < 0.0f)
			swap(vectors1[0], vectors1[1]);

		float face1_angle = angleBetween(vectors1[0], float2(0, 0), vectors1[1]);

		for(int n = 0; n < 2; n++) {
			float angle = angleBetween(vectors1[0], float2(0, 0), vectors2[n]);
			float eps = constant::epsilon;
			if(angle > constant::pi * 2.0f - eps)
				angle -= constant::pi * 2.0f;

			int value = face_shared;
			if(angle < -eps)
				value = face_inside;
			else if(angle < eps)
				value = dir2[n] < 0.0f ? face_shared_opposite : face_shared;
			else if(angle < face1_angle - eps)
				value = face_outside;
			else if(angle < face1_angle + eps)
				value = dir2[n] < 0.0f ? face_shared : face_shared_opposite;
			else
				value = face_inside;

			int old = faces2[n]->temp();
			if(old && old != value) {
				printf("Error: %d -> %d\n", old, value);
				printf("dir1: %f %f\ndir2: %f %f\nangles: %f %f\n\n", dir1[0], dir1[1], dir2[0],
					   dir2[1], angle, face1_angle);
			}
			faces2[n]->setTemp(value);
		}

		insertBack(list2, faces2);
	}

	floodFill(mesh2, list2, loops2);
}

TetMesh finalCuts(HalfTetMesh h1, HalfTetMesh h2, TetMesh::CSGVisualData *vis_data) {
	vector<Triangle> tris1, tris2;
	vector<Face *> faces1, faces2;

	for(auto *tet : h1.tets())
		tet->setTemp(h2.isIntersecting(tet->tet()) ? 1 : 0);
	for(auto *tet : h1.tets())
		for(auto *ntet : tet->neighbours())
			if(ntet && ntet->temp())
				tet->setTemp(1);
	for(auto *tet : h1.tets()) {
		if(tet->temp()) {
			auto faces = tet->faces();
			auto neighbours = tet->neighbours();

			for(int i = 0; i < 4; i++)
				if(isOneOf(faces[i]->temp(), face_outside, face_shared_opposite) ||
				   (neighbours[i] && !neighbours[i]->temp()))
					faces1.emplace_back(faces[i]);
		}
	}

	for(auto *face : h2.faces())
		if(face->temp() == face_inside)
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

	bool is_manifold = true; // HalfMesh(fill_mesh).is2Manifold();
	DASSERT(is_manifold);

	if(is_manifold) {
		auto fill = HalfTetMesh(TetMesh::make(fill_mesh, 0));
		if(vis_data && vis_data->phase >= 4) {
			vis_data->tet_meshes.emplace_back(Color::red, TetMesh(fill));
			vector<Segment> segments;
			for(auto btri : fill_mesh.tris())
				for(auto edge : btri.edges())
					segments.emplace_back(edge.first, edge.second);
			vis_data->segment_groups.emplace_back(Color::blue, segments);
		}

		for(auto *tet : h1.tets())
			if(!tet->temp()) {
				auto old_verts = tet->verts();
				array<Vertex *, 4> new_verts;
				for(int i = 0; i < 4; i++) {
					float3 pos = old_verts[i]->pos();
					new_verts[i] = fill.findVertex(pos);
					if(!new_verts[i])
						new_verts[i] = fill.addVertex(pos);
				}

				fill.addTet(new_verts);
			}

		if(vis_data && vis_data->phase == 5) {
			for(auto *tet : h1.tets())
				if(tet->temp())
					h1.removeTet(tet);
			vis_data->tet_meshes.emplace_back(Color::green, TetMesh(h1));
		}
		return TetMesh(fill);
	}

	return TetMesh();
}

TetMesh TetMesh::csg(const TetMesh &a, const TetMesh &b, CSGMode mode, CSGVisualData *vis_data) {
	// TODO: accuracy depends on bounding box coordinates, take them into consideration
	float epsilon = 0.0001f;

	FBox csg_bbox = enlarge(intersection(a.computeBBox(), b.computeBBox()), epsilon);
	HalfTetMesh hmesh1(a.extract(a.selection(csg_bbox)));
	HalfTetMesh hmesh2(b.extract(b.selection(csg_bbox)));

	// DASSERT(HalfMesh(TetMesh(hmesh1).toMesh()).is2Manifold());
	// DASSERT(HalfMesh(TetMesh(hmesh2).toMesh()).is2Manifold());

	vector<Isect> isects;
	for(auto *face_a : hmesh1.faces())
		if(face_a->isBoundary())
			for(auto *face_b : hmesh2.faces())
				if(face_b->isBoundary()) {
					auto edges = compatibleEdges(face_a->triangle(), face_b->triangle());
					insertBack(edges, compatibleEdges(face_b->triangle(), face_a->triangle()));
					for(auto edge : edges)
						isects.emplace_back(Isect{face_a, face_b, edge});
				}

	// Here we have to make sure, that edges form closed loops
	vector<Edge> loops1, loops2;
	FaceEdgeMap map1, map2;
	extractLoops(isects, hmesh1, hmesh2, loops1, loops2, map1, map2);

	bool do_print = false;
	if(do_print) {
		xmlPrint("Start loops: ");
		for(auto edge : loops1)
			xmlPrint("%-% ", (long long)edge.a % 1337, (long long)edge.b % 1337);
		xmlPrint("\n");
		xmlPrint("Start loops: ");
		for(auto edge : loops2)
			xmlPrint("%-% ", (long long)edge.a % 1337, (long long)edge.b % 1337);
		xmlPrint("\n");
	}

	std::map<Vertex *, int> vcounts;
	for(auto edge : loops1) {
		vcounts[edge.a]++;
		vcounts[edge.b]++;
	}
	for(auto edge : loops2) {
		vcounts[edge.a]++;
		vcounts[edge.b]++;
	}

	vector<Segment> red;
	bool restart = true;
	while(restart) {
		restart = false;
		for(int e = 0; e < (int)loops1.size(); e++) {
			Edge edge = loops1[e];
			if(vcounts[edge.a] < 2 || vcounts[edge.b] < 2) {
				restart = true;
				vcounts[edge.a]--;
				vcounts[edge.b]--;
				vcounts[loops2[e].a]--;
				vcounts[loops2[e].b]--;

				printf("Red: %f\n", loops1[e].segment().length());
				red.emplace_back(loops1[e].segment());
				loops1[e] = loops1.back();
				loops1.pop_back();
				loops2[e] = loops2.back();
				loops2.pop_back();
				e--;
			}
		}
	}

	if(vis_data) {
		struct Compare {
			bool operator()(const float3 &a, const float3 &b) const {
				return std::tie(a.x, a.y, a.z) < std::tie(b.x, b.y, b.z);
			}
		};

		vector<Segment> segs;
		std::map<float3, int, Compare> points;
		vector<vector<float3>> tpoints;

		for(const auto &edge : loops1) {
			segs.emplace_back(edge.segment());
			points[edge.a->pos()]++;
			points[edge.b->pos()]++;
		}

		if(0) {
			for(auto &p : points) {
				if((int)tpoints.size() <= p.second)
					tpoints.resize(p.second + 1);
				tpoints[p.second].emplace_back(p.first);
			}

			vector<Color> colors = {Color::black,  Color::green, Color::red,	Color::blue,
									Color::yellow, Color::white, Color::magneta};
			int id = 0;
			for(auto tp : tpoints)
				vis_data->point_sets.emplace_back(colors[id++ % colors.size()], tp);
		}

		vis_data->segment_groups_trans.emplace_back(Color::black, segs);
		vis_data->segment_groups_trans.emplace_back(Color::red, red);
		if(vis_data->phase == 0) {
			vis_data->tet_meshes.emplace_back(Color(Color::red, 100), TetMesh(hmesh1));
			vis_data->tet_meshes.emplace_back(Color(Color::green, 100), TetMesh(hmesh2));
		}
	}

	for(auto vcount : vcounts)
		DASSERT(vcount.second >= 2 || vcount.second == 0);

	try {
		triangulateMesh(hmesh1, map1, vis_data, 1);
		triangulateMesh(hmesh2, map2, vis_data, 2);

		bool err = false;
		for(auto e1 : loops1)
			if(hmesh1.edgeBoundaryFaces(e1.a, e1.b).empty()) {
				xmlPrint("Not found: %-%\n", (long long)e1.a % 1337, (long long)e1.b % 1337);
				err = true;
			}
		for(auto e2 : loops2)
			if(hmesh2.edgeBoundaryFaces(e2.a, e2.b).empty()) {
				xmlPrint("Not found: %-%\n", (long long)e2.a % 1337, (long long)e2.b % 1337);
				bool err = true;
			}
		if(err)
			THROW("Invalid triangulation (not all edges are present in the output)");

		divideIntoSegments(hmesh1, hmesh2, loops1, loops2);
		divideIntoSegments(hmesh2, hmesh1, loops2, loops1);

		if(vis_data->phase == 2) {
			vector<Color> colors = {Color::black, Color::green, Color::red, Color::yellow,
									Color::magneta};
			vector<vector<Triangle>> segs;
			segs.clear();
			for(auto *face : hmesh1.faces()) {
				int seg_id = face->temp();
				if(seg_id > 0) {
					if((int)segs.size() < seg_id + 1)
						segs.resize(seg_id + 1);
					segs[seg_id].emplace_back(face->triangle());
				}
			}
			for(int n = 0; n < (int)segs.size(); n++)
				vis_data->poly_soups.emplace_back(colors[n % colors.size()], segs[n]);
		}

		auto final = finalCuts(hmesh1, hmesh2, vis_data);
		// DASSERT(HalfMesh(TetMesh(final).toMesh()).is2Manifold());

		return TetMesh(final);
	} catch(const Exception &ex) {
		printf("Error: %s\n%s Mesh #1 stats:\n%s\n\nMesh #2 stats:\n%s\n", ex.what(),
			   ex.backtrace(true).c_str(), hmesh1.stats().c_str(), hmesh2.stats().c_str());
		return {};
	}
}

Mesh TetMesh::toMesh() const {
	vector<uint> faces;
	vector<float3> new_verts;
	vector<int> vert_map(m_verts.size(), -1);

	for(int t = 0; t < size(); t++)
		for(int i = 0; i < 4; i++)
			if(m_tet_tets[t][i] == -1) {
				auto face = tetFace(t, i);
				for(int j = 0; j < 3; j++) {
					if(vert_map[face[j]] == -1) {
						vert_map[face[j]] = (int)new_verts.size();
						new_verts.emplace_back(m_verts[face[j]]);
					}
					faces.emplace_back((uint)vert_map[face[j]]);
				}
			}

	return Mesh(MeshBuffers(std::move(new_verts)), {{std::move(faces)}});
}
}
