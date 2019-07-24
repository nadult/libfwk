// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom_base.h"

// TODO: explain what these functions do
namespace fwk {

inline double attenuate(double dist, double k0, double k1, double k2) {
	return 1.0f / (k0 + dist * k1 + dist * dist * k2);
}

inline float smoothLerp(float val, float smooth_in, float smooth_out) {
	auto val1 = powf(val, smooth_in);
	auto val2 = 1.0f - powf(1.0f - val, smooth_out);
	float p1 = smooth_in < 1.0f ? 1.0f / smooth_in : smooth_in;
	float p2 = smooth_out < 1.0f ? 1.0f / smooth_out : smooth_out;

	return lerp(val1, val2, val);
}

double attenuate(double val);

vector<float2> smoothCurve(vector<float2> points, int target_count);
vector<float2> circularCurve(float scale, float step);

template <class T, EnableIfVec<T, 2>...>
vector<T> randomPoints(Random &random, Box<T> rect, double min_dist);

vector<float3> generateRandomLine(u32 seed, FRect rect);
vector<Triangle3F> generateRandomPatch(vector<float3>, u32 seed, float density, float enlargement);
vector<Segment3F> generatePatchBorder(vector<Triangle3F>);
vector<Segment3F> generateVoronoiLines(vector<Segment3F>);
vector<Segment3F> generateVoronoiLinesFromPoints(vector<float3>);
vector<Segment2F> generateDelaunaySegments(vector<float3>);

vector<Triangle3F> attenuateTest(float enlargement, float density, float3 att_param,
								 float att_offset, float att_max);
vector<Segment2F> smoothLerpTest(float smooth_in, float smooth_out);
}