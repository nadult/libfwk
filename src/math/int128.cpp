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

#include "fwk/math/int128.h"

#include "fwk/math/uint128.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace fwk {

int128 int128::maximumValue() { return int128(0x7fffffffffffffff, 0xfffffffffffffff); }
int128 int128::minimumValue() { return int128(static_cast<int64_t>(0x8000000000000000), 0x0); }

int128 &int128::operator*=(const int128 &rhs) {
	const uint64_t INT_MASK = 0xffffffff;
	const uint64_t CARRY_BIT = INT_MASK + 1;

	// Break the left and right numbers into 32 bit chunks
	// so that we can multiply them without overflow.
	uint64_t L0 = static_cast<uint64_t>(highbits) >> 32;
	uint64_t L1 = static_cast<uint64_t>(highbits) & INT_MASK;
	uint64_t L2 = lowbits >> 32;
	uint64_t L3 = lowbits & INT_MASK;
	uint64_t R0 = static_cast<uint64_t>(rhs.highbits) >> 32;
	uint64_t R1 = static_cast<uint64_t>(rhs.highbits) & INT_MASK;
	uint64_t R2 = rhs.lowbits >> 32;
	uint64_t R3 = rhs.lowbits & INT_MASK;

	uint64_t product = L3 * R3;
	lowbits = product & INT_MASK;
	uint64_t sum = product >> 32;
	product = L2 * R3;
	sum += product;
	highbits = sum < product ? CARRY_BIT : 0;
	product = L3 * R2;
	sum += product;
	if(sum < product) {
		highbits += CARRY_BIT;
	}
	lowbits += sum << 32;
	highbits += static_cast<int64_t>(sum >> 32);
	highbits += L1 * R3 + L2 * R2 + L3 * R1;
	highbits += (L0 * R3 + L1 * R2 + L2 * R1 + L3 * R0) << 32;
	return *this;
}

int128 int128::operator*(const int128 &rhs) const {
	int128 out = *this;
	out *= rhs;
	return out;
}

/**
   * Expands the given value into an array of ints so that we can work on
   * it. The array will be converted to an absolute value and the wasNegative
   * flag will be set appropriately. The array will remove leading zeros from
   * the value.
   * @param array an array of length 4 to set with the value
   * @param wasNegative a flag for whether the value was original negative
   * @result the output length of the array
   */
int64_t int128::fillInArray(uint32_t *array, bool &wasNegative) const {
	uint64_t high;
	uint64_t low;
	if(highbits < 0) {
		low = ~lowbits + 1;
		high = static_cast<uint64_t>(~highbits);
		if(low == 0) {
			high += 1;
		}
		wasNegative = true;
	} else {
		low = lowbits;
		high = static_cast<uint64_t>(highbits);
		wasNegative = false;
	}
	if(high != 0) {
		if(high > UINT32_MAX) {
			array[0] = static_cast<uint32_t>(high >> 32);
			array[1] = static_cast<uint32_t>(high);
			array[2] = static_cast<uint32_t>(low >> 32);
			array[3] = static_cast<uint32_t>(low);
			return 4;
		} else {
			array[0] = static_cast<uint32_t>(high);
			array[1] = static_cast<uint32_t>(low >> 32);
			array[2] = static_cast<uint32_t>(low);
			return 3;
		}
	} else if(low >= UINT32_MAX) {
		array[0] = static_cast<uint32_t>(low >> 32);
		array[1] = static_cast<uint32_t>(low);
		return 2;
	} else if(low == 0) {
		return 0;
	} else {
		array[0] = static_cast<uint32_t>(low);
		return 1;
	}
}

/**
   * Find last set bit in a 32 bit integer. Bit 1 is the LSB and bit 32 is
   * the MSB. We can replace this with bsrq asm instruction on x64.
   */
int64_t fls(uint32_t x) {
	int64_t bitpos = 0;
	while(x) {
		x >>= 1;
		bitpos += 1;
	}
	return bitpos;
}

/**
   * Shift the number in the array left by bits positions.
   * @param array the number to shift, must have length elements
   * @param length the number of entries in the array
   * @param bits the number of bits to shift (0 <= bits < 32)
   */
void shiftArrayLeft(uint32_t *array, int64_t length, int64_t bits) {
	if(length > 0 && bits != 0) {
		for(int64_t i = 0; i < length - 1; ++i) {
			array[i] = (array[i] << bits) | (array[i + 1] >> (32 - bits));
		}
		array[length - 1] <<= bits;
	}
}

/**
   * Shift the number in the array right by bits positions.
   * @param array the number to shift, must have length elements
   * @param length the number of entries in the array
   * @param bits the number of bits to shift (0 <= bits < 32)
   */
void shiftArrayRight(uint32_t *array, int64_t length, int64_t bits) {
	if(length > 0 && bits != 0) {
		for(int64_t i = length - 1; i > 0; --i) {
			array[i] = (array[i] >> bits) | (array[i - 1] << (32 - bits));
		}
		array[0] >>= bits;
	}
}

/**
   * Fix the signs of the result and remainder at the end of the division
   * based on the signs of the dividend and divisor.
   */
void fixDivisionSigns(int128 &result, int128 &remainder, bool dividendWasNegative,
					  bool divisorWasNegative) {
	if(dividendWasNegative != divisorWasNegative) {
		result.negate();
	}
	if(dividendWasNegative) {
		remainder.negate();
	}
}

/**
   * Build a int128 from a list of ints.
   */
