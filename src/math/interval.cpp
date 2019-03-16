// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math/interval.h"

#include "fwk/format.h"
#include "fwk/math/rational.h"

namespace fwk {

template <class T> void Interval<T>::operator>>(TextFormatter &out) const {
	out(out.isStructured() ? "(%; %)" : "% %", min, max);
}

#define INSTANTIATE(type) template void Interval<type>::operator>>(TextFormatter &) const;

INSTANTIATE(short)
INSTANTIATE(int)
INSTANTIATE(float)
INSTANTIATE(double)

INSTANTIATE(Rational<int>)
INSTANTIATE(Rational<i64>)
INSTANTIATE(Rational<qint>)

#undef INSTANTIATE
}
