// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_math.h"
#include "fwk_xml.h"

#include "fwk_profile.h"

namespace fwk {
/*
 *edge2edge2

 Triangle::Triangle(const float3 &a, const float3 &b, const float3 &c) {
 -   m_point = a;
 -   m_edge[0] = b - a;
 -   m_edge[1] = c - a;
 -   m_normal = fwk::cross(edge1(), edge2());
 -   m_length = length(m_normal);
 -   m_normal /= m_length;

 * */

Triangle3F operator*(const Matrix4 &mat, const Triangle3F &tri) {
	return {mulPoint(mat, tri[0]), mulPoint(mat, tri[1]), mulPoint(mat, tri[2])};
}

template <class T, int N> T Triangle<T, N>::surfaceArea() const {
	if
		constexpr(N == 2) return std::abs(cross(v[1] - v[0], v[2] - v[0])) / T(2);
	else
		return length(cross(v[1] - v[0], v[2] - v[0])) / T(2);
}

template <class T, int N>
template <class U, EnableInDimension<U, 3>...>
auto Triangle<T, N>::normal() const -> Vector {
	return normalize(cross(v[1] - v[0], v[2] - v[0]));
}

template <class T, int N>
template <class U, EnableInDimension<U, 3>...>
auto Triangle<T, N>::barycentric(const Point &point) const -> Vector {
	Vector diff = point - v[0];
	Vector nrm = normal();
	T tw = dot(fwk::cross(v[1] - v[0], diff), nrm);
	T tv = dot(fwk::cross(v[2] - v[0], diff), nrm);
	T tu = T(1) - tw - tv;
	return {tu, tv, tw};
}

template <class T, int N> auto Triangle<T, N>::sampleEven(float density) const -> vector<Point> {
	Vector vec1 = v[1] - v[0], vec2 = v[2] - v[0];
	T stepx = T(1) / (length(vec1) * density);
	T stepy = T(1) / (length(vec2) * density);
	vector<Point> out;
	// TODO: reserve

	for(T x = 0; x < T(1); x += stepx)
		for(T y = 0; x + y < T(1); y += stepy)
			out.emplace_back(vec1 * x + vec2 * y + v[0]);

	return out;
}

template <class T, int N>
// Source: realtimecollisiondetection (book)
auto Triangle<T, N>::closestPoint(const Point &point) const -> Point {
	if
		constexpr(dim_size == 2) {
			// TODO: optimize me
			return Triangle3<T>{asXZ(v[0]), asXZ(v[1]), asXZ(v[2])}.closestPoint(asXZ(point)).xz();
		}

	// Check if P in vertex region outside A
	Vector ab = v[1] - v[0];
	Vector ac = v[2] - v[0];
	Vector ap = point - v[0];
	T d1 = dot(ab, ap);
	T d2 = dot(ac, ap);
	if(d1 <= T(0) && d2 <= T(0))
		return v[0]; // barycentric coordinates (1,0,0)

	// Check if P in vertex region outside B
	Vector bp = point - v[1];
	T d3 = dot(ab, bp);
	T d4 = dot(ac, bp);
	if(d3 >= T(0) && d4 <= d3)
		return v[1]; // barycentric coordinates (0,1,0)

	// Check if P in edge region of AB, if so return projection of P onto AB
	T vc = d1 * d4 - d3 * d2;
	if(vc <= T(0) && d1 >= T(0) && d3 <= T(0)) {
		T w = d1 / (d1 - d3);
		return v[0] + ab * w; // barycentric coordinates (1-w,w,0)
	}

	// Check if P in vertex region outside C
	Vector cp = point - v[2];
	T d5 = dot(ab, cp);
	T d6 = dot(ac, cp);
	if(d6 >= T(0) && d5 <= d6)
		return v[2]; // barycentric coordinates (0,0,1)

	// Check if P in edge region of AC, if so return projection of P onto AC
	T vb = d5 * d2 - d1 * d6;
	if(vb <= T(0) && d2 >= T(0) && d6 <= T(0)) {
		T w = d2 / (d2 - d6);
		return v[0] + ac * w; // barycentric coordinates (1-w,0,w)
	}

	// Check if P in edge region of BC, if so return projection of P onto BC
	T va = d3 * d6 - d5 * d4;
	if(va <= T(0) && (d4 - d3) >= T(0) && (d5 - d6) >= T(0)) {
		T w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
		return v[1] + (v[2] - v[1]) * w; // barycentric coordinates (0,1-w,w)
	}

	// P inside face region. Compute Q through its barycentric coordinates (u,v,w)
	T denom = T(1) / (va + vb + vc);
	T tv = vb * denom;
	T tw = vc * denom;
	return v[0] + ab * tv + ac * tw; // = u*a + v*b + w*c, u = va * denom = 1 - v - w
}

template <class T, int N> T Triangle<T, N>::distance(const Point &point) const {
	return fwk::distance(closestPoint(point), point);
}

template <class T, int N>
template <class U, EnableInDimension<U, 3>...>
Triangle2<T> Triangle<T, N>::projection2D() const {
	if
		constexpr(!std::is_same<T, float>::value) {
			FATAL("Please fix me!");
			return {};
		}
	else {
		Projection proj(*this);
		return {(proj * v[0]).xz(), (proj * v[1]).xz(), (proj * v[2]).xz()};
	}
}

template <class T, int N> array<T, 3> Triangle<T, N>::angles() const {
	if
		constexpr(dim_size == 3) { return projection2D().angles(); }
	else {
		array<T, 3> out;
		// TODO: verify this
		Vector dirs[3];
		for(int i = 0; i < 3; i++)
			dirs[i] = normalize(v[i] - v[(i + 2) % 3]);
		for(int n = 0; n < 3; n++)
			out[n] = constant<Scalar>::pi * Scalar(2) - angleBetween(-dirs[n], dirs[(n + 1) % 3]);

		return out;
	}
}

template class Triangle<float, 2>;
template class Triangle<float, 3>;
template class Triangle<double, 2>;
template class Triangle<double, 3>;

template float3 Triangle3<float>::normal() const;
template double3 Triangle3<double>::normal() const;
}
