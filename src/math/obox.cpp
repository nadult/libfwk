// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/math/obox.h"

#include "fwk/format.h"

namespace fwk {

template <class T> bool OBox<T>::isIntersecting(const OBox &rhs) const {
	auto lcorners = corners();
	auto rcorners = rhs.corners();

	// TODO: detect touch ?
	auto test_corners = [](CSpan<T> lcorners, CSpan<T> rcorners) {
		for(int n = 0; n < 4; n++) {
			T p1 = lcorners[n], p2 = lcorners[(n + 1) % 4];
			T vec = p2 - p1;

			bool inside = false;
			for(int j = 0; j < 4; j++) {
				using PT = PromoteIntegral<T>;
				auto side = cross<PT>(vec, rcorners[j] - p1);
				if(side < 0) {
					inside = true;
					break;
				}
			}
			if(!inside)
				return false;
		}
		return true;
	};

	return test_corners(lcorners, rcorners) && test_corners(rcorners, lcorners);
}

template class OBox<int2>;
template class OBox<short2>;
template class OBox<float2>;
template class OBox<double2>;
}
