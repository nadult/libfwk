// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/format.h"
#include "fwk/math/ext24.h"
#include "fwk/math/gcd.h"
#include "fwk/math/rational.h"

using ld = long double;

namespace fwk {
namespace {

	// TODO: why is it so slow, when moved to libfwk?

	// TODO: it makes no sense to test for 0 in signs...
	// Returns sign of a + d * sqrt(6)
	int sign1(int a, int d) {
		int sign_a = a < 0 ? -1 : 1;
		int sign_d = d > 0 ? -1 : 1;

		auto left = i64(a) * i64(a) * sign_a;
		auto right = i64(d) * i64(d) * 6 * sign_d;
		return left == right ? 0 : left < right ? -1 : 1;
		//return ld(a) < -ld(d) * sqrtl(6) ? -1 : 1;
	}

	// Returns sign of -(b * sqrt(2) + c * sqrt(3))
	int sign2(int b, int c, bool do_print = false) {
		int sign_a = b < 0 ? -1 : 1;
		int sign_c = c > 0 ? -1 : 1;

		auto left = i64(b) * i64(b) * 2 * sign_a;
		auto right = i64(c) * i64(c) * 3 * sign_c;
		return left == right ? 0 : left < right ? 1 : -1;
		//return ld(b) * sqrtl(2) < -ld(c) * sqrtl(3)? -1 : 1;
	}

	// Returns sign of S1 * (a^2 + 6d^2)  - S2 * (2 * b^2 + 3 * c^2)
	int sign3(int a, int b, int c, int d, int s1, int s2) {
		auto left = s1 * (i64(a) * a + 6 * i64(d) * d);
		auto right = s2 * (2 * i64(b) * b + 3 * i64(c) * c);
		return left == right ? 0 : left < right ? -1 : 1;
	}

	int sign4(int a, int b, int c, int d, int s1, int s2) {
		auto left = s2 * i64(b) * c;
		auto right = s1 * i64(a) * d;
		return left == right ? 0 : left < right ? -1 : 1;
	}

