/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <set>

namespace fwk {

using VertexId = DynamicMesh::VertexId;
using FaceId = DynamicMesh::FaceId;
using EdgeId = DynamicMesh::EdgeId;
using EdgeLoop = DynamicMesh::EdgeLoop;

namespace {

	vector<Segment> compatibleEdges(const Triangle &tri1, const Triangle &tri2, float eps) {
		Projection proj(tri1);
		Triangle ptri2(proj * tri2);

		vector<pair<float3, float3>> edges;

		bool vert_touching[3] = {false, false, false};
		float isect[3] = {constant::inf, constant::inf, constant::inf};

		for(int n = 0; n < 3; n++) {
			float3 v1 = ptri2[n], v2 = ptri2[(n + 1) % 3];
			if(fabs(v1.y) < eps) {
				vert_touching[n] = true;
				continue;
			}

			if((v1.y <= 0.0f) == (v2.y <= 0.0f))
				continue;

			isect[n] = -v1.y / (v2.y - v1.y);
		}

		float3 points[3];
		int npoints = 0;

		for(int n = 0; n < 3; n++) {
			if(vert_touching[n]) {
				points[npoints++] = ptri2[n];
			}
			int nn = (n + 1) % 3;
			if(isect[n] < constant::inf && !vert_touching[nn])
				points[npoints++] = ptri2[n] + (ptri2[nn] - ptri2[n]) * isect[n];
		}

		if(npoints == 2) {
			edges.emplace_back(points[0], points[1]);
		}
		if(npoints == 3) {
			for(int n = 0; n < 3; n++)
				edges.emplace_back(points[n], points[(n + 1) % 3]);
		}

		Triangle2D tri1_2d((proj * tri1).xz());
		vector<Segment> out;

		for(auto &edge : edges) {
			auto tedge = Segment2D(edge.first.xz(), edge.second.xz());
			if(tedge.empty())
				continue;

			// TODO: dont use clip method (which should be removed btw)
			auto result = clip(tri1_2d, tedge);
			auto clipped = result.inside;

			if(!clipped.empty())
				out.emplace_back(proj / asXZ(clipped.start), proj / asXZ(clipped.end));
		}

		return out;
	}
	struct FaceEdgeInfo {
		vector<EdgeId> edges;
		std::map<VertexId, int> border_verts;
	};

	using FaceEdgeMap = std::map<FaceId, FaceEdgeInfo>;

	EdgeId addEdge(DynamicMesh &mesh, Segment segment) {
		float epsilon = constant::epsilon;
		DASSERT(segment.length() > epsilon);
		auto v1 = mesh.closestVertex(segment.origin());
		auto v2 = mesh.closestVertex(segment.end());

		if(!v1.isValid() || distance(mesh.point(v1), segment.origin()) > epsilon)
			v1 = mesh.addVertex(segment.origin());
		if(!v2.isValid() || distance(mesh.point(v2), segment.end()) > epsilon)
			v2 = mesh.addVertex(segment.end());

		// TODO: what if v1 == v2? it might happen...
		DASSERT(v1 != v2);
		DASSERT(mesh.isValid(v1));
		DASSERT(mesh.isValid(v2));

		return EdgeId(v1, v2);
	}

	int findClosestEdge(DynamicMesh &mesh, FaceId face, VertexId vert, float tolerance) {
		if(isOneOf(vert, mesh.verts(face)))
			return -1;

		int closest = -1;
		float closest_dist = constant::inf;
		auto edges = mesh.edges(face);

		for(int i = 0; i < 3; i++) {
			auto edge = edges[i];
			float dist = distance(mesh.segment(edge), mesh.point(vert));
			if(dist < closest_dist) {
				closest = i;
				closest_dist = dist;
			}
		}

		if(closest_dist < tolerance)
			return closest;
		return -1;
	}

