// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/format.h"
#include "fwk/math/ext24.h"
#include "fwk/math/gcd.h"
#include "fwk/math/hash.h"
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

template <class T> T Ext24<T>::gcd() const { return fwk::gcd(v); }
template <class T> llint Ext24<T>::hash() const { return hashMany<llint>(a, b, c, d); }

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

		auto fmt_element = [](T value, int sq) {
			if(value == 1)
				return format("\u221A%", sq);
			return format("%\u221A%", value, sq);
		};

		if(a != 0)
			elements.emplace_back(toString(a));
		if(b != 0)
			elements.emplace_back(fmt_element(b, 2));
		if(c != 0)
			elements.emplace_back(fmt_element(c, 3));
		if(d != 0)
			elements.emplace_back(fmt_element(d, 6));

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

struct Ext24Vec {
	Ext24<short> x, y;
	short divisor = 0;
};

struct Ext24Tan {
	Ext24<short> num;
	short den = 0;
};

static constexpr Ext24Tan vector_tans[5] = {
	{{2, 0, -1, 0}, 1},
	{{0, 0, 1, 0}, 3},
	{{1, 0, 0, 0}, 1},
	{{0, 0, 1, 0}, 1},
	{{2, 0, 1, 0}, 1},
};

static constexpr array<Ext24Vec, 24> vectors = []() {
	array<Ext24Vec, 24> out;

	// clang-format off
	out[0] = {{1,  0,  0,  0}, {0,  0,  0,  0}, 1}; //  0
	out[1] = {{0,  1,  0,  1}, {0, -1,  0,  1}, 4}; // 15
	out[2] = {{0,  0,  1,  0}, {1,  0,  0,  0}, 2}; // 30
	out[3] = {{0,  1,  0,  0}, {0,  1,  0,  0}, 2}; // 45
	out[4] = {{1,  0,  0,  0}, {0,  0,  1,  0}, 2}; // 60
	out[5] = {{0, -1,  0,  1}, {0,  1,  0,  1}, 4}; // 75
	out[6] = {{0,  0,  0,  0}, {1,  0,  0,  0}, 1}; // 90
	// clang-format on

	for(int n = 1; n <= 6; n++)
		out[n + 6] = {-out[6 - n].x, out[6 - n].y, out[6 - n].divisor};
	for(int n = 1; n < 12; n++)
		out[n + 12] = {out[12 - n].x, -out[12 - n].y, out[12 - n].divisor};

	return out;
}();

Rat2Ext24<short> angleToVectorExt24(int angle, int scale) {
	DASSERT(angle % 15 == 0);
	angle = angle % 360;
	if(angle < 0)
		angle += 360;

	auto &vec = vectors[angle / 15];
	int div = gcd(scale, vec.divisor);
	scale /= div;
	return {{vec.x * scale, vec.y * scale}, vec.divisor / div};
}

template <class T> Rat2Ext24<T> rotateVector(const Rat2Ext24<T> &vec, int degs) {
	Rat2Ext24<T> rot(angleToVectorExt24(degs));

	auto nx = rot.numX() * vec.numX() - rot.numY() * vec.numY();
	auto ny = rot.numX() * vec.numY() + rot.numY() * vec.numX();
	return {{nx, ny}, vec.den() * rot.den()};
}

template <class T> Maybe<int> vectorToAngle(const Rat2Ext24<T> &vec) {
	if(vec.numX() == 0) {
		if(vec.numY() == 0)
			return none;
		return vec.numY() < T(0)? 270 : 90;
	}
	if(vec.numY() == 0)
		return vec.numX() < T(0)? 180 : 0;
	
	int sign_x = vec.numX().sign();
	int sign_y = vec.numY().sign();
	auto div = abs(vec.numY() / vec.numX());

	for(int n : intRange(vector_tans)) {
		auto cur = RatExt24<T>(vector_tans[n].num, vector_tans[n].den);
		if(div == cur) {
			bool neg = (sign_x < 0) ^ (sign_y < 0);
			int quad = sign_x < 0? (sign_y < 0? 2 : 1) : (sign_y < 0? 3 : 0);
			int angle = quad * 6 + (neg? 5 - n : n + 1);
			return angle * 15;
		}

	}

	return none;
}

#define INSTANTIATE(type)                                                                          \
	template struct Ext24<type>;                                                                   \
	template Maybe<int> vectorToAngle(const Rat2Ext24<type> &);                                    \
	template Rat2Ext24<type> rotateVector(const Rat2Ext24<type> &, int);

INSTANTIATE(short)
INSTANTIATE(int)
INSTANTIATE(llint)
INSTANTIATE(qint)
}
