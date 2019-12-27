/*
 uint128 comes from Google::Abseil: https://abseil.io/
 Code was slightly adapted to libfwk by Krzysztof Jakubowski.

 Copyright 2017 The Abseil Authors.

 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

#include "fwk/math/uint128.h"

#include <stddef.h>

namespace fwk {

namespace {

// Returns the 0-based position of the last set bit (i.e., most significant bit)
// in the given uint64_t. The argument may not be 0.
//
// For example:
//   Given: 5 (decimal) == 101 (binary)
//   Returns: 2
#define STEP(T, n, pos, sh)                                                                        \
	do {                                                                                           \
		if((n) >= (static_cast<T>(1) << (sh))) {                                                   \
			(n) = (n) >> (sh);                                                                     \
			(pos) |= (sh);                                                                         \
		}                                                                                          \
	} while(0)
	static inline int Fls64(uint64_t n) {
		PASSERT(n != 0);
		int pos = 0;
		STEP(uint64_t, n, pos, 0x20);
		uint32_t n32 = static_cast<uint32_t>(n);
		STEP(uint32_t, n32, pos, 0x10);
		STEP(uint32_t, n32, pos, 0x08);
		STEP(uint32_t, n32, pos, 0x04);
		return pos + ((uint64_t{0x3333333322221100} >> (n32 << 2)) & 0x3);
	}
#undef STEP

	// Like Fls64() above, but returns the 0-based position of the last set bit
	// (i.e., most significant bit) in the given uint128. The argument may not be 0.
	static inline int Fls128(uint128 n) {
		if(uint64_t hi = Uint128High64(n)) {
			return Fls64(hi) + 64;
		}
		return Fls64(Uint128Low64(n));
	}

	// Long division/modulo for uint128 implemented using the shift-subtract
	// division algorithm adapted from:
	// http://stackoverflow.com/questions/5386377/division-without-using
	void DivModImpl(uint128 dividend, uint128 divisor, uint128 *quotient_ret,
					uint128 *remainder_ret) {
		PASSERT(divisor != 0);

		if(divisor > dividend) {
			*quotient_ret = 0;
			*remainder_ret = dividend;
			return;
		}

		if(divisor == dividend) {
			*quotient_ret = 1;
			*remainder_ret = 0;
			return;
		}

		uint128 denominator = divisor;
		uint128 quotient = 0;

		// Left aligns the MSB of the denominator and the dividend.
		const int shift = Fls128(dividend) - Fls128(denominator);
		denominator <<= shift;

		// Uses shift-subtract algorithm to divide dividend by denominator. The
		// remainder will be left in dividend.
		for(int i = 0; i <= shift; ++i) {
			quotient <<= 1;
			if(dividend >= denominator) {
				dividend -= denominator;
				quotient |= 1;
			}
			denominator >>= 1;
		}

		*quotient_ret = quotient;
		*remainder_ret = dividend;
	}

	template <typename T> uint128 Initialize128FromFloat(T v) {
		// Rounding behavior is towards zero, same as for built-in types.

		// Undefined behavior if v is NaN or cannot fit into uint128.
		PASSERT(!std::isnan(v) && v > -1 && v < std::ldexp(static_cast<T>(1), 128));

		if(v >= std::ldexp(static_cast<T>(1), 64)) {
			uint64_t hi = static_cast<uint64_t>(std::ldexp(v, -64));
			uint64_t lo = static_cast<uint64_t>(v - std::ldexp(static_cast<T>(hi), 64));
			return uint128(hi, lo);
		}

		return uint128(0, static_cast<uint64_t>(v));
	}
} // namespace

uint128::uint128(float v) : uint128(Initialize128FromFloat(v)) {}
uint128::uint128(double v) : uint128(Initialize128FromFloat(v)) {}
uint128::uint128(long double v) : uint128(Initialize128FromFloat(v)) {}

uint128 &uint128::operator/=(const uint128 &divisor) {
	uint128 quotient = 0;
	uint128 remainder = 0;
	DivModImpl(*this, divisor, &quotient, &remainder);
	*this = quotient;
	return *this;
}
uint128 &uint128::operator%=(const uint128 &divisor) {
	uint128 quotient = 0;
	uint128 remainder = 0;
	DivModImpl(*this, divisor, &quotient, &remainder);
	*this = remainder;
	return *this;
}
}