void buildFromArray(int128 &value, uint32_t *array, int64_t length) {
	PASSERT(length <= 5);

	switch(length) {
	case 0:
		value = 0;
		break;
	case 1:
		value = array[0];
		break;
	case 2:
		value = int128(0, (static_cast<uint64_t>(array[0]) << 32) + array[1]);
		break;
	case 3:
		value = int128(array[0], (static_cast<uint64_t>(array[1]) << 32) + array[2]);
		break;
	case 4:
		value = int128((static_cast<int64_t>(array[0]) << 32) + array[1],
					   (static_cast<uint64_t>(array[2]) << 32) + array[3]);
		break;
	case 5:
		PASSERT(array[0] == 0);
		value = int128((static_cast<int64_t>(array[1]) << 32) + array[2],
					   (static_cast<uint64_t>(array[3]) << 32) + array[4]);
		break;
	}
}

/**
   * Do a division where the divisor fits into a single 32 bit value.
   */
int128 singleDivide(uint32_t *dividend, int64_t dividendLength, uint32_t divisor, int128 &remainder,
					bool dividendWasNegative, bool divisorWasNegative) {
	uint64_t r = 0;
	uint32_t resultArray[5];
	for(int64_t j = 0; j < dividendLength; j++) {
		r <<= 32;
		r += dividend[j];
		resultArray[j] = static_cast<uint32_t>(r / divisor);
		r %= divisor;
	}
	int128 result;
	buildFromArray(result, resultArray, dividendLength);
	remainder = static_cast<int64_t>(r);
	fixDivisionSigns(result, remainder, dividendWasNegative, divisorWasNegative);
	return result;
}

int128 int128::divide(const int128 &divisor, int128 &remainder) const {
	// Split the dividend and divisor into integer pieces so that we can
	// work on them.
	uint32_t dividendArray[5];
	uint32_t divisorArray[4];
	bool dividendWasNegative;
	bool divisorWasNegative;
	// leave an extra zero before the dividend
	dividendArray[0] = 0;
	int64_t dividendLength = fillInArray(dividendArray + 1, dividendWasNegative) + 1;
	int64_t divisorLength = divisor.fillInArray(divisorArray, divisorWasNegative);
	PASSERT(divisorLength != 0);

	// Handle some of the easy cases.
	if(dividendLength <= divisorLength) {
		remainder = *this;
		return 0;
	} else if(divisorLength == 1) {
		return singleDivide(dividendArray, dividendLength, divisorArray[0], remainder,
							dividendWasNegative, divisorWasNegative);
	}

	int64_t resultLength = dividendLength - divisorLength;
	uint32_t resultArray[4];

	// Normalize by shifting both by a multiple of 2 so that
	// the digit guessing is better. The requirement is that
	// divisorArray[0] is greater than 2**31.
	int64_t normalizeBits = 32 - fls(divisorArray[0]);
	shiftArrayLeft(divisorArray, divisorLength, normalizeBits);
	shiftArrayLeft(dividendArray, dividendLength, normalizeBits);

	// compute each digit in the result
	for(int64_t j = 0; j < resultLength; ++j) {
		// Guess the next digit. At worst it is two too large
		uint32_t guess = UINT32_MAX;
		uint64_t highDividend =
			static_cast<uint64_t>(dividendArray[j]) << 32 | dividendArray[j + 1];
		if(dividendArray[j] != divisorArray[0]) {
			guess = static_cast<uint32_t>(highDividend / divisorArray[0]);
		}

		// catch all of the cases where guess is two too large and most of the
		// cases where it is one too large
		uint32_t rhat =
			static_cast<uint32_t>(highDividend - guess * static_cast<uint64_t>(divisorArray[0]));
		while(static_cast<uint64_t>(divisorArray[1]) * guess >
			  (static_cast<uint64_t>(rhat) << 32) + dividendArray[j + 2]) {
			guess -= 1;
			rhat += divisorArray[0];
			if(static_cast<uint64_t>(rhat) < divisorArray[0]) {
				break;
			}
		}

		// subtract off the guess * divisor from the dividend
		uint64_t mult = 0;
		for(int64_t i = divisorLength - 1; i >= 0; --i) {
			mult += static_cast<uint64_t>(guess) * divisorArray[i];
			uint32_t prev = dividendArray[j + i + 1];
			dividendArray[j + i + 1] -= static_cast<uint32_t>(mult);
			mult >>= 32;
			if(dividendArray[j + i + 1] > prev) {
				mult += 1;
			}
		}
		uint32_t prev = dividendArray[j];
		dividendArray[j] -= static_cast<uint32_t>(mult);

		// if guess was too big, we add back divisor
		if(dividendArray[j] > prev) {
			guess -= 1;
			uint32_t carry = 0;
			for(int64_t i = divisorLength - 1; i >= 0; --i) {
				uint64_t sum =
					static_cast<uint64_t>(divisorArray[i]) + dividendArray[j + i + 1] + carry;
				dividendArray[j + i + 1] = static_cast<uint32_t>(sum);
				carry = static_cast<uint32_t>(sum >> 32);
			}
			dividendArray[j] += carry;
		}

		resultArray[j] = guess;
	}

	// denormalize the remainder
	shiftArrayRight(dividendArray, dividendLength, normalizeBits);

	// return result and remainder
	int128 result;
	buildFromArray(result, resultArray, resultLength);
	buildFromArray(remainder, dividendArray, dividendLength);
	fixDivisionSigns(result, remainder, dividendWasNegative, divisorWasNegative);
	return result;
}

int128 int128::operator/(const int128 &rhs) const {
	int128 remainder;
	divide(rhs, remainder);
	return rhs;
}

int128 int128::operator%(const int128 &rhs) const {
	DASSERT(*this >= 0 && rhs >= 0);
	return int128(uint128(*this) % uint128(rhs));
}
}
