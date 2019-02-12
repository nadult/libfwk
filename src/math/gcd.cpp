#include "fwk/math/gcd.h"

namespace fwk {

namespace {

	int ctz(unsigned x) { return __builtin_ctz(x); }
	int ctz(unsigned long long x) { return __builtin_ctzll(x); }

	int ctz(__uint128_t x) {
		unsigned long long hi = x >> 64;
		unsigned long long lo = x;

		int ctz_hi = __builtin_ctzll(hi);
		int ctz_lo = __builtin_ctzll(lo);
		return lo ? ctz_lo : 64 + ctz_hi;
	}

	template <class T> T gcdBinary(T u, T v) {
		if(u == 0)
			return v;
		if(v == 0)
			return u;

		unsigned shift = ctz(u | v);
		u >>= ctz(u);

		do {
			v >>= ctz(v);
			if(u > v)
				swap(u, v);
			v = v - u;
		} while(v);

		return u << shift;
	}
}

int gcd(int a, int b) { return gcdBinary<unsigned>(fwk::abs(a), fwk::abs(b)); }
llint gcd(llint a, llint b) { return gcdBinary<unsigned long long>(fwk::abs(a), fwk::abs(b)); }
qint gcd(qint a, qint b) { return gcdBinary<__uint128_t>(fwk::abs(a), fwk::abs(b)); }

template <class T> vector<Pair<T, int>> extractPrimes(T value) {
	vector<Pair<T, int>> out;

	for(T n = 2; n <= value; n++) {
		int count = 0;
		while(value % n == 0) {
			count++;
			value /= n;
		}
		if(count > 0)
			out.emplace_back(n, count);
	}

	return out;
}

template vector<Pair<int, int>> extractPrimes(int);
template vector<Pair<llint, int>> extractPrimes(llint);
}
