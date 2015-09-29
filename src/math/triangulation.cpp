/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/
#include "fwk_math.h"
#include "fwk_xml.h"
#include <map>

#define TRILIBRARY
#define ANSI_DECLARATORS
#define VOID void

extern "C" {
#include "triangle/triangle.c"
}

namespace fwk {

static void free(triangulateio &data) {
#define FREE(attrib)                                                                               \
	if(data.attrib) {                                                                              \
		trifree(data.attrib);                                                                      \
		data.attrib = 0;                                                                           \
	}
	FREE(pointlist);
	FREE(pointattributelist);
	FREE(pointmarkerlist);
	FREE(trianglelist);
	FREE(triangleattributelist);
	FREE(trianglearealist);
	FREE(neighborlist);
	FREE(segmentlist);
	FREE(segmentmarkerlist);
	DASSERT(!data.holelist);
	DASSERT(!data.regionlist);
	FREE(edgelist);
	FREE(edgemarkerlist);
	FREE(normlist);
#undef FREE
}

void saveSvg(vector<float2> points, vector<Segment2D> segs, vector<Triangle2D> tris, int id,
			 float scale);

vector<Triangle2D> triangulate(const vector<Segment2D> &segs, vector<int> boundary_markers) {
	DASSERT(boundary_markers.size() == segs.size());

	triangulateio input, output;
	memset(&input, 0, sizeof(input));
	memset(&output, 0, sizeof(output));

	vector<pair<double, double>> points;
	vector<int> seg_inds;

	static int id = 0;
	printf("Generating #%d:\n", ++id);

	for(int s = 0; s < (int)segs.size(); s++) {
		const auto &seg = segs[s];
		int index[2] = {-1, -1};
		for(int n = 0; n < (int)points.size(); n++) {
			float2 pt(points[n].first, points[n].second);

			if(distance(pt, seg.start) < constant::epsilon)
				index[0] = n;
			if(distance(pt, seg.end) < constant::epsilon)
				index[1] = n;
		}

		if(index[0] == -1) {
			index[0] = (int)points.size();
			points.emplace_back(double(seg.start.x), double(seg.start.y));
		}
		if(index[1] == -1) {
			index[1] = (int)points.size();
			points.emplace_back(double(seg.end.x), double(seg.end.y));
		}

		xmlPrint("Seg %: (%) (%) width: %\n", s, seg.start, seg.end, distance(seg.start, seg.end));

		insertBack(seg_inds, index);
	}
	xmlPrint("Points: %\n", (int)points.size());
	//	saveSvg(points, segs, {}, id, 1000.0f);

	input.pointlist = &points.front().first;
	input.numberofpoints = (int)points.size();
	input.segmentlist = seg_inds.data();
	input.numberofsegments = (int)seg_inds.size();
	input.segmentmarkerlist = boundary_markers.data();

	char flags[128];
	snprintf(flags, sizeof(flags), "pz");

	::triangulate(flags, &input, &output, 0);

	try {
		vector<Triangle2D> out;
		out.resize(output.numberoftriangles);

		for(int n = 0; n < output.numberoftriangles; n++) {
			float2 points[3];
			auto *triangle = output.trianglelist + n * 3;
			for(int i = 0; i < 3; i++)
				points[i] = float2(output.pointlist[triangle[i] * 2 + 0],
								   output.pointlist[triangle[i] * 2 + 1]);
			out[n] = Triangle2D(points[0], points[1], points[2]);
		}

		free(output);
		return out;
	} catch(...) {
		free(output);
		throw;
	}
}
}
