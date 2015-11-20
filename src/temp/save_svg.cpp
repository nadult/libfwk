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
		float area = t3.surfaceArea();

		auto poly = tnode.addChild("polygon");
		string points = format("%f,%f %f,%f %f,%f", p[0].x, p[0].y, p[1].x, p[1].y, p[2].x, p[2].y);
		poly.addAttrib("points", points);
		poly.addAttrib("style", "stroke-width:2.5;fill:red;stroke:blue;fill-opacity:0.4");
	}

	Saver(format("temp/file%d.svg", id)) << doc;
}

}
