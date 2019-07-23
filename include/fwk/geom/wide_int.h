// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom_base.h"

namespace fwk {

// Based on extended_int from boost::polygon (Copyright Andrii Sydorchuk 2010-2012)
// see file: boost_polygon/detail/voronoi_ctypes.hpp
template <int N> class WideInt {
  public:
	using uint64 = unsigned long long;
	using int64 = long long;
	using uint128 = __uint128_t;
	using int128 = __int128;
	using fpt64 = double;

	WideInt() {}

	WideInt(int64 that) { (*this) = that; }
	template <int M> WideInt(const WideInt<M> &that) { (*this) = that; }

	WideInt(CSpan<uint64> &chunks, bool plus = true) {
		m_count = min(N, chunks.size());
		for(int i = 0; i < m_count; ++i)
			m_chunks[i] = chunks[chunks.size() - i - 1];
		m_negative = !plus;
	}

	void operator=(int64 that) {
		if(that == 0) {
			m_count = m_negative = 0;
			return;
		}

		m_count = 1;
		m_negative = that < 0;
		m_chunks[0] = std::abs(that);
	}

	template <int M> WideInt &operator=(const WideInt<M> &that) {
		static_assert(M <= N);
		m_count = that.m_count;
		m_negative = that.m_negative;
		std::memcpy(m_chunks, that.m_chunks, that.m_count * sizeof(uint64));
		return *this;
	}

	bool is_pos() const { return !m_negative && m_count != 0; }
	bool is_neg() const { return m_negative && m_count != 0; }
	bool is_zero() const { return m_count == 0; }

	bool operator==(const WideInt &that) const;
	bool operator<(const WideInt &that) const;

	bool operator!=(const WideInt &that) const { return !(*this == that); }
	bool operator>(const WideInt &that) const { return that < *this; }
	bool operator<=(const WideInt &that) const { return !(that < *this); }
	bool operator>=(const WideInt &that) const { return !(*this < that); }

	WideInt operator-() const {
		WideInt ret_val = *this;
		ret_val.neg();
		return ret_val;
	}

	void neg() { m_negative ^= 1; }

	WideInt operator+(const WideInt &that) const {
		WideInt ret_val;
		ret_val.add(*this, that);
		return ret_val;
	}

	WideInt operator-(const WideInt &that) const {
		WideInt ret_val;
		ret_val.dif(*this, that);
		return ret_val;
	}

	void add(const WideInt &e1, const WideInt &e2);
	void dif(const WideInt &e1, const WideInt &e2);

	WideInt operator*(int64 that) const {
		WideInt temp(that);
		return (*this) * temp;
	}

	WideInt operator*(const WideInt &that) const {
		WideInt ret_val;
		ret_val.mul(*this, that);
		return ret_val;
	}

	void mul(const WideInt &e1, const WideInt &e2);

	pair<fpt64, int> p() const;

	fpt64 d() const {
		auto pair = p();
		return std::ldexp(pair.first, pair.second);
	}

  private:
	void add(const uint64 *c1, int sz1, const uint64 *c2, int sz2);
	void dif(const uint64 *c1, int sz1, const uint64 *c2, int sz2, bool rec = false);
	void mul(const uint64 *c1, int sz1, const uint64 *c2, int sz2);

	uint64 m_chunks[N];
	int m_count;
	int m_negative;
};
}
