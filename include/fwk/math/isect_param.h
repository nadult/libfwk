// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math/interval.h"

namespace fwk {

template <class T> class IsectParam {
  public:
	IsectParam(T point) : m_interval(point) {}
	IsectParam(T min, T max) : m_interval(min, max) {}
	IsectParam(Interval<T> interval) : m_interval(interval) {}
	IsectParam() : m_interval(inf, -T(inf)) {}

	bool isPoint() const { return m_interval.min == m_interval.max; }
	bool isInterval() const { return m_interval.max > m_interval.min; }
	bool valid() const { return m_interval.valid(); }
	explicit operator bool() const { return valid(); }

	const Interval<T> &asInterval() const { return m_interval; }
	T asPoint() const { return m_interval.min; }
	T closest() const { return m_interval.min; }
	T farthest() const { return m_interval.max; }

	void operator>>(TextFormatter &fmt) const { m_interval >> fmt; }
	bool operator==(const IsectParam &rhs) const { return m_interval == rhs.m_interval; }
	bool operator<(const IsectParam &rhs) const { return m_interval < rhs.m_interval; }

  private:
	Interval<T> m_interval;
};
}
