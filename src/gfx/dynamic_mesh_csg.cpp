/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_gfx.h"
#include "fwk_xml.h"
#include <set>

namespace fwk {

using VertexId = DynamicMesh::VertexId;
using PolyId = DynamicMesh::PolyId;
using EdgeId = DynamicMesh::EdgeId;
using EdgeLoop = DynamicMesh::EdgeLoop;
using Simplex = DynamicMesh::Simplex;

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

	using FaceEdgeMap = std::map<PolyId, FaceEdgeInfo>;

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

	int findClosestEdge(DynamicMesh &mesh, PolyId face, VertexId vert, float tolerance) {
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

	void updateBorderVert(DynamicMesh &mesh, FaceEdgeMap &map, PolyId face, VertexId vert,
						  float tolerance) {
		int edge_id = findClosestEdge(mesh, face, vert, tolerance);

		if(edge_id != -1) {
			map[face].border_verts[vert] = edge_id;
			auto edge = mesh.polyEdge(face, edge_id);
			for(auto oface : mesh.polys(edge)) {
				if(oface == face)
					continue;

				int other_id = mesh.polyEdgeIndex(oface, edge.inverse());
				if(other_id == -1)
					other_id = mesh.polyEdgeIndex(oface, edge);
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

	int floodFill(std::map<VertexId, vector<VertexId>> &map, vector<int> &values, VertexId start) {
		vector<VertexId> stack = {start};
		int count = 0;
		while(!stack.empty()) {
			VertexId vert = stack.back();
			stack.pop_back();
			if(values[vert])
				continue;
			values[vert] = 1;
			count++;

			for(auto target : map[vert])
				stack.push_back(target);
		}
		return count;
	}

	EdgeId findBestEdge(const DynamicMesh &mesh, std::map<VertexId, vector<VertexId>> &map,
						const vector<int> &values) {
		EdgeId best_edge;
		float max_distance = 0.0f; // TODO: is it a good idea?

		for(const auto &it1 : map)
			if(values[it1.first] == 1)
				for(const auto &it2 : map)
					if(values[it2.first] == 0) {
						EdgeId edge(it1.first, it2.first);

						bool invalid = false;
						float dist = constant::inf;
						for(const auto &it3 : map)
							for(auto ttarget : it3.second) {
								EdgeId tedge(it3.first, ttarget);

								if(tedge == edge.inverse() || tedge == edge)
									invalid = true;
								if(!tedge.hasSharedEnds(edge)) {
									float edist = distance(mesh.segment(edge), mesh.segment(tedge));
									dist = min(dist, edist);
								}
							}
						if(dist > max_distance && !invalid) {
							best_edge = edge;
							max_distance = dist;
						}
					}

		DASSERT(best_edge.isValid());
		DASSERT(max_distance >= constant::epsilon); // TODO: check if this could happen

		return best_edge;
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

		std::map<VertexId, vector<VertexId>> map;
		for(auto bedge : bedges)
			map[bedge.a].emplace_back(bedge.b);
		for(auto iedge : iedges) {
			map[iedge.a].emplace_back(iedge.b);
			if(iedge.a != iedge.b)
				map[iedge.b].emplace_back(iedge.a);
		}

		// Creating bridges between unconnected edges
		while(true) {
			vector<int> temps(mesh.vertexIdCount(), 0);
			if(floodFill(map, temps, map.begin()->first) == (int)map.size())
				break;

			EdgeId best_edge = findBestEdge(mesh, map, temps);

			if(do_print)
				xmlPrint("Adding bridge: %-%\n", (long long)best_edge.a % 1337,
						 (long long)best_edge.b % 1337);
			map[best_edge.a].emplace_back(best_edge.b);
			map[best_edge.b].emplace_back(best_edge.a);
		}

		vector<vector<EdgeId>> out;

		// Removing single verts
		for(auto &it : map) {
			for(int n = 0; n < (int)it.second.size(); n++)
				if(it.second[n] == it.first) {
					it.second[n--] = it.second.back();
					it.second.pop_back();
				}
		}

		vector<int> vert_neighbour_counts(mesh.vertexIdCount(), 0);
		{
			vector<EdgeId> all_edges;
			for(auto it : map)
				for(auto target : it.second)
					all_edges.emplace_back(EdgeId(it.first, target).ordered());
			makeUnique(all_edges);
			for(auto edge : all_edges) {
				vert_neighbour_counts[edge.a]++;
				vert_neighbour_counts[edge.b]++;
			}
		}

		for(auto it : map) {
			if(vert_neighbour_counts[it.first] != 1)
				continue;

			vector<int> temps(mesh.vertexIdCount(), 0);
			temps[it.first] = 1;
			EdgeId best_edge = findBestEdge(mesh, map, temps);

			if(do_print)
				xmlPrint("Linking: %-%\n", (long long)best_edge.a % 1337,
						 (long long)best_edge.b % 1337);
			map[best_edge.a].emplace_back(best_edge.b);
			map[best_edge.b].emplace_back(best_edge.a);
			vert_neighbour_counts[best_edge.a]++;
			vert_neighbour_counts[best_edge.b]++;
		}

		while(!map.empty()) {
			auto it = map.begin();
			DASSERT(!it->second.empty());
			EdgeId start(it->first, it->second.back());
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

				for(auto target : it->second) {
					if(target == loop.back().a)
						continue;
					float angle = angleBetween(mesh, loop.back().a, current, target, proj);

					if(do_print)
						xmlPrint("Consider: %-% (%)\n", (long long)it->first % 1337,
								 (long long)target % 1337, radToDeg(angle));
					if(angle < min_angle) {
						min_angle = angle;
						best_edge = EdgeId(it->first, target);
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
					if(EdgeId(it->first, it->second[i]) == best_edge) {
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

	vector<array<VertexId, 3>> triangulateFace(const DynamicMesh &mesh, const Triangle &tri,
											   const vector<EdgeId> &bedges,
											   const vector<EdgeId> &iedges) {
		Projection proj(tri);

		vector<vector<EdgeId>> simple_polys = findSimplePolygons(mesh, bedges, iedges, proj);

		vector<array<VertexId, 3>> out;
		for(const auto &poly : simple_polys)
			insertBack(out, triangulateSimplePolygon(mesh, poly, proj));
		return out;
	}
}

void DynamicMesh::triangulateFaces(EdgeLoop loop, float tolerance) {
	vector<PolyId> rem_faces;
	vector<array<VertexId, 3>> new_faces;
	DASSERT(isTriangular());

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
		auto triangles =
			triangulateFace(*this, triangle(face), tri_boundary_edges, tri_inside_edges);
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
		addPoly(new_face[0], new_face[1], new_face[2]);

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
	DASSERT(isTriangular() && rhs.isTriangular());

	for(auto face1 : polys())
		for(auto face2 : rhs.polys()) {
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

static void floodFill(const DynamicMesh &mesh, vector<PolyId> list, EdgeLoop limits_vec,
					  vector<FaceType> &data) {
	std::set<EdgeId> limits;
	for(auto pair : limits_vec)
		limits.insert(pair.second.ordered());

	while(!list.empty()) {
		PolyId face = list.back();
		list.pop_back();

		for(auto edge : mesh.edges(face))
			if(!limits.count(edge.ordered()))
				for(auto nface : mesh.polys(edge))
					if(data[nface] == FaceType::unclassified) {
						data[nface] = data[face];
						list.emplace_back(nface);
					}
	}
}

vector<FaceType> DynamicMesh::classifyFaces(const DynamicMesh &mesh2, const EdgeLoop &loop1,
											const EdgeLoop &loop2) const {
	vector<FaceType> out(mesh2.polyIdCount(), FaceType::unclassified);
	DASSERT(isTriangular() && mesh2.isTriangular());

	vector<PolyId> list1, list2;
	const auto &mesh1 = *this;

	DASSERT(loop1.size() == loop2.size());
	for(int i = 0; i < (int)loop1.size(); i++) {
		auto edge1 = loop1[i].second;
		auto edge2 = loop2[i].second;

		auto faces1 = mesh1.polys(edge1);
		auto faces2 = mesh2.polys(edge2);
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

template <class T> struct Grid {
	Grid(FBox bbox, int3 size, float tolerance)
		: m_bbox(bbox), m_size(size), m_tolerance(tolerance) {
		m_bbox = enlarge(m_bbox, tolerance);
		m_cells.resize(size.x * size.y * size.z);
		m_cell_size = m_bbox.size() / float3(m_size);
		m_inv_cell_size = inv(m_cell_size);
	}

	IBox cellRange(FBox box) const {
		float3 offset(m_tolerance, m_tolerance, m_tolerance);
		float3 min_pos = (box.min - m_bbox.min - offset) * m_inv_cell_size;
		float3 max_pos = (box.max - m_bbox.min + offset) * m_inv_cell_size;
		return IBox(max(int3(0, 0, 0), int3(min_pos)), min(m_size, int3(max_pos) + int3(1, 1, 1)));
	}

	int cellId(int3 pos) const { return pos.x + m_size.x * (pos.y + m_size.y * pos.z); }

	void remove(T simplex) {}

	void add(T simplex, FBox box) {
		auto range = cellRange(box);
		DASSERT(!range.isEmpty());
		for(int x = range.min.x; x < range.max.x; x++)
			for(int y = range.min.y; y < range.max.y; y++)
				for(int z = range.min.z; z < range.max.z; z++)
					m_cells[cellId({x, y, z})].emplace_back(simplex);
	}

	vector<T> find(FBox box) const {
		auto range = cellRange(box);
		DASSERT(!range.isEmpty());
		vector<T> out;
		for(int x = range.min.x; x < range.max.x; x++)
			for(int y = range.min.y; y < range.max.y; y++)
				for(int z = range.min.z; z < range.max.z; z++)
					insertBack(out, m_cells[cellId({x, y, z})]);
		makeUnique(out);
		return out;
	}

	vector<vector<T>> m_cells;
	FBox m_bbox;
	float3 m_cell_size, m_inv_cell_size;
	float m_tolerance;
	int3 m_size;
};

DynamicMesh DynamicMesh::csgDifference(DynamicMesh dmesh1, DynamicMesh dmesh2,
									   CSGVisualData *vis_data) {
	float tolerance = 0.001f;

	if(vis_data && vis_data->phase == 0) {
		vector<Triangle> tris[2];
		for(auto poly : dmesh1.polys())
			tris[0].emplace_back(dmesh1.triangle(poly));
		for(auto poly : dmesh2.polys())
			tris[1].emplace_back(dmesh2.triangle(poly));
		vis_data->poly_soups.emplace_back(Color(Color::red, 140), tris[0]);
		vis_data->poly_soups.emplace_back(Color(Color::green, 140), tris[1]);
	}

	try {
		DASSERT(dmesh1.representsVolume());
		DASSERT(dmesh2.representsVolume());
		auto loops = dmesh1.findIntersections(dmesh2, tolerance);

		vector<float3> new_points(loops.first.size());

		for(int i = 0, size = (int)loops.first.size(); i < size; i++) {
			const auto &elem1 = loops.first[i], &elem2 = loops.second[i];
			auto edge1 = elem1.second, edge2 = elem2.second;
			auto pos1 = (dmesh1.point(edge1.a) + dmesh2.point(edge2.a)) * 0.5f;
			auto pos2 = (dmesh1.point(edge1.b) + dmesh2.point(edge2.b)) * 0.5f;
			dmesh1.move(edge1.a, pos1);
			dmesh1.move(edge1.b, pos2);
			dmesh2.move(edge2.a, pos1);
			dmesh2.move(edge2.b, pos2);
		}

		if(vis_data) {
			vector<Segment> isect_lines;
			vector<Triangle> tris[2];
			vector<Segment> tri_segs[2];
			for(const auto &edge : loops.first)
				isect_lines.emplace_back(dmesh1.segment(edge.second));
			vis_data->segment_groups_trans.emplace_back(Color::black, isect_lines);
			for(auto elem : loops.first) {
				tris[0].emplace_back(dmesh1.triangle(elem.first));
				for(auto edge : dmesh1.edges(elem.first))
					tri_segs[0].emplace_back(dmesh1.segment(edge));
			}
			for(auto elem : loops.second) {
				tris[1].emplace_back(dmesh2.triangle(elem.first));
				for(auto edge : dmesh2.edges(elem.first))
					tri_segs[1].emplace_back(dmesh2.segment(edge));
			}

			if(vis_data->phase == 0) {
				auto points1 =
					transform(dmesh1.verts(), [&](auto vert) { return dmesh1.point(vert); });
				auto points2 =
					transform(dmesh2.verts(), [&](auto vert) { return dmesh2.point(vert); });
				//		vis_data->point_sets.emplace_back(Color::black, points1);
				//		vis_data->point_sets.emplace_back(Color::black, points2);
				vis_data->poly_soups.emplace_back(Color::red, tris[0]);
				vis_data->poly_soups.emplace_back(Color::green, tris[1]);
				vis_data->segment_groups.emplace_back(Color::yellow, tri_segs[0]);
				vis_data->segment_groups.emplace_back(Color::blue, tri_segs[1]);
			}
		}

		dmesh1.triangulateFaces(loops.first, tolerance);
		dmesh2.triangulateFaces(loops.second, tolerance);

		if(vis_data && vis_data->phase == 1) {
			vector<Segment> segs1, segs2;
			for(auto face : dmesh1.polys())
				for(auto edge : dmesh1.edges(face))
					segs1.emplace_back(dmesh1.segment(edge));
			for(auto face : dmesh2.polys())
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

			for(auto face : dmesh1.polys()) {
				int seg_id = (int)ftypes1[face];
				if((int)segs.size() < seg_id + 1)
					segs.resize(seg_id + 1);
				segs[seg_id].emplace_back(dmesh1.triangle(face));
			}
			/*	for(auto face : dmesh2.polys()) {
					int seg_id = 5;
					if((int)segs.size() < seg_id + 1)
						segs.resize(seg_id + 1);
					segs[seg_id].emplace_back(dmesh2.triangle(face));
				}*/

			for(int n = 0; n < (int)segs.size(); n++)
				vis_data->poly_soups.emplace_back(colors[n % colors.size()], segs[n]);
		}

		vector<Triangle> tris;
		for(auto face : dmesh1.polys())
			if(ftypes1[face] == FaceType::outside)
				tris.emplace_back(dmesh1.triangle(face));
		int num_outside = tris.size();
		for(auto face : dmesh2.polys())
			if(ftypes2[face] == FaceType::inside)
				tris.emplace_back(dmesh2.triangle(face).inverse());

		if(vis_data && vis_data->phase >= 3) {
			vis_data->poly_soups.emplace_back(Color::red, subRange(tris, 0, num_outside));
			vis_data->poly_soups.emplace_back(Color::green,
											  subRange(tris, num_outside, tris.size()));
			vector<Segment> segs;
			for(auto tri : tris)
				insertBack(segs, transform<Segment>(tri.edges()));
			if(vis_data->phase == 3)
				vis_data->segment_groups.emplace_back(Color::blue, segs);
		}

		return DynamicMesh(Mesh::makePolySoup(tris));
	} catch(const Exception &ex) {
		printf("Error: %s\n%s\n\n", ex.what(), ex.backtrace(true).c_str());
		return {};
	}
}
}
