// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom/wide_int.h"

#define EXPECT_TAKEN(a) __builtin_expect(!!(a), 1)
#define EXPECT_NOT_TAKEN(a) __builtin_expect(!!(a), 0)

namespace fwk {

static const double wideint_mul_64bit = double(0x100000000LL) * double(0x100000000LL);

template <int N> bool WideInt<N>::operator==(const WideInt &that) const {
	if(m_count != that.m_count || is_neg() != that.is_neg())
		return false;
	for(int i = 0; i < m_count; ++i)
		if(m_chunks[i] != that.m_chunks[i])
			return false;
	return true;
}

template <int N> bool WideInt<N>::operator<(const WideInt &that) const {
	if(m_count != that.m_count || is_neg() != that.is_neg()) {
		int t1 = m_negative ? -m_count : m_count;
		int t2 = that.m_negative ? -that.m_count : that.m_count;
		return t1 < t2;
	}

	int i = m_count;
	if(!i)
		return false;
	do {
		--i;
		if(m_chunks[i] != that.m_chunks[i])
			return (m_chunks[i] < that.m_chunks[i]) ^ m_negative;
	} while(i);
	return false;
}

template <int N> auto WideInt<N>::p() const -> pair<fpt64, int> {
	pair<fpt64, int> ret_val;
	int sz = m_count;
	if(!sz) {
		return ret_val;
	} else {
		if(sz == 1) {
			ret_val.first = fpt64(m_chunks[0]);
		} else {
			ret_val.first = fpt64(m_chunks[sz - 1]) * wideint_mul_64bit + fpt64(m_chunks[sz - 2]);
			ret_val.second = (sz - 2) << 6;
		}
	}
	if(m_negative)
		ret_val.first = -ret_val.first;
	return ret_val;
}

template <int N> void WideInt<N>::add(const WideInt &e1, const WideInt &e2) {
	if(!e1.m_count) {
		*this = e2;
		return;
	}
	if(!e2.m_count) {
		*this = e1;
		return;
	}
	if(e1.m_negative ^ e2.m_negative)
		dif(e1.m_chunks, e1.m_count, e2.m_chunks, e2.m_count);
	else
		add(e1.m_chunks, e1.m_count, e2.m_chunks, e2.m_count);

	if(e1.m_negative)
		m_negative ^= 1;
}

template <int N> void WideInt<N>::dif(const WideInt &e1, const WideInt &e2) {
	if(!e1.m_count) {
		*this = e2;
		m_negative ^= 1;
		return;
	}
	if(!e2.m_count) {
		*this = e1;
		return;
	}

	if(e1.m_negative ^ e2.m_negative)
		add(e1.m_chunks, e1.m_count, e2.m_chunks, e2.m_count);
	else
		dif(e1.m_chunks, e1.m_count, e2.m_chunks, e2.m_count);

	if(e1.m_negative)
		m_negative ^= 1;
}

template <int N> void WideInt<N>::add(const uint64 *c1, int sz1, const uint64 *c2, int sz2) {
	if(EXPECT_TAKEN(sz1 <= 2 && sz2 <= 2)) {
		auto c1_high = c1[1] & (sz1 == 2 ? ~0LL : 0);
		auto c2_high = c2[1] & (sz2 == 2 ? ~0LL : 0);
		bool carry = (c1_high >> 63) | (c2_high >> 63);

		if(!carry) {
			uint128 v1(c1[0] | (uint128(c1_high) << 64));
			uint128 v2(c2[0] | (uint128(c2_high) << 64));
			uint128 sum = v1 + v2;

			m_negative = 0;
			m_chunks[0] = uint64(sum);
			m_chunks[1] = uint64(sum >> 64);
			m_count = m_chunks[1] ? 2 : 1;
			return;
		}
	}

	if(sz1 < sz2) {
		add(c2, sz2, c1, sz1);
		return;
	}

	m_count = sz1;
	m_negative = 0;
	uint128 temp = 0;
	for(int i = 0; i < sz2; ++i) {
		temp += uint128(c1[i]) + uint128(c2[i]);
		m_chunks[i] = uint64(temp);
		temp >>= 64;
	}
	for(int i = sz2; i < sz1; ++i) {
		temp += uint128(c1[i]);
		m_chunks[i] = uint64(temp);
		temp >>= 64;
	}
	if(temp && (m_count != N)) {
		m_chunks[m_count] = uint64(temp);
		++m_count;
	}
}

template <int N>
void WideInt<N>::dif(const uint64 *c1, int sz1, const uint64 *c2, int sz2, bool rec) {
	if(sz1 < sz2) {
		dif(c2, sz2, c1, sz1, true);
		m_negative ^= 1;
		return;
	} else if((sz1 == sz2) && !rec) {
		do {
			--sz1;
			if(c1[sz1] < c2[sz1]) {
				++sz1;
				dif(c2, sz1, c1, sz1, true);
				m_negative ^= 1;
				return;
			} else if(c1[sz1] > c2[sz1]) {
				++sz1;
				break;
			}
		} while(sz1);
		if(!sz1) {
			m_count = 0;
			m_negative = 0;
			return;
		}
		sz2 = sz1;
	}
	m_count = sz1 - 1;
	m_negative = 0;
	bool flag = false;
	for(int i = 0; i < sz2; ++i) {
		m_chunks[i] = c1[i] - c2[i] - (flag ? 1 : 0);
		flag = (c1[i] < c2[i]) || ((c1[i] == c2[i]) && flag);
	}
	for(int i = sz2; i < sz1; ++i) {
		m_chunks[i] = c1[i] - (flag ? 1 : 0);
		flag = !c1[i] && flag;
	}
	if(m_chunks[m_count])
		++m_count;
}

template <int N> void WideInt<N>::mul(const WideInt &e1, const WideInt &e2) {
	if(EXPECT_TAKEN(e1.m_count == 1 && e2.m_count == 1)) {
		auto result = uint128(e1.m_chunks[0]) * uint128(e2.m_chunks[0]);
		m_chunks[0] = uint64(result);
		m_chunks[1] = uint64(result >> 64);
		m_count = m_chunks[1] ? 2 : 1;
		m_negative = e1.m_negative ^ e2.m_negative;
		return;
	}

	if(!e1.m_count || !e2.m_count) {
		m_count = 0;
		m_negative = 0;
		return;
	}

	mul(e1.m_chunks, e1.m_count, e2.m_chunks, e2.m_count);
	m_negative = e1.m_negative ^ e2.m_negative;
}

template <int N> void WideInt<N>::mul(const uint64 *c1, int sz1, const uint64 *c2, int sz2) {
	uint128 cur = 0, nxt, tmp;
	m_count = min(N, sz1 + sz2 - 1);
	m_negative = 0;
	for(int shift = 0; shift < m_count; ++shift) {
		nxt = 0;

		int tstart = max(0, shift - sz2 + 1);
		int tend = min(sz1 - 1, shift);
		for(int i = tstart; i <= tend; ++i) {
			tmp = uint128(c1[i]) * uint128(c2[shift - i]);
			cur += uint64(tmp);
			nxt += tmp >> 64;
		}
		m_chunks[shift] = uint64(cur);
		cur = nxt + (cur >> 64);
	}
	if(cur && (m_count != N)) {
		m_chunks[m_count] = uint64(cur);
		++m_count;
	}
}

template class WideInt<32>;
template class WideInt<4>;
}