	void updateBorderVert(DynamicMesh &mesh, FaceEdgeMap &map, FaceId face, VertexId vert,
						  float tolerance) {
		int edge_id = findClosestEdge(mesh, face, vert, tolerance);

		if(edge_id != -1) {
			map[face].border_verts[vert] = edge_id;
			auto edge = mesh.faceEdge(face, edge_id);
			for(auto oface : mesh.faces(edge)) {
				if(oface == face)
					continue;

				int other_id = mesh.faceEdgeIndex(oface, edge.inverse());
				if(other_id == -1)
					other_id = mesh.faceEdgeIndex(oface, edge);
				DASSERT(other_id != -1);
				map[oface].border_verts[vert] = other_id;
			}
		}
	}

	void makeEdgesUnique(vector<EdgeId> &edges) {
		for(int e1 = 0; e1 < (int)edges.size(); e1++)
			for(int e2 = e1 + 1; e2 < (int)edges.size(); e2++)
				if(edges[e2] == edges[e1] || edges[e2].inverse() == edges[e1]) {
					edges[e2--] = edges.back();
					edges.pop_back();
				}
	}

	vector<VertexId> sortEdgeVerts(DynamicMesh &mesh, EdgeId edge, vector<VertexId> splits) {
		float3 ref_point = mesh.point(edge.a);
		auto pairs = transform(
			splits, [&](auto v) { return make_pair(distanceSq(mesh.point(v), ref_point), v); });
		std::sort(begin(pairs), end(pairs));
		return transform(pairs, [](auto pair) { return pair.second; });
	}

	float angleBetween(const DynamicMesh &mesh, VertexId vprev, VertexId vcur, VertexId vnext,
					   const Projection &proj) {
		DASSERT(vprev != vcur && vcur != vnext);

		float2 cur = (proj * mesh.point(vcur)).xz();
		float2 prev = (proj * mesh.point(vprev)).xz();
		float2 next = (proj * mesh.point(vnext)).xz();
		return angleBetween(prev, cur, next);
	}

	vector<float> computeAngles(const DynamicMesh &mesh, const vector<VertexId> &verts,
								const Projection &proj) {
		vector<float> out(verts.size());

		for(int n = 0, count = (int)verts.size(); n < count; n++) {
			auto prev = verts[(n + count - 1) % count];
			auto next = verts[(n + 1) % count];
			out[n] = angleBetween(mesh, prev, verts[n], next, proj);
		}
		return out;
	}

