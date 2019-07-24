// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom/procgen.h"

#include "fwk/geom/contour.h"
#include "fwk/geom/delaunay.h"
#include "fwk/geom/plane_graph.h"
#include "fwk/geom/plane_graph_builder.h"
#include "fwk/geom/segment_grid.h"
#include "fwk/geom/voronoi.h"
#include "fwk/math/random.h"
#include "fwk/math/triangle.h"
#include "fwk/sys/expected.h"

namespace fwk {

double attenuate(double val) { return attenuate(val, 0.7, 0.0, 0.1); }

vector<float2> smoothCurve(vector<float2> points, int target_count) {
	DASSERT(points.size() >= 2);
	auto vec0 = normalize(points[0] - points[1]);
	auto vecn = normalize(points.back() - points[points.size() - 2]);

	points.insert(points.begin(), points[0] - vec0);
	points.emplace_back(points.back() + vecn);

	int num_segments = points.size() - 3;
	vector<float2> out;
	out.reserve(target_count);

	for(int n = 0; n < target_count; n++) {
		double t = double(n * num_segments) / double(target_count - 1);

		int offset = min(points.size() - 4, int(t));
		t -= offset;

		out.emplace_back(interpCubic(points[offset], points[offset + 1], points[offset + 2],
									 points[offset + 3], t));
	}

	return out;
}

template <class T, EnableIfVec<T, 2>...>
vector<T> randomPoints(Random &random, Box<T> rect, double min_dist) {
	PGraph<T> pgraph;
	pgraph.addGrid();
	return pgraph.randomPoints(random, min_dist, rect);
}

template vector<float2> randomPoints(Random &, FRect, double);

vector<float2> circularCurve(float scale, float step) {
	vector<float2> init_points{
		{float2(scale, 0), float2(0, scale), float2(-scale, 0), float2(0, -scale)}};

	Contour<float2> contour(init_points, true);
	return contour.cubicInterpolate(step).points();
}

// Source: http://paulbourke.net/miscellaneous/dft/
// This computes an in-place complex-to-complex FFT
// x and y are the real and imaginary arrays of 2^m points.
// dir =  1 gives forward transform
// dir = -1 gives reverse transform
static void FFT(short int dir, Span<double> x, Span<double> y) {
	int i, i1, j, k, i2, l, l1, l2;
	double c1, c2, tx, ty, t1, t2, u1, u2, z;

	DASSERT_EQ(x.size(), y.size());
	DASSERT(isPowerOfTwo(x.size()));

	int n = x.size();
	int m = log2(x.size());

	// Do the bit reversal
	i2 = n >> 1;
	j = 0;
	for(i = 0; i < n - 1; i++) {
		if(i < j) {
			tx = x[i];
			ty = y[i];
			x[i] = x[j];
			y[i] = y[j];
			x[j] = tx;
			y[j] = ty;
		}
		k = i2;
		while(k <= j) {
			j -= k;
			k >>= 1;
		}
		j += k;
	}

	// Compute the FFT c1 = -1.0;
	c2 = 0.0;
	l2 = 1;
	for(l = 0; l < m; l++) {
		l1 = l2;
		l2 <<= 1;
		u1 = 1.0;
		u2 = 0.0;
		for(j = 0; j < l1; j++) {
			for(i = j; i < n; i += l2) {
				i1 = i + l1;
				t1 = u1 * x[i1] - u2 * y[i1];
				t2 = u1 * y[i1] + u2 * x[i1];
				x[i1] = x[i] - t1;
				y[i1] = y[i] - t2;
				x[i] += t1;
				y[i] += t2;
			}
			z = u1 * c1 - u2 * c2;
			u2 = u1 * c2 + u2 * c1;
			u1 = z;
		}
		c2 = sqrt((1.0 - c1) / 2.0);
		if(dir == 1)
			c2 = -c2;
		c1 = sqrt((1.0 + c1) / 2.0);
	}

	// Scaling for forward transform
	if(dir == 1) {
		for(i = 0; i < n; i++) {
			x[i] /= n;
			y[i] /= n;
		}
	}
}

// Source: http://paulbourke.net/fractals/noise/
static vector<double> randomLine(double beta, u32 seed, int count) {
	DASSERT(beta >= 1.0 && beta <= 3.0);

	DASSERT(isPowerOfTwo(count));

	double mag, pha;
	vector<double> real(count), imag(count);

	Random random(seed);

	real[0] = 0;
	imag[0] = 0;
	for(int i = 1; i <= count / 2; i++) {
		mag = std::pow(i + 1.0, -beta / 2) * random.normal(0.0, 1.0);
		pha = (pi * 2.0) * random.uniform(0.0, 1.0);
		real[i] = mag * cos(pha);
		imag[i] = mag * sin(pha);
		real[count - i] = real[i];
		imag[count - i] = -imag[i];
	}
	imag[count / 2] = 0;

	FFT(-1, real, imag);

	for(int i : intRange(real)) {
		if(isNan(real[i])) // TODO: this shoudlnt happen
			real[i] = 0.0;
		auto mid_dist = fwk::abs(double(i) - double(count / 2)) / double(count / 2);
		real[i] *= 1.0 - mid_dist * mid_dist;
	}

	DASSERT(!isNan(real));
	return real;
}

vector<float3> generateRandomLine(u32 seed, FRect rect) {
	Random random(seed);

	auto from = random.sampleBox(rect.min(), rect.max());
	auto to = random.sampleBox(rect.min(), rect.max());
	FRect box(vmin(from, to), vmax(from, to));

	auto mid1 = lerp(from, to, random.uniform(0.25f, 0.45f));
	auto mid2 = lerp(from, to, random.uniform(0.55f, 0.75f));

	auto bsize = box.size();
	mid1 += random.sampleBox(-bsize, bsize) * 0.25;
	mid2 += random.sampleBox(-bsize, bsize) * 0.25;

	float2 base[4] = {from, mid1, mid2, to};
	auto main_points = span(base);

	// TODO: za dużo szumu o wysokiej częstotliiwości
	auto line = randomLine(3.0, random(), 256);

	auto points = smoothCurve(main_points, line.size());

	vector<float2> tan;
	for(int n : intRange(0, points.size() - 1)) {
		auto vec = normalize(points[n + 1] - points[n]);
		tan.emplace_back(perpendicular(vec));
	}
	tan.emplace_back(tan.back());

	for(int n : intRange(points))
		points[n] += tan[n] * line[n] * 0.2;

	Contour<float2> contour(points, false);
	points = contour.points(); //contour.simplify(0.1, 0.5).simplify(0.5, 0.5).points();

	line = randomLine(3.0, random(), 256);
	vector<float3> out;

	for(int n : intRange(points)) {
		float height = (1.0f - fwk::abs(float(n - points.size() / 2) / points.size())) * 2.0f;
		out.emplace_back(asXZY(points[n], height + line[n] * 0.5f));
	}
	return out;
}

vector<Triangle3F> generateRandomPatch(vector<float3> generators, u32 seed, float density,
									   float enl) {
	Random random(seed);
	FRect rect = enclose(generators).xz().enlarge(enl + density * 2.0f);
	auto rpoints = randomPoints(random, rect, density);
	float max_dist_sq = enl * enl;
	auto test_func = [&](float2 p) {
		float dist_sq = inf;
		for(auto g : generators)
			dist_sq = min(dist_sq, distanceSq(g.xz(), p));
		return dist_sq <= max_dist_sq;
	};

	rpoints = filter(rpoints, test_func);

	auto rpoints3 = transform(rpoints, [](float2 p) { return asXZY(p, 0.0f); });
	auto tris = delaunayTriangles<float3>(delaunay<float2>(rpoints), rpoints3);

	tris = filter(tris, [&](const Triangle3F &tri) {
		for(auto edge : tri.edges()) {
			float2 center = (edge.from + edge.to).xz() * 0.5f;
			if(!test_func(center))
				return false;
		}
		return true;
	});
	return tris;
}

vector<Segment3F> generatePatchBorder(vector<Triangle3F> tris) {
	auto patch2 = transform(tris, [](const Triangle3F &tri) { return tri.xz(); });

	PlaneGraphBuilder<float2> builder;
	vector<int> ecounter;

	for(auto &tri : patch2)
		for(auto edge : tri.edges()) {
			if(edge.to < edge.from)
				edge = {edge.to, edge.from};
			auto eid = builder(edge.from, edge.to);
			ecounter.resize(builder.edges().capacity());
			ecounter[eid]++;
		}
	auto graph = builder.build();

	vector<Segment3F> out;
	for(auto eref : graph.edgeRefs())
		if(ecounter[eref] <= 1)
			out.emplace_back(asXZY(graph[eref.from()], 0.0f), asXZY(graph[eref.to()], 0.0f));
	return out;
}

vector<Segment3F> generateVoronoiLines(vector<Segment3F> patch_outline, vector<float3> points) {
	PlaneGraphBuilder<float2> builder;
	for(auto pt : points)
		builder(pt.xz());
	for(auto &edge : patch_outline)
		builder(edge.from.xz(), edge.to.xz());
	auto graph = builder.build();

	auto scale = bestIntegralScale(graph.points()), iscale = 1.0 / scale;
	auto igraph = toIntegral<int2>(graph, scale);

	igraph.addGrid();
	igraph.checkPlanar().check();
	auto voronoi = VoronoiDiagram::construct(igraph);

	vector<Segment3F> out;

	for(auto ref : voronoi.segmentGraph().edgeRefs()) {
		auto &edge = voronoi[ArcSegmentId(ref)];
		double3 p1(asXZY(voronoi[ref.from()], 0.0f)), p2(asXZY(voronoi[ref.to()], 0.0f));
		out.emplace_back(float3(p1 * iscale), float3(p2 * iscale));
	}
	return out;
}

vector<Segment3F> generateVoronoiLines(vector<Segment3F> segments) {
	return generateVoronoiLines(segments, {});
}

vector<Segment3F> generateVoronoiLinesFromPoints(vector<float3> points) {
	return generateVoronoiLines({}, points);
}

vector<Segment2F> generateDelaunaySegments(vector<float3> points) {
	PlaneGraphBuilder<float2> builder;
	for(auto pt : points)
		builder(pt.xz());
	auto graph = builder.build();

	auto scale = bestIntegralScale(graph.points()), iscale = 1.0 / scale;
	auto igraph = toIntegral<int2>(graph, scale);

	igraph.addGrid();
	igraph.checkPlanar().check();
	auto voronoi = VoronoiDiagram::construct(igraph);
	auto del = delaunay(voronoi);

	return delaunaySegments<float2>(del, graph);
}

vector<Triangle3F> attenuateTest(float ex, float density, float3 aparams, float aoff, float amax) {
	Random random(123);
	auto points = randomPoints(random, FRect(float2(-ex), float2(ex)), density);
	float3 params = aparams;
	auto attenuate = [=](float dist) {
		return max(1.0f / (params[0] + dist * params[1] + dist * dist * params[2]) - aoff,
				   float(amax));
	};
	vector<float3> points3 =
		transform(points, [&](float2 p) { return asXZY(p, attenuate(length(p))); });

	return delaunayTriangles<float3>(delaunay<float2>(points), points3);
}

vector<Segment2F> smoothLerpTest(float sm_in, float sm_out) {
	constexpr int num_points = 1024;
	vector<Segment2F> out;
	out.reserve(num_points);
	float2 prev;

	for(int n = 0; n < num_points; n++) {
		float pos = float(n) / float(num_points - 1);
		auto value = smoothLerp(pos, sm_in, sm_out);
		if(n > 0)
			out.emplace_back(prev, float2(pos, value));
		prev = {pos, value};
	}

	return out;
}
}