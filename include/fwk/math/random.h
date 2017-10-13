// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/flat_impl.h"
#include "fwk_math.h"

namespace fwk {

using RandomSeed = unsigned long;

class Random {
  public:
	Random(RandomSeed = 123);
	Random(const Random &);
	Random(Random &&);
	~Random();

	Random &operator=(const Random &);
	Random &operator=(Random &&);

	RandomSeed operator()();

	int uniform(int min, int max);
	int uniform(int up_to) { return uniform(0, up_to - 1); }
	long long uniform(long long min, long long max);
	long long uniform(long long up_to) { return uniform(0ll, up_to - 1ll); }

	float uniform(float min, float max);
	double uniform(double min, double max);

	float normal(float mean, float stddev);
	double normal(double mean, double stddev);

	template <class T, EnableIfVector<T>...> T sampleBox(const T &min, const T &max) {
		T out;
		for(int n = 0; n < T::vector_size; n++)
			out[n] = uniform(min[n], max[n]);
		return out;
	}
	template <class T, EnableIfVector<T>...> T sampleUnitHemisphere() {
		auto point = sampleUnitSphere<T>();
		while(isZero(point))
			point = sampleUnitSphere<T>();
		return normalize(point);
	}

	template <class T, EnableIfVector<T>...> T sampleUnitSphere() {
		using Scalar = typename T::Scalar;
		T one;
		for(int n = 0; n < T::vector_size; n++)
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

  private:
	struct Engine;
	FlatImpl<Engine, 8> m_engine;
};
}
