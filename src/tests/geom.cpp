// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom/contour.h"
#include "testing.h"

static void testContour() {
	vector<double2> points = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
	Contour<double2> contour(points, true);

	//print("Normals:\n");
	//for(auto ei : indexRange<EdgeId>(contour.numEdges()))
	//	print("%\n", contour.normal(ei));
	ASSERT_GE(contour.edgeSide(EdgeId(0), double2(0.5, 0.5)), 0.0);
}

void testMain() { testContour(); }