	// bedges: border edges
	// iedges: inside edges
	vector<vector<EdgeId>> findSimplePolygons(const DynamicMesh &mesh, const vector<EdgeId> &bedges,
											  const vector<EdgeId> &iedges, const Projection &proj,
											  bool do_print = false) {
		if(do_print) {
			xmlPrint("bedges: ");
			for(auto e : bedges)
				xmlPrint("(%-%)[%:%] ", (long long)e.a % 1337, (long long)e.b % 1337,
						 (proj * mesh.point(e.a)).xz(), (proj * mesh.point(e.b)).xz());
			xmlPrint("\n");
			xmlPrint("iedges: ");
			for(auto e : iedges)
				xmlPrint("(%-%)[%:%] ", (long long)e.a % 1337, (long long)e.b % 1337,
						 (proj * mesh.point(e.a)).xz(), (proj * mesh.point(e.b)).xz());
			xmlPrint("\n");
		}

		std::map<VertexId, vector<EdgeId>> map;
		for(auto bedge : bedges)
			map[bedge.a].emplace_back(bedge);
		for(auto iedge : iedges) {
			map[iedge.a].emplace_back(iedge);
			map[iedge.b].emplace_back(iedge.inverse());
		}

		// Creating bridges between unconnected edges
		while(true) {
			vector<VertexId> stack = {map.begin()->first};
			vector<int> temps(mesh.vertexIdCount(), 0);

			int count = 0;
			while(!stack.empty()) {
				VertexId vert = stack.back();
				stack.pop_back();
				if(temps[vert])
					continue;

				temps[vert] = 1;
				count++;

				for(auto edge : map[vert])
					stack.push_back(edge.a == vert ? edge.b : edge.a);
			}

			if(count == (int)map.size())
				break;

			EdgeId best_edge;
			float max_distance = 0.0f; // TODO: is it a good idea?

			for(const auto &it1 : map)
				if(temps[it1.first] == 1)
					for(const auto &it2 : map)
						if(temps[it2.first] == 0) {
							EdgeId edge(it1.first, it2.first);

							float dist = constant::inf;
							for(const auto &it3 : map)
								for(auto tedge : it3.second)
									if(!tedge.hasSharedEnds(edge)) {
										float edist =
											distance(mesh.segment(edge), mesh.segment(tedge));
										dist = min(dist, edist);
									}
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

		vector<vector<EdgeId>> out;

		while(!map.empty()) {
			auto it = map.begin();
			DASSERT(!it->second.empty());
			EdgeId start = it->second.back();
			it->second.pop_back();
			if(it->second.empty())
				map.erase(it);

			vector<EdgeId> loop = {start};
			if(do_print)
				xmlPrint("Start: %-%\n", (long long)start.a % 1337, (long long)start.b % 1337);

			while(true) {
				VertexId current = loop.back().b;
				auto it = map.find(current);

				if(it == map.end()) {
					DASSERT(current == start.a);
					break;
				}

				float min_angle = constant::inf;
				EdgeId best_edge;

				for(auto edge : it->second) {
					if(edge.b == loop.back().a)
						continue;
					float angle = angleBetween(mesh, loop.back().a, current, edge.b, proj);

					if(do_print)
						xmlPrint("Consider: %-% (%)\n", (long long)edge.a % 1337,
								 (long long)edge.b % 1337, radToDeg(angle));
					if(angle < min_angle) {
						min_angle = angle;
						best_edge = edge;
					}
				}

				if(current == start.a && loop.back().a != start.b) {
					float start_angle = angleBetween(mesh, loop.back().a, current, start.b, proj);
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
						findSimplePolygons(mesh, bedges, iedges, proj, true);
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
	vector<array<VertexId, 3>> triangulateSimplePolygon(const DynamicMesh &mesh,
														const vector<EdgeId> &edges,
														const Projection &proj) {
		vector<array<VertexId, 3>> out;

		vector<VertexId> verts(edges.size());
		for(int e = 0; e < (int)edges.size(); e++) {
			DASSERT(edges[e].b == edges[(e + 1) % edges.size()].a);
			verts[e] = edges[e].a;
		}

		vector<float> angles = computeAngles(mesh, verts, proj);
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
			vector<float> angles = computeAngles(mesh, verts, proj);

			int best_vert = -1;
			float best_angle = constant::pi;
			std::array<VertexId, 3> best_ear;

			for(int n = 0, count = (int)verts.size(); n < count; n++) {
				if(angles[n] > best_angle)
					continue;

				VertexId cur = verts[n];
				VertexId prev = verts[(n + count - 1) % count];
				VertexId next = verts[(n + 1) % count];
				Triangle2D new_tri((proj * mesh.point(prev)).xz(), (proj * mesh.point(cur)).xz(),
								   (proj * mesh.point(next)).xz());

				float min_dist = constant::inf;
				for(int i = 0; i < count; i++)
					if(!isOneOf(verts[i], cur, prev, next))
						min_dist =
							min(min_dist, distance(new_tri, (proj * mesh.point(verts[i])).xz()));

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

	vector<array<VertexId, 3>> triangulateFace(const DynamicMesh &mesh, FaceId face,
											   const vector<EdgeId> &bedges,
											   const vector<EdgeId> &iedges) {
		Projection proj(mesh.triangle(face));

		vector<vector<EdgeId>> simple_polys = findSimplePolygons(mesh, bedges, iedges, proj);

		vector<array<VertexId, 3>> out;
		for(const auto &poly : simple_polys)
			insertBack(out, triangulateSimplePolygon(mesh, poly, proj));
		return out;
	}
}

void DynamicMesh::triangulateFaces(EdgeLoop loop, float tolerance) {
	vector<FaceId> rem_faces;
	vector<array<VertexId, 3>> new_faces;

	FaceEdgeMap face_edge_map;
	for(auto ledge : loop) {
		face_edge_map[ledge.first].edges.emplace_back(ledge.second);
		updateBorderVert(*this, face_edge_map, ledge.first, ledge.second.a, tolerance);
		updateBorderVert(*this, face_edge_map, ledge.first, ledge.second.b, tolerance);
	}

	for(auto pair : face_edge_map) {
		makeEdgesUnique(pair.second.edges);

		auto face = pair.first;
		auto face_edges = edges(face);
		auto &border_verts = pair.second.border_verts;

		array<vector<VertexId>, 3> edge_verts;
		rem_faces.emplace_back(face);

		for(auto bvert : border_verts) {
			DASSERT(bvert.second >= 0 && bvert.second < 3);
			edge_verts[bvert.second].emplace_back(bvert.first);
		}

		vector<EdgeId> tri_boundary_edges, tri_inside_edges;
		for(int i = 0; i < 3; i++) {
			auto edge = face_edges[i];
			if(edge_verts[i].empty()) {
				tri_boundary_edges.emplace_back(edge.a, edge.b);
				continue;
			}

			auto splits = sortEdgeVerts(*this, edge, edge_verts[i]);

			tri_boundary_edges.emplace_back(edge.a, splits.front());
			//			DASSERT(distance(edge.segment(), splits.front()->pos()) <
			// constant::epsilon);
			for(int i = 0; i + 1 < (int)splits.size(); i++) {
				tri_boundary_edges.emplace_back(splits[i], splits[i + 1]);
				DASSERT(distance(segment(edge), point(splits[i])) < constant::epsilon);
			}
			tri_boundary_edges.emplace_back(splits.back(), edge.b);
		}

		for(int i = 0; i < 3; i++)
			insertBack(edge_verts[i], {face_edges[i].a, face_edges[i].b});

		for(auto edge : pair.second.edges) {
			bool on_border = false;
			for(int i = 0; i < 3; i++)
				if(isOneOf(edge.a, edge_verts[i]) && isOneOf(edge.b, edge_verts[i]))
					on_border = true;
			if(!on_border) {
				// TODO: it can be on the border, but not present in tri_boundary_edges
				tri_inside_edges.emplace_back(edge.ordered());
			}
		}

		//		printf("triangulating face (i:%d b:%d)\n", (int)tri_inside_edges.size(),
		//			   (int)tri_boundary_edges.size());
		auto triangles = triangulateFace(*this, face, tri_boundary_edges, tri_inside_edges);
		insertBack(new_faces, triangles);
		//		printf("new tris: %d\n", (int)triangles.size());

		/*if(vis_data && vis_data->phase == 1) {
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
		DASSERT(fabs(area_sum - face->triangle().area()) < constant::epsilon);*/
	}

	for(auto face : rem_faces)
		remove(face);
	for(auto new_face : new_faces)
		addFace(new_face[0], new_face[1], new_face[2]);

	/*
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
	DASSERT(fabs(new_volume - old_volume) < constant::epsilon);*/
}

pair<EdgeLoop, EdgeLoop> DynamicMesh::findIntersections(DynamicMesh &rhs, float tolerance) {
	EdgeLoop loop1, loop2;

	for(auto face1 : faces())
		for(auto face2 : rhs.faces()) {
			auto edges = compatibleEdges(triangle(face1), rhs.triangle(face2), tolerance);
			insertBack(edges, compatibleEdges(rhs.triangle(face2), triangle(face1), tolerance));

			for(auto edge : edges) {
				loop1.emplace_back(face1, addEdge(*this, edge));
				loop2.emplace_back(face2, addEdge(rhs, edge));
			}
		}

	return make_pair(loop1, loop2);
}

using FaceType = DynamicMesh::FaceType;

static void floodFill(const DynamicMesh &mesh, vector<FaceId> list, EdgeLoop limits_vec,
					  vector<FaceType> &data) {
	std::set<EdgeId> limits;
	for(auto pair : limits_vec)
		limits.insert(pair.second.ordered());

	while(!list.empty()) {
		FaceId face = list.back();
		list.pop_back();

		for(auto edge : mesh.edges(face))
			if(!limits.count(edge.ordered()))
				for(auto nface : mesh.faces(edge))
					if(data[nface] == FaceType::unclassified) {
						data[nface] = data[face];
						list.emplace_back(nface);
					}
	}
}

vector<FaceType> DynamicMesh::classifyFaces(const DynamicMesh &mesh2, const EdgeLoop &loop1,
											const EdgeLoop &loop2) const {
	vector<FaceType> out(mesh2.faceIdCount(), FaceType::unclassified);

	vector<FaceId> list1, list2;
	const auto &mesh1 = *this;

	DASSERT(loop1.size() == loop2.size());
	for(int i = 0; i < (int)loop1.size(); i++) {
		auto edge1 = loop1[i].second;
		auto edge2 = loop2[i].second;

		auto faces1 = mesh1.faces(edge1);
		auto faces2 = mesh2.faces(edge2);
		DASSERT(faces1.size() == 2 && faces2.size() == 2);

		Projection proj = mesh1.edgeProjection(edge1, faces1[0]);

		float2 vectors1[2] = {(proj * mesh1.point(mesh1.otherVertex(faces1[0], edge1))).xz(),
							  (proj * mesh1.point(mesh1.otherVertex(faces1[1], edge1))).xz()};
		float2 vectors2[2] = {(proj * mesh2.point(mesh2.otherVertex(faces2[0], edge2))).xz(),
							  (proj * mesh2.point(mesh2.otherVertex(faces2[1], edge2))).xz()};

		float2 normals1[2] = {(proj.projectVector(mesh1.triangle(faces1[0]).normal())).xz(),
							  (proj.projectVector(mesh1.triangle(faces1[1]).normal())).xz()};
		float2 normals2[2] = {(proj.projectVector(mesh2.triangle(faces2[0]).normal())).xz(),
							  (proj.projectVector(mesh2.triangle(faces2[1]).normal())).xz()};

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

			auto value = FaceType::shared;
			if(angle < -eps)
				value = FaceType::inside;
			else if(angle < eps)
				value = dir2[n] < 0.0f ? FaceType::shared_opposite : FaceType::shared;
			else if(angle < face1_angle - eps)
				value = FaceType::outside;
			else if(angle < face1_angle + eps)
				value = dir2[n] < 0.0f ? FaceType::shared : FaceType::shared_opposite;
			else
				value = FaceType::inside;

			auto old = out[faces2[n]];
			if(old != FaceType::unclassified && old != value) {
				printf("Error: %d -> %d\n", old, value);
				printf("dir1: %f %f\ndir2: %f %f\nangles: %f %f\n\n", dir1[0], dir1[1], dir2[0],
					   dir2[1], angle, face1_angle);
			}
			out[faces2[n]] = value;
		}

		insertBack(list2, faces2);
	}

	floodFill(mesh2, list2, loop2, out);
	return out;
}

void DynamicMesh::makeCool(float tolerance) {
	// TODO: write me
}

DynamicMesh DynamicMesh::csgDifference(const DynamicMesh &a, const DynamicMesh &b,
									   CSGVisualData *vis_data) {
	// TODO: make sure that faces are properly marked
	vector<int> faceop(a.faceCount(), 0);
	faceop.resize(a.faceCount() + b.faceCount(), 1);

	float epsilon = 0.01f;

	auto merged = DynamicMesh::merge({a, b});

	if(vis_data && vis_data->phase == 0) {
		vector<Triangle> tris[2];
		for(auto face : merged.faces())
			tris[faceop[face]].emplace_back(merged.triangle(face));
		vis_data->poly_soups.emplace_back(Color::red, tris[0]);
		vis_data->poly_soups.emplace_back(Color::green, tris[1]);
	}

	merged.makeCool(epsilon);

	if(vis_data && vis_data->phase == 1) {
		vector<Triangle> tris[2];
		vector<Segment> segs;

		for(auto face : merged.faces()) {
			tris[faceop[face]].emplace_back(merged.triangle(face));
			for(auto edge : merged.edges(face))
				segs.emplace_back(merged.segment(edge));
			vis_data->poly_soups.emplace_back(Color::red, tris[0]);
			vis_data->poly_soups.emplace_back(Color::green, tris[1]);
			vis_data->segment_groups.emplace_back(Color::black, segs);
		}
	}
	return merged;
}
}
