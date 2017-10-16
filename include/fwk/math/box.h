// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk_math.h"
#include "fwk_maybe.h"

namespace fwk {

#define ENABLE_IF_SIZE(n) template <class U = Vector, EnableInDimension<U, n>...>

// Axis-aligned box (or rect in 2D case)
// Invariant: min <= max (use validRange)
template <class T> class Box {
	using NoAsserts = detail::NoAssertsTag;
	Box(T min, T max, NoAsserts) : m_min(min), m_max(max) {}

  public:
	static_assert(isVector<T>(), "Box<> has to be based on a vector");

	using Scalar = typename T::Scalar;
	using Vector = T;
	using Vector2 = vector2<Scalar>;
	using Point = Vector;

	enum { dim_size = Vector::vector_size, num_corners = 1 << dim_size };

	// min <= max in all dimensions; can be empty
	bool validRange(const Point &min, const Point &max) const {
		for(int i = 0; i < dim_size; i++)
			if(!(min[i] <= max[i]))
				return false;
		return true;
	}

	// min >= max in any dimension
	bool emptyRange(const Point &min, const Point &max) const {
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
	explicit Box(Vector size) : Box(Vector(), size) {}
	Box() : m_min(), m_max() {}

	template <class TVector>
	explicit Box(const Box<TVector> &irect) : Box(Vector(irect.min()), Vector(irect.max())) {}

	Scalar min(int i) const { return m_min[i]; }
	Scalar max(int i) const { return m_max[i]; }

	const Point &min() const { return m_min; }
	const Point &max() const { return m_max; }

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
	Vector size() const { return m_max - m_min; }
	Point center() const { return (m_max + m_min) / Scalar(2); }

	Box operator+(const Vector &offset) const { return Box(m_min + offset, m_max + offset); }
	Box operator-(const Vector &offset) const { return Box(m_min - offset, m_max - offset); }
	Box operator*(const Vector &scale) const {
		Vector tmin = m_min * scale, tmax = m_max * scale;
		for(int n = 0; n < dim_size; n++)
			if(scale[n] < Scalar(0))
				swap(tmin[n], tmax[n]);
		return {tmin, tmax, NoAsserts()};
	}

	Box operator*(Scalar scale) const {
		auto tmin = m_min * scale, tmax = m_max * scale;
		if(scale < Scalar(0))
			swap(tmin, tmax);
		return {tmin, tmax, NoAsserts()};
	}

	bool empty() const { return emptyRange(m_min, m_max); }

	bool contains(const Point &point) const {
		for(int i = 0; i < dim_size; i++)
			if(!(point[i] >= m_min[i] && point[i] <= m_max[i]))
				return false;
		return true;
	}

	bool contains(const Box &box) const { return box == intersection(box); }

	bool containsPixel(const T &pos) const {
		for(int i = 0; i < dim_size; i++)
			if(!(pos[i] >= m_min[i] && pos[i] + Scalar(1) <= m_max[i]))
				return false;
		return true;
	}

	Scalar pixelCount(int axis) const { return max(size(axis) - Scalar(1), Scalar(0)); }
	T pixelCount() const { return vmax(size() - Vector(Scalar(1)), T(Scalar(0))); }

	ENABLE_IF_SIZE(2) array<Point, 4> corners() const {
		return {{m_min, {m_min[0], m_max[1]}, m_max, {m_max[0], m_min[1]}}};
	}

	ENABLE_IF_SIZE(3) array<Point, num_corners> corners() const {
		array<Vector, num_corners> out;
		for(int n = 0; n < num_corners; n++)
			for(int i = 0; i < dim_size; i++) {
				int bit = 1 << (dim_size - i - 1);
				out[n][i] = (n & bit ? m_min : m_max)[i];
			}
		return out;
	}

	Box intersectionOrEmpty(const Box &rhs) const {
		auto tmin = vmax(m_min, rhs.m_min);
		auto tmax = vmin(m_max, rhs.m_max);

		if(!emptyRange(tmin, tmax))
			return Box{tmin, tmax, NoAsserts()};
		return Box{};
	}

	// When Boxes touch, returned Box will be empty
	Maybe<Box> intersection(const Box &rhs) const {
		auto tmin = vmax(m_min, rhs.m_min);
		auto tmax = vmin(m_max, rhs.m_max);

		if(!validRange(tmin, tmax))
			return none;
		return Box{tmin, tmax, NoAsserts()};
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

	Box inset(const Vector &val_min, const Vector &val_max) const {
		auto new_min = m_min + val_min, new_max = m_max - val_max;
		return {vmin(new_min, new_max), vmax(new_min, new_max), NoAsserts()};
	}
	Box inset(const Vector &value) const { return inset(value, value); }
	Box inset(Scalar value) const { return inset(Vector(value)); }

	Box enlarge(const Vector &val_min, const Vector &val_max) const {
		return inset(-val_min, -val_max);
	}
	Box enlarge(const Vector &value) const { return inset(-value); }
	Box enlarge(Scalar value) const { return inset(Vector(-value)); }

	FWK_ORDER_BY(Box, m_min, m_max);

	ENABLE_IF_SIZE(3) Box<Vector2> xz() const { return {m_min.xz(), m_max.xz()}; }
	ENABLE_IF_SIZE(3) Box<Vector2> xy() const { return {m_min.xy(), m_max.xy()}; }
	ENABLE_IF_SIZE(3) Box<Vector2> yz() const { return {m_min.yz(), m_max.yz()}; }

	class Iter2D {
	  public:
		Iter2D(T pos, Scalar begin_x, Scalar end_x)
			: m_pos(pos), m_begin_x(begin_x), m_end_x(end_x) {}
		auto operator*() const { return m_pos; }
		const Iter2D &operator++() {
			m_pos[0]++;
			if(m_pos[0] >= m_end_x) {
				m_pos[0] = m_begin_x;
				m_pos[1]++;
			}
			return *this;
		}

		FWK_ORDER_BY(Iter2D, m_pos.y, m_pos.x);

	  private:
		T m_pos;
		Scalar m_begin_x, m_end_x;
	};

	// TODO: were acutally iterating over pixels here...
	// how to make it more clear ?
	template <class U = Vector, EnableIf<isIntegralVector<U, 2>()>...> Iter2D begin() const {
		return {m_min, m_min[0], m_max[0]};
	}
	template <class U = Vector, EnableIf<isIntegralVector<U, 2>()>...> Iter2D end() const {
		return {T(m_min[0], m_max[1]), m_min[0], m_max[0]};
	}

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

template <class TRange, class T = RemoveConst<RangeBase<TRange>>, EnableIfVector<T>...>
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

template <class T, EnableIfRealVector<T>...> auto encloseIntegral(const Box<T> &box) {
	using IVec = MakeVector<int, T::vector_size>;
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
	if constexpr(isMathObject<T>() && !isVector<T>()) {
		return isSame<decltype(enclose(std::declval<T>())), Box<typename T::Vector>>();
	} else {
		return false;
	}
}

template <class TRange, class T = RangeBase<TRange>, EnableIf<isEnclosable<T>()>...>
auto enclose(const TRange &objects) {
	using Box = decltype(enclose(std::declval<T>()));
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
