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

TetMesh TetMesh::merge(const vector<TetMesh> &sub_tets) {
	vector<float3> positions;
	vector<TetIndices> indices;
	std::map<std::tuple<float, float, float>, int> pos_map;

	for(const auto &sub_tet : sub_tets) {
		const auto &verts = sub_tet.verts();

		for(auto tidx : sub_tet.tetVerts()) {
			TetIndices inds;
			for(int i = 0; i < 4; i++) {
				int idx = tidx[i];
				auto point = verts[tidx[i]];
				auto it = pos_map.find(std::tie(point.x, point.y, point.z));
				if(it == pos_map.end()) {
					pos_map[std::tie(point.x, point.y, point.z)] = inds[i] = positions.size();
					positions.emplace_back(point);
				} else {
					inds[i] = it->second;
				}
			}
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

using FaceType = DynamicMesh::FaceType;

TetMesh finalCuts(HalfTetMesh h1parts, HalfTetMesh h2parts, const DynamicMesh &dmesh1,
				  const DynamicMesh &dmesh2, const vector<FaceType> &data1,
				  const vector<FaceType> &data2, TetMesh::CSGVisualData *vis_data) {
	vector<Triangle> tris;

	// TODO: handle situations when one mesh is inside another and the boundaries
	// do not touch each other

	// TODO: vertex positions still may differ by a small value; it has to be fixed!
	for(auto face : dmesh1.faces())
		if(data1[face] == FaceType::outside)
			tris.emplace_back(dmesh1.triangle(face));
	auto tris1_count = tris.size();

	for(auto face : dmesh2.faces())
		if(data2[face] == FaceType::inside)
			tris.emplace_back(dmesh2.triangle(face).inverse());

	Mesh fill_mesh = Mesh::makePolySoup(tris);
	if(vis_data && vis_data->phase == 3) {
		vis_data->poly_soups.emplace_back(Color::red,
										  vector<Triangle>(begin(tris), begin(tris) + tris1_count));
		vis_data->poly_soups.emplace_back(Color::green,
										  vector<Triangle>(begin(tris) + tris1_count, end(tris)));
	}

	bool is_valid = DynamicMesh(fill_mesh).representsVolume();
	DASSERT(is_valid);

	if(is_valid) {
		auto fill = HalfTetMesh(TetMesh::make(fill_mesh, 0));

		if(vis_data && vis_data->phase >= 4) {
			vis_data->tet_meshes.emplace_back(Color::red, TetMesh(fill));
			vis_data->tet_meshes.emplace_back(Color::green, TetMesh(h1parts));

			vector<Segment> segments;
			for(auto btri : fill_mesh.tris())
				for(auto edge : btri.edges())
					segments.emplace_back(edge.first, edge.second);
			vis_data->segment_groups.emplace_back(Color::blue, segments);
		}

		for(auto *tet : h1parts.tets())
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

	HalfTetMesh hmesh1_parts(a.extract(a.invertSelection(a.selection(csg_bbox))));
	HalfTetMesh hmesh2_parts(b.extract(b.invertSelection(b.selection(csg_bbox))));

	DynamicMesh dmesh1(TetMesh(hmesh1).toMesh());
	DynamicMesh dmesh2(TetMesh(hmesh2).toMesh());

	try {
		DASSERT(dmesh1.representsVolume());
		DASSERT(dmesh2.representsVolume());
		auto loops = dmesh1.findIntersections(dmesh2);

		if(vis_data) {
			vector<Segment> isect_lines;
			for(const auto &edge : loops.first)
				isect_lines.emplace_back(dmesh1.segment(edge.second));
			vis_data->segment_groups_trans.emplace_back(Color::black, isect_lines);

			if(vis_data->phase == 0) {
				vis_data->tet_meshes.emplace_back(Color(Color::red, 100), TetMesh(hmesh1));
				vis_data->tet_meshes.emplace_back(Color(Color::green, 100), TetMesh(hmesh2));
			}
		}

		dmesh1.triangulateFaces(loops.first);
		dmesh2.triangulateFaces(loops.second);

		if(vis_data && vis_data->phase == 1) {
			vector<Segment> segs1, segs2;
			for(auto face : dmesh1.faces())
				for(auto edge : dmesh1.edges(face))
					segs1.emplace_back(dmesh1.segment(edge));
			for(auto face : dmesh2.faces())
				for(auto edge : dmesh2.edges(face))
					segs2.emplace_back(dmesh2.segment(edge));
			vis_data->segment_groups.emplace_back(Color::yellow, segs1);
			vis_data->segment_groups.emplace_back(Color::blue, segs2);
		}

		auto ftypes2 = dmesh1.classifyFaces(dmesh2, loops.first, loops.second);
		auto ftypes1 = dmesh2.classifyFaces(dmesh1, loops.second, loops.first);

		if(vis_data && vis_data->phase == 2) {
			vector<Color> colors = {Color::black,  Color::green,   Color::red,
									Color::yellow, Color::magneta, Color::purple};
			vector<vector<Triangle>> segs;
			segs.clear();

			for(auto face : dmesh1.faces()) {
				int seg_id = (int)ftypes1[face];
				if((int)segs.size() < seg_id + 1)
					segs.resize(seg_id + 1);
				segs[seg_id].emplace_back(dmesh1.triangle(face));
			}
			/*	for(auto face : dmesh2.faces()) {
					int seg_id = 5;
					if((int)segs.size() < seg_id + 1)
						segs.resize(seg_id + 1);
					segs[seg_id].emplace_back(dmesh2.triangle(face));
				}*/

			for(int n = 0; n < (int)segs.size(); n++)
				vis_data->poly_soups.emplace_back(colors[n % colors.size()], segs[n]);
		}

		auto final =
			finalCuts(hmesh1_parts, hmesh2_parts, dmesh1, dmesh2, ftypes1, ftypes2, vis_data);
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
