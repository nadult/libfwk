// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom/contour.h"
#include "fwk/geom/delaunay.h"
#include "fwk/geom/geom_graph.h"
#include "fwk/geom/regular_grid.h"
#include "fwk/geom/segment_grid.h"
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

/*
static void testImmutableGraph() {
	vector<pair<int, int>> pairs1 = {{0, 1}, {1, 2}, {2, 0}};
	vector<pair<int, int>> pairs2 = {{0, 1}, {0, 2}, {2, 3}, {1, 3}};

	ImmutableGraph graph1(pairs1.reinterpret<Pair<VertexId>>());
	ImmutableGraph graph2(pairs2.reinterpret<Pair<VertexId>>());

	ASSERT(graph1.hasCycles());
	ASSERT(!graph2.hasCycles());
}*/

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

/*
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
}*/

void testDelaunayFuncs() {
	int2 quad1[4] = {{0, 0}, {10000, 0}, {10000, 10000}, {0, 10000}};
	ASSERT(isPositiveConvexQuad(quad1));

	int2 quad2[4] = {{0, 0}, {3, -4}, {6, 1}, {2, 6}};
	ASSERT(isPositiveConvexQuad(quad2));

	int2 quad3[4] = {{3, 0}, {0, 6}, {0, 0}, {-2, -5}};
	ASSERT(!isPositiveConvexQuad(quad3));

	int2 quad4[4] = {{0, 0}, {2, 1}, {0, 2}, {0, 1}};
	ASSERT(!isPositiveConvexQuad(quad4));

	ASSERT(!insideCircumcircle(quad1[0], quad1[1], quad1[2], quad1[3]));
	ASSERT(insideCircumcircle(quad1[0], quad1[1], quad1[2], int2(5000, 5000)));

	int2 vectors[6] = {{2, 3}, {-2, 3}, {-3, 0}, {-4, -2}, {0, -2}, {3, -2}};
	ASSERT_EQ(selectCCWMaxAngle(vectors[0], CSpan<int2>(vectors + 1, 5)), 2);
	ASSERT_EQ(selectCWMaxAngle(vectors[0], CSpan<int2>(vectors + 1, 5)), 3);

	int2 points[4] = {{0, 0}, {10, 0}, {10, 10}, {0, 10}};
	ASSERT_EQ(polygonArea(points), 100);
}

static void testGraph() NOINLINE;
static void testGraph() {
	GeomGraph<float2> graph;
	float2 points[] = {float2(-1, 0), float2(0, 0),   float2(1, 0),
					   float2(0, -1), float2(-1, -1), float2(0, 1)};
	int indices[][2] = {{1, 0}, {3, 1}, {2, 1}, {0, 4}, {1, 5}};

	for(auto pt : points)
		graph.fixVertex(pt);

	for(int n : intRange(indices)) {
		auto eid = graph.fixEdge(points[indices[n][0]], points[indices[n][1]]).id;
		auto eref = graph.ref(eid);
		graph[eref.from()].ival1 += 1;
	}

	ASSERT_EQ(graph[VertexId(1)].ival1, 2);
	// TODO: better tests?
}

vector<int2> slowSquareBorders(IRect rect, int2 pos, int radius) {
	vector<int2> out;
	for(auto pt : cells(rect)) {
		int dist = max(std::abs(pt.x - pos.x), std::abs(pt.y - pos.y));
		if(dist == radius)
			out.emplace_back(pt);
	}
	makeSorted(out);
	return out;
}

void testSquareBorder() {
	int max_radius = 20;
	IRect rect(0, 0, max_radius, max_radius);

	for(auto pt : cells(rect)) {
		for(int radius = 1; radius < max_radius; radius++) {
			vector<int2> result;
			for(auto ps : SquareBorder(rect, pt, radius))
				result.emplace_back(ps);
			makeSortedUnique(result);
			auto slow_result = slowSquareBorders(rect, pt, radius);
			if(result != slow_result)
				ASSERT_FAILED("Invalid result; pt:% radius:%\ncorrest:%\ninvalid:%\n", pt, radius,
							  slow_result, result);
		}

		vector<int2> sum;
		for(int r : intRange(1, max_radius)) {
			for(auto ps : SquareBorder(rect, pt, r))
				sum.emplace_back(ps);
		}
		makeSortedUnique(sum);
		ASSERT(sum.size() == rect.width() * rect.height() - 1);
	}
}

void testMain() {
	testContour();
	//testImmutableGraph();
	testRegularGrid();
	//testPlaneGraph();
	orderByDirectionTest();
	testGraph();
	testDelaunayFuncs();
	testSquareBorder();
}
