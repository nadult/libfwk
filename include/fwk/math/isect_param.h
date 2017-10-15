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
	IsectParam() : m_interval(constant<T>::inf(), -constant<T>::inf()) {}

	bool isPoint() const { return m_interval.min == m_interval.max; }
	bool isInterval() const { return m_interval.max > m_interval.min; }
	bool isEmpty() const { return m_interval.empty(); }
	explicit operator bool() const { return !isEmpty(); }

	const Interval<T> &asInterval() const { return m_interval; }
	T asPoint() const { return m_interval.min; }
	T closest() const { return m_interval.min; }
	T farthest() const { return m_interval.max; }

  private:
	Interval<T> m_interval;
};

}
