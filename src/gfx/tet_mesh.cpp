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

TetMesh TetMesh::selectTets(const TetMesh &mesh, const vector<int> &indices) {
	// TODO: select vertices
	auto indexer = indexWith(mesh.tetVerts(), indices);
	return TetMesh(mesh.verts(), vector<TetIndices>(begin(indexer), end(indexer)));
}

using Tet = HalfTetMesh::Tet;
using Face = HalfTetMesh::Face;
using Vertex = HalfTetMesh::Vertex;
using Edge = HalfTetMesh::Edge;

struct Isect {
	Face *face_a, *face_b;
	Segment segment;
};

struct LoopElem {
	Face *face;
	Vertex *origin;
};

using Loop = vector<LoopElem>;

pair<Loop, Loop> extractLoop(vector<Isect> &isects, HalfTetMesh &ha, HalfTetMesh &hb) {
	// TODO: what if loop touches 3 faces of single tetrahedron?
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

void saveSvg(vector<float2> points, vector<Segment2D> segs, vector<Triangle2D> tris, int id,
			 float scale) {
	XMLDocument doc;
	auto svg_node = doc.addChild("svg");
	svg_node = svg_node.addChild("g");

	auto cnode = svg_node.addChild("g");
	cnode.addAttrib("render-order", -1);
	cnode.addAttrib("style", "stroke-width:3;stroke:black");
	float2 tmin;
	for(auto tri : tris)
		for(int i = 0; i < 3; i++)
			tmin = min(tmin, tri[i]);
	for(auto pt : points)
		tmin = min(tmin, pt);
	for(auto seg : segs)
		tmin = min(tmin, min(seg.start, seg.end));

	tmin *= scale;
	float2 offset = -tmin + float2(50, 50);

	for(auto pt : points) {
		auto vert = cnode.addChild("circle");
		vert.addAttrib("cx", pt.x * scale + offset.x);
		vert.addAttrib("cy", pt.y * scale + offset.y);
		vert.addAttrib("r", 4);
	}

	auto lnode = svg_node.addChild("g");
	lnode.addAttrib("style", "stroke-width:2.5;stroke:black;"
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
		for(auto &pt : p)
			pt = lerp(pt, center, 0.01f);
		Triangle t3{float3(p[0], 0.0f), float3(p[1], 0.0f), float3(p[2], 0.0f)};
		float area = t3.area();

		auto poly = tnode.addChild("polygon");
		string points = format("%f,%f %f,%f %f,%f", p[0].x, p[0].y, p[1].x, p[1].y, p[2].x, p[2].y);
		poly.addAttrib("points", points);
		poly.addAttrib("style", "stroke-width:2;fill:red;stroke:blue;fill-opacity:0.4");
	}

	Saver(format("temp/file%d.svg", id)) << doc;
}

struct Line {
	Vertex *e1, *e2;
	vector<Edge> edges;
};
vector<Line> extractLines(const vector<Edge> &edges, vector<Vertex *> endings) {
	vector<Line> out;
	vector<int> marked(edges.size(), 0);

	std::map<Vertex *, vector<int>> edge_map;
	for(int n = 0; n < (int)edges.size(); n++) {
		edge_map[edges[n].a].emplace_back(n);
		edge_map[edges[n].b].emplace_back(n);
	}

	/*	printf("Extracting lines:\n");
		for(const auto &edge : edges)
			xmlPrint("% - %\n", (long long)edge.a % 1337, (long long)edge.b % 1337);
		printf("endings:\n");
		for(auto *v : endings)
			xmlPrint("%\n", (long long)v % 1337);*/

	for(int n = 0; n < (int)edges.size(); n++) {
		if(marked[n])
			continue;
		const auto &start = edges[n];

		bool aends = isOneOf(start.a, endings);
		bool bends = isOneOf(start.b, endings);
		if(!aends && !bends)
			continue;

		marked[n] = true;

		Line new_line{start.a, start.b, {}};
		if(bends)
			swap(new_line.e1, new_line.e2);
		new_line.edges.emplace_back(new_line.e1, new_line.e2);

		while(!isOneOf(new_line.e2, endings)) {
			auto &inds = edge_map[new_line.e2];
			int edge_id = -1;
			for(int i : inds)
				if(!marked[i])
					edge_id = i;
			DASSERT(edge_id != -1);
			marked[edge_id] = 1;

			Vertex *next = edges[edge_id].a == new_line.e2 ? edges[edge_id].b : edges[edge_id].a;
			new_line.edges.emplace_back(new_line.e2, next);
			new_line.e2 = next;
		}

		out.emplace_back(std::move(new_line));
	}

	return out;
}

// bedges: border edges
// iedges: inside edges
vector<vector<Edge>> findSimplePolygons(const vector<Edge> &bedges, const vector<Edge> &iedges,
										const vector<Vertex *> &endings) {
	vector<Line> blines = extractLines(bedges, endings);
	vector<Line> lines = extractLines(iedges, endings);
	vector<int> marked(blines.size(), 0);

	vector<vector<Edge>> out;

	/*	for(auto bl : blines) {
			xmlPrint("Bline: ");
			for(auto e : bl.edges)
				xmlPrint("(%-%) ", (long long)e.a % 1337, (long long)e.b % 1337);
			xmlPrint("\n");
		}
		for(auto l : lines) {
			xmlPrint("Line: ");
			for(auto e : l.edges)
				xmlPrint("(%-%) ", (long long)e.a % 1337, (long long)e.b % 1337);
			xmlPrint("\n");
		}*/

	for(int n = 0; n < (int)blines.size(); n++) {
		if(marked[n])
			continue;
		marked[n] = true;
		//		xmlPrint("Select: %-%: ", (long long)blines[n].e1 % 1337, (long long)blines[n].e2 %
		// 1337);

		const auto &line = blines[n];
		int b1 = -1, b2 = -1;
		for(int i = 0; i < (int)lines.size(); i++) {
			if(isOneOf(line.e1, lines[i].e1, lines[i].e2))
				b1 = i;
			if(isOneOf(line.e2, lines[i].e1, lines[i].e2))
				b2 = i;
		}
		DASSERT(b1 != -1 && b2 != -1);
		const auto &line1 = lines[b1], &line2 = lines[b2];

		/*
		xmlPrint("(%-%) ", (long long)lines[b1].e1 % 1337, (long long)lines[b1].e2 % 1337);
		if(b1 != b2)
			xmlPrint("(%-%) ", (long long)lines[b2].e1 % 1337, (long long)lines[b2].e2 % 1337);
		printf("\n");*/

		int inside = -1;
		bool inside_invert = false;
		if(b1 != b2) {
			const auto *e1 = isOneOf(line1.e1, line.e1, line.e2) ? line1.e2 : line1.e1;
			const auto *e2 = isOneOf(line2.e1, line.e1, line.e2) ? line2.e2 : line2.e1;

			if(e1 != e2) {
				bool invert = false;
				for(int j = n + 1; j < (int)blines.size(); j++) {
					if(marked[j])
						continue;
					if(blines[j].e1 == e1 && blines[j].e2 == e2) {
						inside = j;
						break;
					}
					if(blines[j].e1 == e2 && blines[j].e2 == e1) {
						inside = j;
						inside_invert = true;
						break;
					}
				}
				if(inside == -1)
					exit(0);
				DASSERT(inside != -1);
				marked[inside] = 1;
			}
		}

		// TODO: extract ordered
		vector<Edge> poly_edges;
		insertBack(poly_edges, line.edges);
		insertBack(poly_edges, line1.edges);
		if(b1 != b2)
			insertBack(poly_edges, line2.edges);
		if(inside != -1)
			insertBack(poly_edges, blines[inside].edges);

		/*		xmlPrint("Poly: ");
				for(auto e : poly_edges)
					xmlPrint("(%-%) ", (long long)e.a % 1337, (long long)e.b % 1337);
				xmlPrint("\n");*/
		out.emplace_back(std::move(poly_edges));
	}

	return out;
}

vector<Vertex *> polyVerts(const vector<Edge> &edges) {
	DASSERT(edges.size() >= 3);
	vector<int> marked(edges.size(), 0);

	Vertex *end_vert = edges.front().b;
	vector<Vertex *> out = {edges.front().a};
	marked[0] = 1;
	int count = 1;

	while(count < (int)edges.size()) {
		Vertex *next = nullptr;
		for(int n = 0; n < (int)edges.size(); n++) {
			if(marked[n])
				continue;
			if(edges[n].a == out.back()) {
				next = edges[n].b;
				marked[n] = 1;
				break;
			}
			if(edges[n].b == out.back()) {
				next = edges[n].a;
				marked[n] = 1;
				break;
			}
		}

		DASSERT(next);
		count++;
		out.emplace_back(next);
	}

	DASSERT(out.back() == end_vert);
	return out;
}

vector<float> computeAngles(const vector<Vertex *> &verts, const Projection &proj) {
	vector<float> out(verts.size());

	for(int n = 0, count = (int)verts.size(); n < (int)verts.size(); n++) {
		float2 cur = (proj * verts[n]->pos()).xz();
		float2 prev = (proj * verts[(n + count - 1) % count]->pos()).xz();
		float2 next = (proj * verts[(n + 1) % count]->pos()).xz();

		float vcross = -cross(normalize(cur - prev), normalize(next - cur));
		float vdot = dot(normalize(next - cur), normalize(prev - cur));
		out[n] = atan2(vcross, vdot);
		if(out[n] < 0.0f)
			out[n] = constant::pi * 2.0f + out[n];
		DASSERT(out[n] >= 0.0f && out[n] < constant::pi * 2.0f);
	}

	return out;
}

// Simple ear-clipping algorithm from: http://arxiv.org/pdf/1212.6038.pdf
// TODO: avoid slivers
vector<array<Vertex *, 3>> triangulateSimplePolygon(Face *face, const vector<Edge> &edges) {
	vector<array<Vertex *, 3>> out;

	Projection proj(face->triangle());
	vector<Vertex *> verts = polyVerts(edges);
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

	/*
	printf("Verts: (%f)\n", radToDeg(angle_sum));
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
										   const vector<Edge> &iedges,
										   const vector<Vertex *> all_splits) {

	vector<vector<Edge>> simple_polys = findSimplePolygons(bedges, iedges, all_splits);

	vector<array<Vertex *, 3>> out;
	for(const auto &poly : simple_polys)
		insertBack(out, triangulateSimplePolygon(face, poly));
	return out;
}

// When more than one triangulated face belong to single tetrahedron,
// then this tet should be subdivided
void prepareMesh(HalfTetMesh &mesh, vector<Loop> &loops) {
	vector<Tet *> split_tets;

	for(auto *face : mesh.faces())
		face->setTemp(0);
	for(const auto &loop : loops)
		for(const auto &elem : loop)
			elem.face->setTemp(1);

	std::map<Face *, Face *> face_map;

	for(auto *tet : mesh.tets()) {
		int count = 0;
		for(auto *face : tet->faces())
			count += face->temp();

		if(count > 1) {
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

	for(auto &loop : loops)
		for(auto &elem : loop) {
			auto it = face_map.find(elem.face);
			if(it != face_map.end())
				elem.face = it->second;
		}
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

vector<Edge> triangulateMesh(HalfTetMesh &mesh, vector<Loop> &loops,
							 TetMesh::CSGVisualData *vis_data, int mesh_id) {
	// printf("Triangulation:\n");
	// TODO: edge may already exist
	vector<Edge> processed;

	prepareMesh(mesh, loops);

	struct FaceEdge {
		Vertex *v1, *v2;
		Face *prev, *next;
	};

	std::map<Face *, vector<FaceEdge>> face_edges;

	if(vis_data && vis_data->phase == 1 && mesh_id == 1) {
		vector<Triangle> tris;
		for(auto loop : loops)
			for(auto elem : loop) {
				auto verts = elem.face->verts();
				tris.emplace_back(verts[0]->pos(), verts[1]->pos(), verts[2]->pos());
			}
		vis_data->poly_soups.emplace_back(Color::red, tris);
	}

	for(const auto &loop : loops) {
		if(loop.empty())
			continue;
		DASSERT(loop.size() >= 2);
		LoopElem prev = loop.back();

		for(int n = 0; n < (int)loop.size(); n++) {
			const auto &cur = loop[n];
			const auto &prev = loop[(n + loop.size() - 1) % loop.size()];
			const auto &next = loop[(n + 1) % loop.size()];

			face_edges[cur.face].emplace_back(
				FaceEdge{cur.origin, next.origin, prev.face, next.face});
		}
	}

	// After triangulation face pointers in loops will no longer be valid
	for(auto &loop : loops)
		for(auto &elem : loop)
			elem.face = nullptr;

	struct SplitInfo {
		Edge edge;
		vector<Vertex *> splits;
	};

	vector<pair<Vertex *, vector<array<Vertex *, 3>>>> face_triangulations;
	vector<SplitInfo> edge_splits;
	vector<Tet *> rem_tets;

	// TODO: specified edge may lie on the boundary of the triangle

	int tid = 0;
	for(auto pair : face_edges) {
		auto *face = pair.first;
		auto verts = pair.first->verts();
		array<vector<Vertex *>, 3> edge_verts;
		vector<Vertex *> all_splits;
		rem_tets.emplace_back(face->tet());

		//	printf("Preparing %d:\n", tid);
		vector<Edge> tri_boundary_edges, tri_inside_edges;
		for(auto edge : pair.second) {
			//		xmlPrint("Inside edge: (%) (%)\n", edge.v1->pos(), edge.v2->pos());

			if(edge.prev != face)
				edge_verts[face->edgeId(mesh.sharedEdge(face, edge.prev))].emplace_back(edge.v1);
			if(edge.next != face)
				edge_verts[face->edgeId(mesh.sharedEdge(face, edge.next))].emplace_back(edge.v2);
			tri_inside_edges.emplace_back(Edge(edge.v1, edge.v2));
			processed.emplace_back(tri_inside_edges.back());
		}

		for(int i = 0; i < 3; i++) {
			auto edge = face->edges()[i];

			if(edge_verts[i].empty()) {
				tri_boundary_edges.emplace_back(edge.a, edge.b);
				continue;
			}

			auto splits = sortEdgeVerts(edge, edge_verts[i]);
			edge_splits.emplace_back(SplitInfo{face->edges()[i], splits});
			insertBack(all_splits, splits);
			//		xmlPrint("Outside edge #% (%) (%): ", i, edge.a->pos(), edge.b->pos());
			//		for(auto split : splits)
			//			xmlPrint("(%) ", split->pos());
			//		xmlPrint("\n");

			tri_boundary_edges.emplace_back(edge.a, splits.front());
			for(int i = 0; i + 1 < (int)splits.size(); i++)
				tri_boundary_edges.emplace_back(splits[i], splits[i + 1]);
			tri_boundary_edges.emplace_back(splits.back(), edge.b);
		}

		auto other_vert = setDifference(face->tet()->verts(), face->verts());
		DASSERT(other_vert.size() == 1);

		auto triangles = triangulateFace(face, tri_boundary_edges, tri_inside_edges, all_splits);
		if(vis_data && vis_data->phase == 1) {
			Color col = mesh_id % 2 ? Color::yellow : Color::blue;
			vector<Segment> edges;
			for(auto tri : triangles)
				for(int i = 0; i < 3; i++)
					edges.emplace_back(tri[i]->pos(), tri[(i + 1) % 3]->pos());
			vis_data->segment_groups_trans.emplace_back(col, edges);
		}

		face_triangulations.emplace_back(other_vert.front(), std::move(triangles));
	}

	makeUnique(rem_tets);
	for(auto *tet : rem_tets)
		mesh.removeTet(tet);

	// TODO: better ordering
	std::sort(begin(edge_splits), end(edge_splits), [](const auto &s1, const auto &s2) {
		float3 c1 = s1.edge.a->pos() + s1.edge.b->pos();
		float3 c2 = s2.edge.a->pos() + s2.edge.b->pos();
		return c1.x + c1.y + c1.z < c2.x + c2.y + c2.z;
	});

	for(const auto &split : edge_splits)
		mesh.subdivideEdge(split.edge.a, split.edge.b, split.splits);

	for(const auto &tring : face_triangulations)
		for(auto face_verts : tring.second)
			mesh.addTet(face_verts[0], face_verts[1], face_verts[2], tring.first);

	if(vis_data && vis_data->phase == 1 && mesh_id == 1) {
		vector<Triangle> tris;
		for(auto *face : mesh.faces()) {
			auto verts = face->verts();
			if(face->isBoundary())
				tris.emplace_back(verts[0]->pos(), verts[1]->pos(), verts[2]->pos());
		}
		vis_data->poly_soups.emplace_back(Color(Color::blue, 100), tris);
	}

	return processed;
}

static float volume(Vertex *a, Vertex *b, Vertex *c, Vertex *d) {
	return Tetrahedron(a->pos(), b->pos(), c->pos(), d->pos()).volume();
}

void divideIntoSegments(HalfTetMesh &mesh1, HalfTetMesh &mesh2, const vector<Loop> &loops1,
						const vector<Loop> &loops2, const vector<Edge> &edges2) {
	//	for(auto *face1 : mesh1.faces())
	//		face1->setTemp(0);
	for(auto *face2 : mesh2.faces())
		face2->setTemp(0);

	vector<Face *> list;

	DASSERT(loops1.size() == loops2.size());
	for(int l = 0; l < (int)loops1.size(); l++) {
		const auto &loop1 = loops1[l];
		const auto &loop2 = loops2[l];
		DASSERT(loop1.size() == loop2.size());

		for(int i = 0; i < (int)loop1.size(); i++) {
			std::array<Vertex *, 2> edge1{{loop1[i].origin, loop1[(i + 1) % loop1.size()].origin}};
			std::array<Vertex *, 2> edge2{{loop2[i].origin, loop2[(i + 1) % loop2.size()].origin}};

			auto faces1 = mesh1.edgeBoundaryFaces(edge1[0], edge1[1]);
			auto faces2 = mesh2.edgeBoundaryFaces(edge2[0], edge2[1]);
			DASSERT(faces1.size() == 2 && faces2.size() == 2);

			std::array<Vertex *, 2> far_verts1 = {
				{faces1[0]->otherVert(edge1), faces1[1]->otherVert(edge1)}};
			std::array<Vertex *, 2> far_verts2 = {
				{faces2[0]->otherVert(edge2), faces2[1]->otherVert(edge2)}};
			auto verts1a = faces1[0]->verts(), verts1b = faces1[1]->verts();
			auto verts2a = faces2[0]->verts(), verts2b = faces2[1]->verts();

			float v1 = volume(verts1a[0], verts1a[1], verts1a[2], far_verts2[0]);
			float v2 = volume(verts1b[0], verts1b[1], verts1b[2], far_verts2[0]);

			int vert_inside = 0;
			if(v1 < 0.0f && v2 < 0.0f)
				vert_inside = 0;
			else if(v1 > 0.0f && v2 > 0.0f)
				vert_inside = 1;
			else {
				// TODO: verify me
				float v3 = volume(verts1a[0], verts1a[1], verts1a[2], far_verts1[1]);
				vert_inside = v3 < 0.0f ? 0 : 1;
			}

			faces2[vert_inside]->setTemp(1);
			faces2[vert_inside ^ 1]->setTemp(-1);
			insertBack(list, faces2);
		}
	}

	while(!list.empty()) {
		Face *face = list.back();
		list.pop_back();

		for(auto edge : face->edges()) {
			bool is_seg_boundary = false;
			for(auto tedge : edges2)
				if(edge == tedge || edge == tedge.inverse()) {
					is_seg_boundary = true;
					break;
				}

			if(!is_seg_boundary)
				for(auto *nface : mesh2.edgeFaces(edge.a, edge.b))
					if(nface->temp() == 0 && nface->isBoundary()) {
						list.emplace_back(nface);
						nface->setTemp(face->temp());
					}
		}
	}
}

TetMesh finalCuts(HalfTetMesh h1, HalfTetMesh h2, TetMesh::CSGVisualData *vis_data) {
	vector<Triangle> tris1, tris2;
	vector<Face *> faces1, faces2;

	for(auto *tet : h1.tets())
		tet->setTemp(h2.isIntersecting(tet->tet()) ? 1 : 0);
	for(auto *tet : h1.tets()) {
		if(tet->temp()) {
			auto faces = tet->faces();
			auto neighbours = tet->neighbours();

			for(int i = 0; i < 4; i++)
				if(faces[i]->temp() > 0 || (neighbours[i] && !neighbours[i]->temp()))
					faces1.emplace_back(faces[i]);
		}
	}

	for(auto *face : h2.faces())
		if(face->temp() < 0)
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
	HalfTetMesh ha(a), hb(b);

	// DASSERT(HalfMesh(TetMesh(ha).toMesh()).is2Manifold());
	// DASSERT(HalfMesh(TetMesh(hb).toMesh()).is2Manifold());

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

						// TODO: handle situation when cuts are very close to vertices
						DASSERT(!ha.findVertex(isect.first.origin()));
						DASSERT(!ha.findVertex(isect.first.end()));
						DASSERT(!hb.findVertex(isect.first.origin()));
						DASSERT(!hb.findVertex(isect.first.end()));
					}

	vector<Loop> a_loops, b_loops;

	vector<Segment> boundary_segs;
	vector<Triangle> boundary_tris[2];

	while(true) {
		auto new_loop = extractLoop(isects, ha, hb);
		if(new_loop.first.empty())
			break;

		for(int n = 0, count = (int)new_loop.first.size(); n < count; n++) {
			const auto &prev = new_loop.first[(n - 1 + count) % count];
			const auto &cur = new_loop.first[n];

			boundary_segs.emplace_back(Segment(prev.origin->pos(), cur.origin->pos()));
			boundary_tris[0].emplace_back(cur.face->triangle());
		}
		for(int n = 0, count = (int)new_loop.second.size(); n < count; n++) {
			const auto &cur = new_loop.second[n];
			boundary_tris[1].emplace_back(cur.face->triangle());
		}

		a_loops.emplace_back(new_loop.first);
		b_loops.emplace_back(new_loop.second);
	}

	if(vis_data) {
		vis_data->segment_groups_trans.emplace_back(Color::black, boundary_segs);
		if(vis_data->phase == 0) {
			vis_data->poly_soups.emplace_back(Color::red, boundary_tris[0]);
			vis_data->poly_soups.emplace_back(Color::green, boundary_tris[1]);
		}
	}

	try {
		auto edges1 = triangulateMesh(ha, a_loops, vis_data, 1);
		auto edges2 = triangulateMesh(hb, b_loops, vis_data, 2);

		divideIntoSegments(ha, hb, a_loops, b_loops, edges2);
		divideIntoSegments(hb, ha, b_loops, a_loops, edges1);

		if(vis_data->phase == 2) {
			vector<Color> colors = {Color::green, Color::red, Color::blue, Color::yellow,
									Color::white};
			vector<vector<Triangle>> segs;
			segs.clear();
			for(auto *face : ha.faces()) {
				int seg_id = face->temp();
				if(seg_id < 0)
					seg_id = colors.size() / 2 - seg_id;
				if(seg_id != 0) {
					if((int)segs.size() < seg_id + 1)
						segs.resize(seg_id + 1);
					segs[seg_id].emplace_back(face->triangle());
				}
			}
			for(int n = 0; n < (int)segs.size(); n++)
				vis_data->poly_soups.emplace_back(colors[n % colors.size()], segs[n]);
		}

		auto final = finalCuts(ha, hb, vis_data);
		// DASSERT(HalfMesh(TetMesh(final).toMesh()).is2Manifold());

		return TetMesh(final);
	} catch(const Exception &ex) {
		printf("Error: %s\n%s", ex.what(), ex.backtrace(true).c_str());
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
