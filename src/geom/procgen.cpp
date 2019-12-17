// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom/procgen.h"

#include "fwk/geom/contour.h"
#include "fwk/geom/delaunay.h"
#include "fwk/geom/segment_grid.h"
#include "fwk/geom/voronoi.h"
#include "fwk/math/interpolation.h"
#include "fwk/math/random.h"
#include "fwk/math/triangle.h"
#include "fwk/sys/expected.h"

namespace fwk {

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
	vector<T> out;

	using Scalar = Base<T>;
	RegularGrid<T> ugrid(rect, Scalar(min_dist / std::sqrt(2.0)), 1);
	auto min_dist_sq = min_dist * min_dist;

	T invalid_pt(inf);
	vector<T> points(ugrid.width() * ugrid.height(), invalid_pt);

	for(auto pos : cells(ugrid.cellRect().inset(1))) {
		auto pt = ugrid.toWorld(pos) + random.sampleBox(T(), T(1, 1)) * ugrid.cellSize();

		int idx = pos.x + pos.y * ugrid.width();
		int indices[4] = {idx - 1, idx - ugrid.width(), idx - ugrid.width() - 1,
						  idx - ugrid.width() + 1};

		if(allOf(indices, [&](int idx) { return distanceSq(points[idx], pt) >= min_dist_sq; })) {
			points[idx] = pt;
			out.emplace_back(pt);
		}
	}

	return out;
}

template vector<float2> randomPoints(Random &random, Box<float2> rect, double min_dist);

vector<float2> circularCurve(float scale, float step) {
	vector<float2> init_points{
		{float2(scale, 0), float2(0, scale), float2(-scale, 0), float2(0, -scale)}};

	Contour<float2> contour(init_points, true);
	return contour.smooth(step).points();
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
vector<double> generateNoise(double beta, int num_points, int seed) {
	DASSERT(beta >= 0.0);
	DASSERT(num_points >= 1);
	int count = nextPow2(num_points);

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
		if(isNan(real[i])) // TODO: this shouldn't happen
			real[i] = 0.0;
		auto mid_dist = fwk::abs(double(i) - double(count / 2)) / double(count / 2);
		real[i] *= 1.0 - mid_dist * mid_dist;
	}

	DASSERT(!isNan(real));
	real.resize(num_points);
	return real;
}

/*
Ex<vector<Triangle3F>> generateRandomPatch(vector<float3> generators, u32 seed, float density,
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
	auto tris = delaunayTriangles<float3>(EX_PASS(delaunay<float2>(rpoints)), rpoints3);

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
}*/

/*
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
	auto voronoi = Voronoi::construct(igraph);

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
	auto voronoi = Voronoi::construct(igraph);
	auto del = delaunay(voronoi);

	return delaunaySegments<float2>(del, graph);
}*/
}
