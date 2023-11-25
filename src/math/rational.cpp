// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math/rational.h"
#include "fwk/format.h"
#include "fwk/math/gcd.h"
#include "fwk/math/hash.h"

namespace fwk {

// Source: RTCD 11.5.2
template <class T> static int order(T lnum, T lden, T rnum, T rden) {
	// Handling infinities
	if(lden == 0 || rden == 0) {
		int lval = lden == 0 ? lnum < 0 ? -1 : 1 : 0;
		int rval = rden == 0 ? rnum < 0 ? -1 : 1 : 0;
		return lval < rval ? -1 : lval > rval ? 1 : 0;
	}

	using PInt = Promote<T>;
	if constexpr(!is_same<PInt, T>) {
		auto v1 = PInt(lnum) * rden;
		auto v2 = PInt(rnum) * lden;
		return v1 == v2 ? 0 : v1 < v2 ? -1 : 1;
	}

	auto a = lnum, b = lden;
	auto c = rnum, d = rden;

	if(a == c && b == d)
		return 0;

	if(c < 0) {
		b = -b;
		c = -c;
	}
	if(a < 0 && b < 0) {
		auto old_a = a, old_b = b;
		a = c;
		b = d;
		c = -old_a;
		d = -old_b;
	}
	if(a < 0)
		return -1;
	if(b < 0)
		return 1;

	if(a > b) {
		if(c < d)
			return 1;
		auto old_a = a, old_b = b;
		a = d;
		b = c;
		c = old_b;
		d = old_a;
	}

	if(c > d)
		return -1;

	// Continued fraction expansion
	while(a && c) {
		auto m = d / c;
		auto n = b / a;

		if(m != n) {
			if(m < n)
				return -1;
			if(m > n)
				return 1;
		}

		auto old_a = a;
		auto old_b = b;
		a = d % c;
		b = c;
		c = old_b % old_a;
		d = old_a;
	}

	if(a == 0)
		return c == 0 ? 0 : -1;
	return 1;
}

#define TEMPLATE template <class T, int N>
#define TRATIONAL Rational<T, N>

TEMPLATE TRATIONAL TRATIONAL::operator+(const Rational &rhs) const {
	if(m_den == rhs.m_den)
		return {Num(m_num + rhs.m_num), T(m_den), no_sign_check};
	return {Num(m_num * rhs.m_den + rhs.m_num * m_den), T(m_den * rhs.m_den), no_sign_check};
}

TEMPLATE TRATIONAL TRATIONAL::operator-(const Rational &rhs) const {
	if(m_den == rhs.m_den)
		return {Num(m_num - rhs.m_num), T(m_den), no_sign_check};
	return {Num(m_num * rhs.m_den - rhs.m_num * m_den), T(m_den * rhs.m_den), no_sign_check};
}

TEMPLATE TRATIONAL TRATIONAL::operator*(const Rational &rhs) const {
	return {Num(m_num * rhs.m_num), T(m_den * rhs.m_den), no_sign_check};
}

TEMPLATE
template <class U, EnableIf<fwk::dim<U> == 0>...> int TRATIONAL::order(const Rational &rhs) const {
	if constexpr(is_ext24<T>) {
		using PT = Promote<T>;
		// Handling infinities
		if(m_den == 0 || rhs.m_den == 0) {
			int lval = m_den == 0 ? m_num < 0 ? -1 : 1 : 0;
			int rval = rhs.m_den == 0 ? rhs.m_num < 0 ? -1 : 1 : 0;
			return lval < rval ? -1 : lval > rval ? 1 : 0;
		}

		auto left = PT(m_num) * rhs.m_den, right = PT(rhs.m_num) * m_den;
		return (left - right).sign();
	} else {
		return fwk::order(m_num, m_den, rhs.m_num, rhs.m_den);
	}
}

TEMPLATE bool TRATIONAL::operator==(const Rational &rhs) const {
	if constexpr(dim == 0)
		return order(rhs) == 0;
	else if constexpr(dim > 0) {
		// TODO: sometimes we want different kind of comparison: i.e. when one number
		//       is normalized and the other one is not then they are different
		for(int n = 0; n < dim; n++)
			if((*this)[n].order(rhs[n]) != 0)
				return false;
		return true;
	}
}

TEMPLATE void TRATIONAL::operator>>(TextFormatter &out) const {
	// TODO: option to disable special characters ?
	if(out.isStructured()) {
		if(*this == inf)
			out << "\u221E";
		else if(*this == -inf)
			out << "-\u221E";
		else if(den() == 1)
			out << num();
		else
			out("%/%", num(), den());
	} else {
		out("% %", num(), den());
	}
}

TEMPLATE bool TRATIONAL::operator<(const Rational &rhs) const {
	if constexpr(dim == 0) {
		return order(rhs) == -1;
	} else if constexpr(dim > 0) {
		// TODO: sometimes we want different kind of comparison: i.e. when one number
		//       is normalized and the other one is not then they are different
		for(int n = 0; n < dim; n++)
			if(int cmp = (*this)[n].order(rhs[n]))
				return cmp == -1;
		return false;
	}
}

TEMPLATE TRATIONAL TRATIONAL::normalized() const {
	// TODO: more normalization is possible for Ext24 (denominator can be turned into integer)
	// Should we create another normalization func?
	if constexpr(is_ext24<T>) {
		if(m_den.isIntegral()) {
			using S = Base<T>;
			if constexpr(dim == 0) {
				S t = gcd(cspan({m_num.a, m_num.b, m_num.c, m_num.d, m_den.a}));
				return {m_num / t, S(m_den.a / t), no_sign_check};
			} else if constexpr(dim == 2) {
				auto &x = m_num[0], &y = m_num[1];
				S t = gcd(cspan({x.a, x.b, x.c, x.d, y.a, y.b, y.c, y.d, m_den.a}));
				return {{x / t, y / t}, S(m_den.a / t), no_sign_check};
			}
		}

		return *this;
	} else {
		T t;
		if constexpr(dim == 0)
			t = gcd(m_num, m_den);
		else if constexpr(dim == 2)
			t = gcd(cspan({m_num[0], m_num[1], m_den}));
		else if constexpr(dim == 3)
			t = gcd(cspan({m_num[0], m_num[1], m_num[2], m_den}));

		return t == 1 ? *this : Rational(m_num / t, m_den / t, no_sign_check);
	}
}

TEMPLATE llint TRATIONAL::hash() const { return hashMany<llint>(m_num, m_den); }

Rational<int> rationalApprox(double value, int max_num, bool upper_bound) {
	bool sign = false;
	if(value < 0) {
		sign = true;
		value = -value;
	}

	Rational<int> best = int(value);
	double best_err = abs(double(best) - value);

	for(int n = 1; n < max_num; n++) {
		int avg_d = double(n) / value;
		for(int d = avg_d - 1; d <= avg_d + 1; d++) {
			Rational<int> rat(n, d);
			double drat = double(rat);

			if((!upper_bound && drat <= value) || (upper_bound && drat >= value)) {
				double err = abs(drat - value);
				if(err < best_err) {
					best = rat;
					best_err = err;
				}
			}
		}
	}

	return sign ? -best : best;
}

#define INST_VEC(type, size) template struct Rational<type, size>;
#define INST_SCALAR(type)                                                                          \
	template struct Rational<type>;                                                                \
	template int Rational<type>::order(const Rational &) const;

#define INST_ALL_SIZES(type)                                                                       \
	INST_SCALAR(type)                                                                              \
	INST_VEC(type, 2)                                                                              \
	INST_VEC(type, 3)

INST_ALL_SIZES(short)
INST_ALL_SIZES(int)
INST_ALL_SIZES(llint)
INST_ALL_SIZES(qint)

//INST_ALL_SIZES(Ext24<short>)
//INST_ALL_SIZES(Ext24<int>)
//INST_ALL_SIZES(Ext24<llint>)
//INST_ALL_SIZES(Ext24<qint>)

#undef TEMPLATE

}
