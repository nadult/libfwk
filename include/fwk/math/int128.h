/*
 int128  comes from Apache::Orc: https://orc.apache.org/
 Code was slightly adapted to libfwk by Krzysztof Jakubowski.

 Licensed to the Apache Software Foundation (ASF) under one
 or more contributor license agreements.  See the NOTICE file
 distributed with this work for additional information
 regarding copyright ownership.  The ASF licenses this file
 to you under the Apache License, Version 2.0 (the
 "License"); you may not use this file except in compliance
 with the License.  You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
 */

#pragma once

#include "fwk/sys_base.h"
#include <cmath>
#include <string>

namespace fwk {

#ifndef __LITTLE_ENDIAN__
#error Only little endian supported for now :(
#endif

// Represents a signed 128-bit integer in two's complement.
// Calculations wrap around and overflow is ignored.
// For a discussion of the algorithms, look at Knuth's volume 2,
// Semi-numerical Algorithms section 4.3.1.
class int128 {
  public:
	int128() : highbits(0), lowbits(0) {}
	int128(int64_t rhs) {
		if(rhs >= 0) {
			highbits = 0;
			lowbits = static_cast<uint64_t>(rhs);
		} else {
			highbits = -1;
			lowbits = static_cast<uint64_t>(rhs);
		}
	}

	int128(int64_t high, uint64_t low) {
		highbits = high;
		lowbits = low;
	}

	explicit operator i64() const { return (i64)lowbits; }
	explicit operator u64() const { return (u64)lowbits; }
	explicit operator i32() const { return (i32)lowbits; }
	explicit operator u32() const { return (u32)lowbits; }
	explicit operator double() const { return double(lowbits) + std::ldexp(double(highbits), 64); }
	explicit operator float() const { return float(double(*this)); }
	explicit operator bool() const { return lowbits != 0 || highbits != 0; }

	static int128 maximumValue();
	static int128 minimumValue();

	int128 operator-() const {
		auto lo = ~lowbits + 1;
		auto hi = ~highbits;
		if(!lo)
			hi++;
		return {hi, lo};
	}

	int128 &negate() {
		lowbits = ~lowbits + 1;
		highbits = ~highbits;
		if(lowbits == 0) {
			highbits += 1;
		}
		return *this;
	}

	// TODO: convert to +, -, use automatic +=
	int128 &operator+=(const int128 &rhs) {
		uint64_t sum = lowbits + rhs.lowbits;
		highbits += rhs.highbits;
		if(sum < lowbits) {
			highbits += 1;
		}
		lowbits = sum;
		return *this;
	}

	int128 &operator-=(const int128 &rhs) {
		uint64_t diff = lowbits - rhs.lowbits;
		highbits -= rhs.highbits;
		if(diff > lowbits) {
			highbits -= 1;
		}
		lowbits = diff;
		return *this;
	}

	int128 &operator*=(const int128 &);

	int128 divide(const int128 &rhs, int128 &remainder) const;

	int128 operator*(const int128 &) const;
	int128 operator/(const int128 &) const;
	int128 operator%(const int128 &) const;

	int128 operator+(const int128 &rhs) const {
		int128 temp = *this;
		temp += rhs;
		return temp;
	}

	int128 operator-(const int128 &rhs) const {
		int128 temp = *this;
		temp -= rhs;
		return temp;
	}

	int128 &operator|=(const int128 &rhs) {
		lowbits |= rhs.lowbits;
		highbits |= rhs.highbits;
		return *this;
	}

	int128 &operator&=(const int128 &rhs) {
		lowbits &= rhs.lowbits;
		highbits &= rhs.highbits;
		return *this;
	}

	int128 operator&(const int128 &rhs) {
		int128 value = *this;
		value &= rhs;
		return value;
	}

	int128 &operator<<=(uint32_t bits) {
		if(bits != 0) {
			if(bits < 64) {
				highbits <<= bits;
				highbits |= (lowbits >> (64 - bits));
				lowbits <<= bits;
			} else if(bits < 128) {
				highbits = static_cast<int64_t>(lowbits) << (bits - 64);
				lowbits = 0;
			} else {
				highbits = 0;
				lowbits = 0;
			}
		}
		return *this;
	}

	int128 &operator>>=(uint32_t bits) {
		if(bits != 0) {
			if(bits < 64) {
				lowbits >>= bits;
				lowbits |= static_cast<uint64_t>(highbits << (64 - bits));
				highbits = static_cast<int64_t>(static_cast<uint64_t>(highbits) >> bits);
			} else if(bits < 128) {
				lowbits = static_cast<uint64_t>(highbits >> (bits - 64));
				highbits = highbits >= 0 ? 0 : -1l;
			} else {
				highbits = highbits >= 0 ? 0 : -1l;
				lowbits = static_cast<uint64_t>(highbits);
			}
		}
		return *this;
	}
	int128 operator>>(uint32_t bits) const {
		int128 out(*this);
		out >>= bits;
		return out;
	}

	bool operator==(const int128 &rhs) const {
		return highbits == rhs.highbits && lowbits == rhs.lowbits;
	}

	bool operator!=(const int128 &rhs) const {
		return highbits != rhs.highbits || lowbits != rhs.lowbits;
	}

	bool operator<(const int128 &rhs) const {
		if(highbits == rhs.highbits)
			return lowbits < rhs.lowbits;
		else
			return highbits < rhs.highbits;
	}

	bool operator<=(const int128 &rhs) const {
		if(highbits == rhs.highbits)
			return lowbits <= rhs.lowbits;
		else
			return highbits <= rhs.highbits;
	}

	bool operator>(const int128 &rhs) const {
		if(highbits == rhs.highbits)
			return lowbits > rhs.lowbits;
		else
			return highbits > rhs.highbits;
	}

	bool operator>=(const int128 &rhs) const {
		if(highbits == rhs.highbits)
			return lowbits >= rhs.lowbits;
		else
			return highbits >= rhs.highbits;
	}

	u32 hash() const {
		return u32(highbits >> 32) ^ u32(highbits) ^ u32(lowbits >> 32) ^ u32(lowbits);
	}

	bool fitsInLong() const {
		switch(highbits) {
		case 0:
			return 0 == (lowbits & LONG_SIGN_BIT);
		case -1:
			return 0 != (lowbits & LONG_SIGN_BIT);
		default:
			return false;
		}
	}

	int64_t getHighBits() { return highbits; }
	uint64_t getLowBits() { return lowbits; }

	friend int128 abs(int128 val) { return val.highbits < 0 ? -val : val; }

	/**
     * Represent the absolute number as a list of uint32.
     * Visible for testing only.
     * @param array the array that is set to the value of the number
     * @param wasNegative set to true if the original number was negative
     * @return the number of elements that were set in the array (1 to 4)
     */
	int64_t fillInArray(uint32_t *array, bool &wasNegative) const;

  private:
	static const uint64_t LONG_SIGN_BIT = 0x8000000000000000u;
	int64_t highbits;
	uint64_t lowbits;
};
}
