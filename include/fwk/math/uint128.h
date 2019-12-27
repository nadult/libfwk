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

#pragma once

#include "fwk/math/int128.h"
#include <cmath>
#include <cstdint>
#include <limits>

namespace fwk {

class uint128 {
  public:
	uint128() = default;
	uint128(uint64_t hi, uint64_t lo) : lo_(lo), hi_(hi) {}

	uint128(int v) : lo_(v), hi_(v < 0 ? std::numeric_limits<uint64_t>::max() : 0) {}
	uint128(long v) : lo_(v), hi_(v < 0 ? std::numeric_limits<uint64_t>::max() : 0) {}
	uint128(long long v) : lo_(v), hi_(v < 0 ? std::numeric_limits<uint64_t>::max() : 0) {}

	uint128(unsigned int v) : lo_(v), hi_(0) {}
	uint128(unsigned long v) : lo_(v), hi_(0) {}
	uint128(unsigned long long v) : lo_(v), hi_(0) {}
	explicit uint128(int128 v) : lo_(v.getLowBits()), hi_(u64(v.getHighBits())) {}

	explicit uint128(float v);
	explicit uint128(double v);
	explicit uint128(long double v);

	// Conversion operators to other arithmetic types
	explicit operator bool() const { return lo_ || hi_; }
	explicit operator i16() const { return i16(lo_); }
	explicit operator u16() const { return u16(lo_); }
	explicit operator i32() const { return i32(lo_); }
	explicit operator u32() const { return u32(lo_); }
	explicit operator i64() const { return i64(lo_); }
	explicit operator u64() const { return u64(lo_); }

	explicit operator int128() const { return {int64_t(hi_), lo_}; }
	explicit operator float() const { return float(lo_) + std::ldexp(float(hi_), 64); }
	explicit operator double() const { return double(lo_) + std::ldexp(double(hi_), 64); }
	explicit operator long double() const {
		return (long double)(lo_) + std::ldexp((long double)(hi_), 64);
	}

	// Arithmetic operators.
	uint128 &operator+=(const uint128 &other);
	uint128 &operator-=(const uint128 &other);
	uint128 &operator*=(const uint128 &other);

	// Long division/modulo for uint128.
	uint128 &operator/=(const uint128 &other);
	uint128 &operator%=(const uint128 &other);
	uint128 operator++(int);
	uint128 operator--(int);
	uint128 &operator<<=(int);
	uint128 &operator>>=(int);
	uint128 &operator&=(const uint128 &other);
	uint128 &operator|=(const uint128 &other);
	uint128 &operator^=(const uint128 &other);
	uint128 &operator++();
	uint128 &operator--();

	// Returns the lower/higher 64-bit value of a `uint128` value.
	friend uint64_t Uint128Low64(const uint128 &v) { return v.lo_; }
	friend uint64_t Uint128High64(const uint128 &v) { return v.hi_; }

	bool operator==(const uint128 &rhs) const { return (lo_ == rhs.lo_ && hi_ == rhs.hi_); }
	bool operator!=(const uint128 &rhs) const { return (lo_ != rhs.lo_ || hi_ != rhs.hi_); }

	bool operator<(const uint128 &rhs) const {
		return hi_ == rhs.hi_ ? lo_ < rhs.lo_ : hi_ < rhs.hi_;
	}
	bool operator>(const uint128 &rhs) const {
		return hi_ == rhs.hi_ ? lo_ > rhs.lo_ : hi_ > rhs.hi_;
	}
	bool operator<=(const uint128 &rhs) const {
		return hi_ == rhs.hi_ ? lo_ <= rhs.lo_ : hi_ <= rhs.hi_;
	}
	bool operator>=(const uint128 &rhs) const {
		return hi_ == rhs.hi_ ? lo_ >= rhs.lo_ : hi_ >= rhs.hi_;
	}

	uint128 operator<<(int amount) const { return uint128(*this) <<= amount; }
	uint128 operator>>(int amount) const { return uint128(*this) >>= amount; }
	uint128 operator+(const uint128 &rhs) const { return uint128(*this) += rhs; }
	uint128 operator-(const uint128 &rhs) const { return uint128(*this) -= rhs; }
	uint128 operator*(const uint128 &rhs) const { return uint128(*this) *= rhs; }
	uint128 operator/(const uint128 &rhs) const { return uint128(*this) /= rhs; }
	uint128 operator%(const uint128 &rhs) const { return uint128(*this) %= rhs; }

	u32 hash() const { return u32(hi_ >> 32) ^ u32(hi_) ^ u32(lo_ >> 32) ^ u32(lo_); }

