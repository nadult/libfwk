// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/fwd_member.h"
#include "fwk/math_base.h"

namespace fwk {

using RandomSeed = unsigned long;

struct RandomEngine;
class Random {
  public:
	Random(RandomSeed = 123);
	FWK_COPYABLE_CLASS(Random);

	RandomSeed operator()();

	int uniform(int min, int max);
	int uniform(int up_to) { return uniform(0, up_to - 1); }
	long long uniform(long long min, long long max);
	long long uniform(long long up_to) { return uniform(0ll, up_to - 1ll); }

	float uniform(float min, float max);
	double uniform(double min, double max);

	float normal(float mean, float stddev);
	double normal(double mean, double stddev);

	template <class T, EnableIfVec<T>...> T sampleBox(const T &min, const T &max) {
		T out;
		for(int n = 0; n < T::vec_size; n++)
			out[n] = uniform(min[n], max[n]);
		return out;
	}
	template <class T, EnableIfVec<T>...> T sampleUnitHemisphere() {
		auto point = sampleUnitSphere<T>();
		while(point == T())
			point = sampleUnitSphere<T>();
		return normalize(point);
	}

	template <class T, EnableIfVec<T>...> T sampleUnitSphere() {
		using Scalar = typename T::Scalar;
		T one;
		for(int n = 0; n < T::vec_size; n++)
			one[n] = Scalar(1);
		auto out = sampleBox(-one, one);
		while(lengthSq(out) > Scalar(1))
			out = sampleBox(-one, one);
		return out;
	}

	Quat uniformRotation();
	Quat uniformRotation(float3 axis);

	template <class T> void permute(Span<T> span) {
		for(int i = span.size() - 1; i > 0; i--) {
			using fwk::swap;
			swap(span[i], span[uniform(0, i)]);
		}
	}
	template <class TSpan, class T = SpanBase<TSpan>> const T &choose(const TSpan &span_) {
		CSpan<T> span(span_);
		PASSERT(span);
		return span[uniform(0, span.size() - 1)];
	}

  private:
	FwdMember<RandomEngine, 2504, 8> m_engine;
};
}