	// Sign of: a + b * sqrt(2) + c * sqrt(3) + d * sqrt(6)
	template <class T> int quadSignSlow(T a, T b, T c, T d) NOINLINE;
	template <class T> int quadSignSlow(T a, T b, T c, T d) {
		T s1 = sign1(a, d);
		T s2 = sign2(b, c);
		T s3 = sign3(a, b, c, d, s1, s2);
		T s4 = sign4(a, b, c, d, s1, s2);

		using PT = Promote<T>;
		using PPT = Promote<T, 2>;

		PT a2 = PT(a) * a;
		PT b2 = PT(b) * b;
		PT c2 = PT(c) * c;
		PT d2 = PT(d) * d;

		PPT a4 = PPT(a2) * a2;
		PPT b4 = PPT(b2) * b2;
		PPT c4 = PPT(c2) * c2;
		PPT d4 = PPT(d2) * d2;

		PPT left = s3 * (a4 + PPT(d4) * 36 + PPT(a2) * 12 * d2 + b4 * 4 + c4 * 9 +
						 PPT(b2) * c2 * 12 - 2 * s1 * s2 * PPT(a2 + d2 * 6) * (2 * b2 + 3 * c2));
		PPT right = 24 * s4 * (PPT(b2) * c2 + PPT(a2) * d2 - 2 * s1 * s2 * PPT(a) * b * c * d);

		/*if(do_print) {
		double left0 = a + sqrt(6) * d;
		double right0 = -(b * sqrt(2) + c * sqrt(3));

		double left1 = s1 * (a2 + 2 * double(a) * d * sqrt(6) + 6 * double(d2));
		double right1 = s2 * (double(b2) * 2 + 3 * c2 + 2 * double(b) * c * sqrt(6));

		double left2 = s1 * (a2 + 6 * d2) - s2 * (2 * b2 + 3 * c2);
		double right2 = 2 * sqrt(6) * (s2 * double(b) * c - s1 * double(a) * d);

		int result0 = left0 < right0 ? -1 : 1;
		int result1 = left1 < right1 ? -1 : 1;
		int result2 = left2 < right2 ? -1 : 1;

		FWK_PRINT(left0, right0, result0);
		FWK_PRINT(left1, right1, result1);
		FWK_PRINT(left2, right2, result2);

		FWK_PRINT(left, right);
		FWK_PRINT(a, a2, a4);
		FWK_PRINT(b, b2, b4);
		FWK_PRINT(c, c2, c4);
		FWK_PRINT(d, d2, d4);
		FWK_PRINT(s1, s2, s3, s4);
	}*/

		return left == right ? 0 : left < right ? -1 : 1;
	}
}

template <class T> Ext24<T> Ext24<T>::operator*(const Ext24 &rhs) const {
	T x1 = a, x2 = b, x3 = c, x4 = d;
	T y1 = rhs.a, y2 = rhs.b, y3 = rhs.c, y4 = rhs.d;

	// TODO: better filtering
	if((b | rhs.b) == 0 && (d | rhs.d) == 0) {
		T new_a = x1 * y1 + x3 * y3 * 3;
		T new_c = x1 * y3 + x3 * y1;
		return Ext24(new_a, 0, new_c, 0);
	} else {
		// clang-format off
		T new_a = x1 * y1 + x2 * y2 * 2 + x3 * y3 * 3 + x4 * y4 * 6;
		T new_b = x1 * y2 + x2 * y1     + x3 * y4 * 3 + x4 * y3 * 3;
		T new_c = x1 * y3 + x2 * y4 * 2 + x3 * y1     + x4 * y2 * 2;
		T new_d = x1 * y4 + x2 * y3     + x3 * y2     + x4 * y1;
		// clang-format on
		return Ext24(new_a, new_b, new_c, new_d);
	}
}

template <class T> auto Ext24<T>::intDenomInverse() const -> RatExt24<PPT> {
	PT z = PT(a) * a - 2 * PT(b) * b - 3 * PT(c) * c + 6 * PT(d) * d;
	PT w = PT(a) * d - PT(b) * c;

	PPT new_a = PPT(a) * z - 12 * PPT(d) * w;
	PPT new_b = 6 * PPT(c) * w - PPT(b) * z;
	PPT new_c = 4 * PPT(b) * w - PPT(c) * z;
	PPT new_d = PPT(d) * z - 2 * PPT(a) * w;

	PPT den = PPT(z) * z - PPT(w) * w * 24;
	if(den < 0) {
		new_a = -new_a;
		new_b = -new_b;
		new_c = -new_c;
		new_d = -new_d;
		den = -den;
	}

	return {{new_a, new_b, new_c, new_d}, den};
}

template <class T> Ext24<T>::operator double() const {
	return double(a) + double(b) * sqrt2 + double(c) * sqrt3 + double(d) * sqrt6;
}

template <class T> bool Ext24<T>::operator==(const Ext24 &rhs) const {
	return a == rhs.a && b == rhs.b && c == rhs.c && d == rhs.d;
}

template <class T> int Ext24<T>::sign() const {
	if(*this == Ext24())
		return 0;
	// TODO: first approx with doubles? but how can we be sure of result ?
	// TODO: use wide int for bigger input types

	// TODO: what to do when we have llints ? use wide int?

	if constexpr(sizeof(T) <= sizeof(int)) {
		// TODO: verify this...
		auto appr = sqrt2 * double(b) + sqrt3 * double(c) + sqrt6 * double(d) + a;
		return appr < 0 ? -1 : 1;
	}

	// Checking with rational approximations:
	using PT = Promote<T, is_same<T, short> ? 2 : 1>;
	PT bnum = 888515016;
	PT cnum = 1088204209;
	PT dnum = 1538953151;
	PT denom = 628274993;

	PT bsign = b < 0, csign = c < 0, dsign = d < 0;
	PT min =
		PT(a) * denom + PT(b) * (bnum + bsign) + PT(c) * (cnum + csign) + PT(d) * (dnum + dsign);
	if(min > 0)
		return 1;
	PT max = PT(a) * denom + PT(b) * (bnum + 1 - bsign) + PT(c) * (cnum + 1 - csign) +
			 PT(d) * (dnum + 1 - dsign);
	if(max < 0)
		return -1;

	if(b == 0 && c == 0 && d == 0)
		return a < 0 ? -1 : 1;

	return quadSignSlow(a, b, c, d);
}

template <class T> void Ext24<T>::operator>>(TextFormatter &fmt) const {
	if(fmt.isStructured()) {
		if(*this == 0) {
			fmt << "0";
			return;
		}

		vector<string> elements;

		if(a != 0)
			elements.emplace_back(toString(a));
		if(b != 0)
			elements.emplace_back(format("%*\u221A2", b));
		if(c != 0)
			elements.emplace_back(format("%*\u221A3", c));
		if(d != 0)
			elements.emplace_back(format("%*\u221A6", d));
		if(elements.size() == 0)
			fmt << "0";
		else if(elements.size() == 1)
			fmt << elements[0];
		else {
			fmt << "(" << elements[0];

			for(int n = 1; n < elements.size(); n++) {
				if(elements[n][0] != '-')
					fmt << " + " << elements[n];
				else
					fmt << " - " << elements[n].substr(1);
			}

			fmt << ")";
		}
	} else {
		fmt("% % % %", a, b, c, d);
	}
}

template struct Ext24<short>;
template struct Ext24<int>;
template struct Ext24<llint>;
template struct Ext24<qint>;
}
