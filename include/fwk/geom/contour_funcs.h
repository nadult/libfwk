// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

// TODO: better file name ?

#include "fwk/math_base.h"

namespace fwk {

template <class Vec, class Scalar = Base<Vec>, EnableIfVec<Vec>...>
vector<Scalar> summedSegmentLengths(CSpan<Vec> points, bool loop) {
	vector<Scalar> out;

	if(points) {
		int num_segs = points.size() - (loop ? 0 : 1);
		out.resize(num_segs);

		Promote<Scalar> sum(0);
		// TODO: stable adding algorithm?
		for(auto [i, j] : pairsRange(points.size())) {
			auto dist = distance(points[i], points[j]);
			PASSERT(dist > Scalar(0));
			sum += dist;
			out[i] = sum;
		}
		if(loop) {
			auto dist = distance(points.back(), points.front());
			PASSERT(dist > Scalar(0));
			out.back() = sum + dist;
		}
	}
	return out;
}

template <class T> int segmentIndex(CSpan<T> sum_lengths, T pos) {
	DASSERT(pos >= 0.0f && pos <= (sum_lengths ? sum_lengths.back() : 0.0f));
	auto it = std::lower_bound(begin(sum_lengths), end(sum_lengths), pos);
	int index = min(int(it - sum_lengths.begin()), sum_lengths.size() - 1);
	return index;
}
}