  private:
	uint64_t lo_;
	uint64_t hi_;
};

// Shift and arithmetic operators.
// Unary operators.

inline uint128 operator-(const uint128 &val) {
	const uint64_t hi_flip = ~Uint128High64(val);
	const uint64_t lo_flip = ~Uint128Low64(val);
	const uint64_t lo_add = lo_flip + 1;
	if(lo_add < lo_flip) {
		return uint128(hi_flip + 1, lo_add);
	}
	return uint128(hi_flip, lo_add);
}

inline bool operator!(const uint128 &val) { return !Uint128High64(val) && !Uint128Low64(val); }

// Logical operators.

inline uint128 operator~(const uint128 &val) {
	return uint128(~Uint128High64(val), ~Uint128Low64(val));
}

inline uint128 operator|(const uint128 &lhs, const uint128 &rhs) {
	return uint128(Uint128High64(lhs) | Uint128High64(rhs), Uint128Low64(lhs) | Uint128Low64(rhs));
}

inline uint128 operator&(const uint128 &lhs, const uint128 &rhs) {
	return uint128(Uint128High64(lhs) & Uint128High64(rhs), Uint128Low64(lhs) & Uint128Low64(rhs));
}

inline uint128 operator^(const uint128 &lhs, const uint128 &rhs) {
	return uint128(Uint128High64(lhs) ^ Uint128High64(rhs), Uint128Low64(lhs) ^ Uint128Low64(rhs));
}

inline uint128 &uint128::operator|=(const uint128 &other) {
	hi_ |= other.hi_;
	lo_ |= other.lo_;
	return *this;
}

inline uint128 &uint128::operator&=(const uint128 &other) {
	hi_ &= other.hi_;
	lo_ &= other.lo_;
	return *this;
}

inline uint128 &uint128::operator^=(const uint128 &other) {
	hi_ ^= other.hi_;
	lo_ ^= other.lo_;
	return *this;
}

// Shift and arithmetic assign operators.

inline uint128 &uint128::operator<<=(int amount) {
	// Shifts of >= 128 are undefined.
	PASSERT(amount < 128);

	// uint64_t shifts of >= 64 are undefined, so we will need some
	// special-casing.
	if(amount < 64) {
		if(amount != 0) {
			hi_ = (hi_ << amount) | (lo_ >> (64 - amount));
			lo_ = lo_ << amount;
		}
	} else {
		hi_ = lo_ << (amount - 64);
		lo_ = 0;
	}
	return *this;
}

inline uint128 &uint128::operator>>=(int amount) {
	// Shifts of >= 128 are undefined.
	PASSERT(amount < 128);

	// uint64_t shifts of >= 64 are undefined, so we will need some
	// special-casing.
	if(amount < 64) {
		if(amount != 0) {
			lo_ = (lo_ >> amount) | (hi_ << (64 - amount));
			hi_ = hi_ >> amount;
		}
	} else {
		lo_ = hi_ >> (amount - 64);
		hi_ = 0;
	}
	return *this;
}

inline uint128 &uint128::operator+=(const uint128 &other) {
	hi_ += other.hi_;
	uint64_t lolo = lo_ + other.lo_;
	if(lolo < lo_)
		++hi_;
	lo_ = lolo;
	return *this;
}

inline uint128 &uint128::operator-=(const uint128 &other) {
	hi_ -= other.hi_;
	if(other.lo_ > lo_)
		--hi_;
	lo_ -= other.lo_;
	return *this;
}

inline uint128 &uint128::operator*=(const uint128 &other) {
	uint64_t a96 = hi_ >> 32;
	uint64_t a64 = hi_ & 0xffffffff;
	uint64_t a32 = lo_ >> 32;
	uint64_t a00 = lo_ & 0xffffffff;
	uint64_t b96 = other.hi_ >> 32;
	uint64_t b64 = other.hi_ & 0xffffffff;
	uint64_t b32 = other.lo_ >> 32;
	uint64_t b00 = other.lo_ & 0xffffffff;
	// multiply [a96 .. a00] x [b96 .. b00]
	// terms higher than c96 disappear off the high side
	// terms c96 and c64 are safe to ignore carry bit
	uint64_t c96 = a96 * b00 + a64 * b32 + a32 * b64 + a00 * b96;
	uint64_t c64 = a64 * b00 + a32 * b32 + a00 * b64;
	this->hi_ = (c96 << 32) + c64;
	this->lo_ = 0;
	// add terms after this one at a time to capture carry
	*this += uint128(a32 * b00) << 32;
	*this += uint128(a00 * b32) << 32;
	*this += a00 * b00;
	return *this;
}

// Increment/decrement operators.

inline uint128 uint128::operator++(int) {
	uint128 tmp(*this);
	*this += 1;
	return tmp;
}

inline uint128 uint128::operator--(int) {
	uint128 tmp(*this);
	*this -= 1;
	return tmp;
}

inline uint128 &uint128::operator++() {
	*this += 1;
	return *this;
}

inline uint128 &uint128::operator--() {
	*this -= 1;
	return *this;
}
}
