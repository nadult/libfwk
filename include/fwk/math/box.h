// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math_base.h"
#include "fwk/maybe.h"

namespace fwk {

#define ENABLE_IF_SIZE(n) template <class U = Vector, EnableInDimension<U, n>...>

// Axis-aligned box (or rect in 2D case)
// Invariant: min <= max (use validRange)
template <class T> class Box {
	Box(T min, T max, NoAssertsTag) : m_min(min), m_max(max) {}

  public:
	static_assert(isVec<T>(), "Box<> has to be based on a vector");

	using Scalar = typename T::Scalar;
	using Vector = T;
	using Vector2 = vec2<Scalar>;
	using Point = Vector;

	enum { dim_size = T::vec_size, num_corners = 1 << dim_size };

	// min <= max in all dimensions; can be empty
	static bool validRange(const Point &min, const Point &max) {
		for(int i = 0; i < dim_size; i++)
			if(!(min[i] <= max[i]))
				return false;
		return true;
	}

	// min >= max in any dimension
	static bool emptyRange(const Point &min, const Point &max) {
		for(int n = 0; n < dim_size; n++)
			if(!(min[n] < max[n]))
				return true;
		return false;
	}

	ENABLE_IF_SIZE(2)
	Box(Scalar min_x, Scalar min_y, Scalar max_x, Scalar max_y)
		: Box({min_x, min_y}, {max_x, max_y}) {}
	ENABLE_IF_SIZE(3)
	Box(Scalar min_x, Scalar min_y, Scalar min_z, Scalar max_x, Scalar max_y, Scalar max_z)
		: Box({min_x, min_y, min_z}, {max_x, max_y, max_z}) {}

	Box(Point min, Point max) : m_min(min), m_max(max) { DASSERT(validRange(min, max)); }
	explicit Box(T size) : Box(T(), size) {}
	Box() : m_min(), m_max() {}

	template <class U, EnableIf<preciseConversion<U, T>()>...>
	Box(const Box<U> &rhs) : Box(T(rhs.min()), T(rhs.max())) {}
	template <class U, EnableIf<!preciseConversion<U, T>()>...>
	explicit Box(const Box<U> &rhs) : Box(T(rhs.min()), T(rhs.max())) {}

	Scalar min(int i) const { return m_min[i]; }
	Scalar max(int i) const { return m_max[i]; }

	const Point &min() const { return m_min; }
	const Point &max() const { return m_max; }

	void setMin(const T &min) {
		m_min = min;
		DASSERT(validRange(m_min, m_max));
	}
	void setMax(const T &max) {
		m_max = max;
		DASSERT(validRange(m_min, m_max));
	}
	void setSize(const T &size) {
		m_max = m_min + size;
		DASSERT(validRange(m_min, m_max));
	}

	Scalar x() const { return m_min[0]; }
	Scalar y() const { return m_min[1]; }
	ENABLE_IF_SIZE(3) Scalar z() const { return m_min[2]; }

	Scalar ex() const { return m_max[0]; }
	Scalar ey() const { return m_max[1]; }
	ENABLE_IF_SIZE(3) Scalar ez() const { return m_max[2]; }

	Scalar width() const { return size(0); }
	Scalar height() const { return size(1); }
	ENABLE_IF_SIZE(3) Scalar depth() const { return size(2); }

	ENABLE_IF_SIZE(2) Scalar surfaceArea() const { return width() * height(); }
	ENABLE_IF_SIZE(3) Scalar volume() const { return width() * height() * depth(); }

	Scalar size(int axis) const { return m_max[axis] - m_min[axis]; }
	T size() const { return m_max - m_min; }
	Point center() const { return (m_max + m_min) / Scalar(2); }

	Box operator+(const T &offset) const { return Box(m_min + offset, m_max + offset); }
	Box operator-(const T &offset) const { return Box(m_min - offset, m_max - offset); }
	Box operator*(const T &scale) const {
		T tmin = m_min * scale, tmax = m_max * scale;
		for(int n = 0; n < dim_size; n++)
			if(scale[n] < Scalar(0))
				swap(tmin[n], tmax[n]);
		return {tmin, tmax, no_asserts_tag};
	}

	Box operator*(Scalar scale) const {
		auto tmin = m_min * scale, tmax = m_max * scale;
		if(scale < Scalar(0))
			swap(tmin, tmax);
		return {tmin, tmax, no_asserts_tag};
	}

	bool empty() const { return emptyRange(m_min, m_max); }

	bool contains(const Point &point) const {
		for(int i = 0; i < dim_size; i++)
			if(!(point[i] >= m_min[i] && point[i] <= m_max[i]))
				return false;
		return true;
	}

	bool contains(const Box &box) const { return box == intersection(box); }

	bool containsCell(const T &pos) const {
		for(int i = 0; i < dim_size; i++)
			if(!(pos[i] >= m_min[i] && pos[i] + Scalar(1) <= m_max[i]))
				return false;
		return true;
	}

	Scalar cellCount(int axis) const { return max(size(axis) - Scalar(1), Scalar(0)); }
	T cellCount() const { return vmax(size() - T(Scalar(1)), T(Scalar(0))); }

	ENABLE_IF_SIZE(2) array<Point, 4> corners() const {
		return {{m_min, {m_min[0], m_max[1]}, m_max, {m_max[0], m_min[1]}}};
	}

	ENABLE_IF_SIZE(3) array<Point, num_corners> corners() const {
		array<T, num_corners> out;
		for(int n = 0; n < num_corners; n++)
			for(int i = 0; i < dim_size; i++)
				out[n][i] = (n & (1 << i) ? m_max : m_min)[i];
		return out;
	}

	Box intersectionOrEmpty(const Box &rhs) const {
		auto tmin = vmax(m_min, rhs.m_min);
		auto tmax = vmin(m_max, rhs.m_max);

		if(!emptyRange(tmin, tmax))
			return Box{tmin, tmax, no_asserts_tag};
		return Box{};
	}

	// When Boxes touch, returned Box will be empty
	Maybe<Box> intersection(const Box &rhs) const {
		auto tmin = vmax(m_min, rhs.m_min);
		auto tmax = vmin(m_max, rhs.m_max);
		return Box{tmin, tmax, no_asserts_tag};
	}

	Point closestPoint(const Point &point) const { return vmin(vmax(point, m_min), m_max); }

	Scalar distanceSq(const Point &point) const {
		return fwk::distanceSq(point, closestPoint(point));
	}

	auto distanceSq(const Box &rhs) const {
		Point p1 = vclamp(rhs.center(), m_min, m_max);
		Point p2 = vclamp(p1, rhs.m_min, rhs.m_max);
		return fwk::distanceSq(p1, p2);
	}

	auto distance(const Point &point) const { return std::sqrt(distanceSq(point)); }
	auto distance(const Box &box) const { return std::sqrt(distanceSq(box)); }

	Box inset(const T &val_min, const T &val_max) const {
		auto new_min = m_min + val_min, new_max = m_max - val_max;
		return {vmin(new_min, new_max), vmax(new_min, new_max), no_asserts_tag};
	}
	Box inset(const T &value) const { return inset(value, value); }
	Box inset(Scalar value) const { return inset(T(value)); }

	Box enlarge(const T &val_min, const T &val_max) const { return inset(-val_min, -val_max); }
	Box enlarge(const T &value) const { return inset(-value); }
	Box enlarge(Scalar value) const { return inset(T(-value)); }

	FWK_ORDER_BY(Box, m_min, m_max);

	ENABLE_IF_SIZE(3) Box<Vector2> xz() const { return {m_min.xz(), m_max.xz()}; }
	ENABLE_IF_SIZE(3) Box<Vector2> xy() const { return {m_min.xy(), m_max.xy()}; }
	ENABLE_IF_SIZE(3) Box<Vector2> yz() const { return {m_min.yz(), m_max.yz()}; }

	Box(Invalid) : m_min(), m_max(-1) {}
	bool valid() const { return validRange(m_min, m_max); } // Invariant

  private:
	union {
		struct {
			Point m_min, m_max;
		};
		Point m_v[2];
	};
};

#undef ENABLE_IF_SIZE

template <class T> Box<T> enclose(const Box<T> &box) { return box; }

template <class TRange, class T = RemoveConst<RangeBase<TRange>>, EnableIfVec<T>...>
Box<T> enclose(const TRange &points) {
	if(fwk::empty(points))
		return {};

	auto it = begin(points);
	T tmin = *it, tmax = *it;
	while(it != end(points)) {
		tmin = vmin(tmin, *it);
		tmax = vmax(tmax, *it);
		++it;
	}
	return {tmin, tmax};
}

template <class T, EnableIfRealVec<T>...> auto encloseIntegral(const Box<T> &box) {
	using IVec = MakeVec<int, T::vec_size>;
	T min = vfloor(box.min());
	T max = vceil(box.max());
	return Box<IVec>{IVec(min), IVec(max)};
}

template <class T> Box<T> enclose(const Box<T> &lhs, const Box<T> &rhs) {
	return {vmin(lhs.min(), rhs.min()), vmax(lhs.max(), rhs.max())};
}

template <class T> Box<T> encloseNotEmpty(const Box<T> &lhs, const Box<T> &rhs) {
	return lhs.empty() ? rhs : rhs.empty() ? lhs : enclose(lhs, rhs);
}

template <class T> Box<T> enclose(const Box<T> &box, const T &point) {
	return {vmin(box.min(), point), vmax(box.max(), point)};
}

template <class T> Box<T> enclose(const T &point, const Box<T> &box) { return enclose(box, point); }

template <class T> auto intersection(const Box<T> &lhs, const Box<T> &rhs) {
	return lhs.intersection(rhs);
}

template <class T> auto intersectionOrEmpty(const Box<T> &lhs, const Box<T> &rhs) {
	return lhs.intersectionOrEmpty(rhs);
}

template <class T> bool overlaps(const Box<T> &lhs, const Box<T> &rhs) {
	auto result = lhs.intersection(rhs);
	return result && !result->empty();
}

template <class T> bool overlapsNotEmpty(const Box<T> &lhs, const Box<T> &rhs) {
	return !lhs.empty() && !rhs.empty() && overlaps(lhs, rhs);
}

template <class T> bool touches(const Box<T> &lhs, const Box<T> &rhs) {
	auto result = lhs.intersection(rhs);
	return result && result->empty();
}

FBox encloseTransformed(const FBox &, const Matrix4 &);

template <class T> constexpr bool isEnclosable() {
	if constexpr(isMathObject<T>() && !isVec<T>()) {
		return isSame<decltype(enclose(declval<T>())), Box<typename T::Vector>>();
	} else {
		return false;
	}
}

template <class TRange, class T = RangeBase<TRange>, EnableIf<isEnclosable<T>()>...>
auto enclose(const TRange &objects) {
	using Box = decltype(enclose(declval<T>()));
	if(objects.empty())
		return Box();

	auto it = begin(objects);
	auto out = enclose(*it++);

	while(it != end(objects)) {
		auto enclosed = enclose(*it);
		out = {vmin(out.min(), enclosed.min()), vmax(out.max(), enclosed.max())};
		++it;
	}
	return out;
}

array<Plane3F, 6> planes(const FBox &);
array<pair<float3, float3>, 12> edges(const FBox &);
array<float3, 8> verts(const FBox &);
}
