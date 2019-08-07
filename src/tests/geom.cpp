// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom/contour.h"
#include "fwk/geom/geom_graph.h"
#include "fwk/geom/plane_graph.h"
#include "fwk/geom/regular_grid.h"
#include "fwk/math/random.h"
#include "fwk/math/rotation.h"
#include "testing.h"

static void orderByDirectionTest() {
	vector<double2> vecs{{1.1, 0.0},  {3.0, 3.0},	{1.0, 5.0},  {-2.0, 4.0},
						 {-3.0, 0.0}, {-1.0, -10.0}, {5.0, -20.0}};
	Random rand(123);

	for(int n = 0; n < 1024; n++) {
		auto rads = rand.uniform(0.0, pi * 2.0);
		auto tvecs = vecs;
		for(auto vec : tvecs)
			vec = rotateVector(vec, rads);
		vector<int> ids = transform<int>(intRange(tvecs));
		rand.permute<int>(ids);
		orderByDirection<double2>(ids, vecs, double2(1, 0));

		ASSERT_EQ(ids, (vector<int>{{{0, 1, 2, 3, 4, 5, 6}}}));
	}
}

static void testImmutableGraph() {
	vector<pair<int, int>> pairs1 = {{0, 1}, {1, 2}, {2, 0}};
	vector<pair<int, int>> pairs2 = {{0, 1}, {0, 2}, {2, 3}, {1, 3}};

	ImmutableGraph graph1(pairs1.reinterpret<Pair<VertexId>>());
	ImmutableGraph graph2(pairs2.reinterpret<Pair<VertexId>>());

	ASSERT(graph1.hasCycles());
	ASSERT(!graph2.hasCycles());
}

static void testRegularGrid() {
	DRect rect(-10, -10, 10, 10);

	RegularGrid<double2> grid(rect, 0.5);
	ASSERT_EQ(grid.toCell(double2(-10.1, -9.9)), int2(-1, 0));
	ASSERT_EQ(grid.toCell(double2(10, 10.0)), int2(40, 40));

	IRect rect2(-100, -100, 100, 100);
	RegularGrid<int2> grid2(rect2, 10);
	ASSERT_EQ(grid2.size(), int2(20, 20));
	ASSERT_EQ(grid2.worldRect(), rect2);
	ASSERT_EQ(grid2.toCell(int2(0, 0)), int2(10, 10));

	IRect rect3(0, 0, 6, 6);
	RegularGrid<int2> grid3(rect3, 2);
	ASSERT_EQ(grid3.toCellRect({1, 1, 2, 2}), IRect(0, 0, 2, 2));

	DRect rect4(0, 0, 6, 6);
	RegularGrid<double2> grid4(rect4, 2.0);
	DRect drect(1, 1, 2.001, 2);
	ASSERT_EQ(grid4.toCellRect(drect), IRect(0, 0, 2, 2));
}

static void testContour() {
	vector<double2> points = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
	Contour<double2> contour(points, true);

	//print("Normals:\n");
	//for(auto ei : indexRange<EdgeId>(contour.numEdges()))
	//	print("%\n", contour.normal(ei));
	ASSERT_GE(contour.edgeSide(EdgeId(0), double2(0.5, 0.5)), 0.0);
}

static void testPlaneGraph() {
	vector<double2> points = {double2(-1, 0), double2(0, 0),   double2(1, 0),
							  double2(0, -1), double2(-1, -1), double2(0, 1)};
	vector<Pair<VertexId>> tnodes;
	tnodes.emplace_back(VertexId(1), VertexId(0));
	tnodes.emplace_back(VertexId(3), VertexId(1));
	tnodes.emplace_back(VertexId(2), VertexId(1));
	tnodes.emplace_back(VertexId(0), VertexId(4));
	tnodes.emplace_back(VertexId(1), VertexId(5));

	PlaneGraph<double2> pgraph(tnodes, points);
	auto contours = pgraph.contourLoops();
	ASSERT(contours.size() == 1);

	vector<float2> target_points{{0, 0}, {-1, 0}, {-1, -1}, {-1, 0}, {0, 0},
								 {0, 1}, {0, 0},  {1, 0},   {0, 0},  {0, -1}};
	ASSERT(transform<float2>(contours.front().points()) == target_points);
}

static void testGraph() NOINLINE;
static void testGraph() {
	GeomGraph<float2> graph;
	float2 points[] = {float2(-1, 0), float2(0, 0),   float2(1, 0),
					   float2(0, -1), float2(-1, -1), float2(0, 1)};
	int indices[][2] = {{1, 0}, {3, 1}, {2, 1}, {0, 4}, {1, 5}};

	for(auto pt : points)
		graph.add(pt);

	for(int n : intRange(indices)) {
		auto eid = graph.add(points[indices[n][0]], points[indices[n][1]]);
		auto eref = graph.ref(eid);
		graph[eref.from()].ival1 += 1;
	}

	ASSERT_EQ(graph[VertexId(1)].ival1, 2);
	// TODO: better tests?
}

void testMain() {
	testContour();
	testImmutableGraph();
	testRegularGrid();
	testPlaneGraph();
	orderByDirectionTest();
	testGraph();
}
