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

TetMesh TetMesh::makeTetSoup(CRange<Tetrahedron> rtets) {
	vector<float3> positions;
	vector<TetIndices> indices;

	vector<Tetrahedron> tets(rtets);
	std::sort(begin(tets), end(tets), [](const auto &a, const auto &b) {
		auto ca = a.center(), cb = b.center();
		return std::tie(ca.x, ca.y, ca.z) < std::tie(cb.x, cb.y, cb.z);
	});

	for(const auto &tet : tets) {
		uint off = positions.size();
		TetIndices inds;
		const auto &verts = tet.verts();

		for(int i = 0; i < 4; i++) {
			auto vert = verts[i];
			auto result = findMin(positions, [vert](auto v) { return distanceSq(v, vert); });

			int index = 0;
			if(result.first != -1 && sqrtf(result.second) < constant::epsilon)
				index = result.first;
			else {
				index = positions.size();
				positions.emplace_back(vert);
			}
			inds[i] = index;
		}
		if(distinct(inds))
			indices.emplace_back(inds);
	}

	return TetMesh(std::move(positions), std::move(indices));
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

vector<Tetrahedron> TetMesh::tets() const {
	vector<Tetrahedron> out;
	out.reserve(size());
	for(int n = 0; n < size(); n++)
		out.emplace_back(makeTet(n));
	return out;
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

pair<vector<Tetrahedron>, vector<Tetrahedron>> splitTet(Tetrahedron tet, Plane plane) {
	auto dots = transform(tet.verts(), [&](auto vert) { return dot(plane, vert); });

	if(allOf(dots, [](auto dot) { return dot <= 0.0f; }))
		return {{}, {tet}};
	if(allOf(dots, [](auto dot) { return dot >= 0.0f; }))
		return {{tet}, {}};

	int neg = 0;
	for(auto dot : dots)
		neg += dot < 0.0f;

	if(neg == 1) {
		const auto &verts = tet.verts();

		int neg_id = 0;
		int pos_id[3];
		for(int n = 0, i = 0; n < 4; n++) {
			if(dots[n] < 0.0f)
				neg_id = n;
			else
				pos_id[i++] = n;
		}

		float isects[3];
		float3 points[3];

		for(int n = 0; n < 3; n++) {
			Segment seg(verts[neg_id], verts[pos_id[n]]);
			isects[n] = clamp(intersection(seg, plane), 0.0f, seg.length());
			points[n] = seg.at(isects[n]);
		}

		auto ntet = fixVolume(Tetrahedron(verts[neg_id], points[0], points[1], points[2]));
		Tetrahedron ptets[3] = {
			fixVolume(Tetrahedron(points[1], verts[pos_id[0]], verts[pos_id[1]], verts[pos_id[2]])),
			fixVolume(Tetrahedron(points[1], verts[pos_id[0]], verts[pos_id[2]], points[2])),
			fixVolume(Tetrahedron(verts[pos_id[0]], points[0], points[1], points[2]))};

		auto vol_sum = ntet.volume() + ptets[0].volume() + ptets[1].volume() + ptets[2].volume();
		DASSERT(fabs(vol_sum - tet.volume()) < constant::epsilon);

		return {{ptets[0], ptets[1], ptets[2]}, {ntet}};
	} else if(neg == 3) {
		auto out = splitTet(tet, -plane);
		return make_pair(out.second, out.first);
	} else {
		DASSERT(neg == 2);
		const auto &verts = tet.verts();

		int neg_id[2];
		int pos_id[2];
		for(int n = 0, i = 0, j = 0; n < 4; n++) {
			if(dots[n] < 0.0f)
				neg_id[i++] = n;
			else
				pos_id[j++] = n;
		}

		float isects[4];
		float3 points[4];
		for(int i = 0; i < 4; i++) {
			Segment seg(verts[neg_id[i / 2]], verts[pos_id[i % 2]]);
			isects[i] = clamp(intersection(seg, plane), 0.0f, seg.length());
			points[i] = seg.at(isects[i]);
		}

		array<Tetrahedron, 3> ptets = {{
			fixVolume(Tetrahedron(verts[pos_id[0]], verts[pos_id[1]], points[1], points[3])),
			fixVolume(Tetrahedron(verts[pos_id[0]], points[0], points[1], points[2])),
			fixVolume(Tetrahedron(verts[pos_id[0]], points[1], points[2], points[3])),
		}};
		array<Tetrahedron, 3> ntets = {{
			fixVolume(Tetrahedron(verts[neg_id[0]], verts[neg_id[1]], points[2], points[3])),
			fixVolume(Tetrahedron(verts[neg_id[0]], points[0], points[1], points[2])),
			fixVolume(Tetrahedron(verts[neg_id[0]], points[1], points[2], points[3])),
		}};
		float vol_sum = 0.0f;
		for(int i = 0; i < 3; i++)
			vol_sum += ptets[i].volume() + ntets[i].volume();
		DASSERT(fabs(vol_sum - tet.volume()) < constant::epsilon);

		return {{ptets[0], ptets[1], ptets[2]}, {ntets[0], ntets[1], ntets[2]}};
	}

	return {{tet}, {}};
}

vector<Tetrahedron> subtract(vector<Tetrahedron> tets, Tetrahedron b) {
	const auto &planes = b.planes();
	vector<Tetrahedron> out;

	for(auto tet : tets) {
		if(!areIntersecting(tet, b)) {
			//		out.emplace_back(tet);
			//		continue;
		}

		vector<Tetrahedron> current = {tet};
		for(auto plane : b.planes()) {
			vector<Tetrahedron> temp;
			for(auto ttet : current) {
				auto result = splitTet(ttet, plane);
				insertBack(out, result.first);
				insertBack(temp, result.second);
			}
			current.swap(temp);
			break;
		}
	}

	return out;
}

auto makePlane(const Plane &plane, float size) {
	DASSERT(size > constant::epsilon);

	float3 p[3] = {{-size, -size, -size}, {size, size, size}, {size, -size, size}};
	for(int i = 0; i < 3; i++)
		p[i] = closestPoint(plane, p[i]);
	if(distance(p[0], p[1]) < distance(p[0], p[2]))
		p[1] = p[2];

	float3 pnormal1 = normalize(p[1] - p[0]);
	float3 pnormal2 = cross(plane.normal(), pnormal1);

	float3 center = closestPoint(plane, float3());
	float3 corner[4] = {
		center - pnormal1 * size - pnormal2 * size, center + pnormal1 * size - pnormal2 * size,
		center + pnormal1 * size + pnormal2 * size, center - pnormal1 * size + pnormal2 * size,
	};

	return vector<Triangle>{
		{corner[0], corner[1], corner[2]},
		{corner[0], corner[2], corner[3]},
		{corner[0], corner[2], corner[1]},
		{corner[0], corner[3], corner[2]},
	};
}

TetMesh TetMesh::csg(const TetMesh &a, const TetMesh &b, CSGMode mode, CSGVisualData *vis_data) {
	// TODO: accuracy depends on bounding box coordinates, take them into consideration
	float tolerance = 0.01f;

	FBox csg_bbox = enlarge(intersection(a.computeBBox(), b.computeBBox()), tolerance);
	auto mesh1 = a; // a.extract(a.selection(csg_bbox));
	auto mesh2 = b; // b.extract(b.selection(csg_bbox));

	auto soup1 = mesh1.tets(), soup2 = mesh2.tets();
	Plane plane;

	vector<Tetrahedron> out = {soup1.front()};
	for(auto tet2 : soup2) {
		plane = tet2.planes()[0];
		out = subtract(out, tet2);
		break;
	}

	mesh2 = makeTetSoup(mesh2.tets());

	auto hmesh1_parts = a.extract(a.invertSelection(a.selection(csg_bbox)));
	auto hmesh2_parts = b.extract(b.invertSelection(b.selection(csg_bbox)));

	if(vis_data) {
		if(vis_data->phase == 0) {
			vis_data->tet_meshes.emplace_back(Color(Color::red, 100), mesh1);
			vis_data->tet_meshes.emplace_back(Color(Color::green, 100), mesh2);
		}
		if(vis_data->phase == 1) {
			vis_data->poly_soups.emplace_back(Color(Color::green, 100), makePlane(plane, 5.0f));
			//	vis_data->tet_meshes.emplace_back(Color(Color::green, 100), mesh2);
			vis_data->tet_meshes.emplace_back(Color(Color::red, 100), TetMesh::makeTetSoup(out));
		}
	}

	return {};
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
