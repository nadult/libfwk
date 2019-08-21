// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/geom_base.h"
#include "fwk/index_range.h"
#include "fwk/math/constants.h"
#include "fwk/math/segment.h"
#include "fwk/sys/assert.h"

namespace fwk {

#define ENABLE_IF_SIZE(n) template <class U = Vec, EnableInDimension<U, n>...>

// TODO: non-continuous contours as well ?
// TODO: VertexId, EdgeId; interface similar to PGraph
// TODO: does it even make sense to use VertexId & EdgeId here ?
//       if we extract contour from graph, then indices break...; unless we make
//       a special kind of class which keeps indices instead of points?
// TODO: Contour as a selection of edges from PlaneGraph (directions would matter though...)
// nodes(points) can be duplicated
template <class T> class Contour {
  public:
	static_assert(is_fpt<Base<T>> && is_vec<T>);
	static constexpr int dim_size = T::vec_size;

	using Vec = T;
	using Point = Vec;
	using Scalar = typename T::Scalar;
	using Segment = fwk::Segment<T>;

	// TODO: pair<EdgeId, Scalar> ?
	using TrackPos = pair<int, Scalar>;

	// TODO: pair Segment::IsectParam, int ?
	using IsectParam = Variant<None, TrackPos, pair<TrackPos, TrackPos>>;
	using Isect = typename Segment::Isect;

	// Edges have to be properly ordered (and actually create a continuous contour)
	Contour(CSpan<Segment>, bool flip_tangents = false);
	Contour(CSpan<T>, bool is_looped, bool flip_tangents = false);
	Contour(const Contour &);
	Contour(Contour &&) = default;
	~Contour();

	Contour &operator=(const Contour &);
	Contour &operator=(Contour &&) = default;

	bool valid(VertexId id) const { return id >= 0 && id < m_points.size(); }
	bool valid(EdgeId id) const { return id >= 0 && id < m_upto_length.size(); }

	const T &operator[](VertexId node_id) const {
		DASSERT_EX(valid(node_id), node_id);
		return m_points[node_id];
	}
	Segment operator[](EdgeId edge_id) const;
	// TODO: remove it ?
	Segment edge(int edge_id) const { return (*this)[EdgeId(edge_id)]; }

	// some id-s may be invalid if contour is not looped
	pair<EdgeId, EdgeId> adjacentEdges(VertexId) const;

	CSpan<T> points() const { return m_points; }
	auto segments() const {
		const auto *ptr = this;
		return indexRange(0, numEdges(), [ptr](int idx) { return (*ptr)[EdgeId(idx)]; });
	}
	// returns positive value if on positive side
	Scalar edgeSide(EdgeId, T point) const;

	// TODO: segments or edges? points or nodes?
	int numEdges() const { return m_upto_length.size(); }
	int numPoints() const { return m_points.size(); }
	int numNodes() const { return numPoints(); }

	Scalar edgeLength(int edge_id) const;

	Scalar clampPos(Scalar) const;
	Scalar wrapPos(Scalar) const;

	bool valid(TrackPos) const;
	// pos: 0 to length
	TrackPos trackPos(Scalar pos) const;
	Scalar linearPos(TrackPos) const;

	Point point(TrackPos) const;
	Vec tangent(TrackPos) const;
	Vec tangent(EdgeId) const;

	Point point(Scalar lpos) const { return point(trackPos(lpos)); }
	Vec tangent(Scalar lpos) const { return tangent(trackPos(lpos)); }

	Segment segmentAt(Scalar) const;
	Segment segmentAt(TrackPos) const;

	Scalar length() const { return m_length; }
	bool isLooped() const { return m_is_looped; }
	bool isPoint() const { return numEdges() == 0; }
	bool flippedTangents() const { return m_flip_tangents; }

	TrackPos closestPos(Point) const;

	ENABLE_IF_SIZE(2) vector<IsectParam> isectParam(const Segment &) const;
	Isect at(IsectParam) const;

	vector<int> findProtrudingPoints() const;
	vector<T> genEvenPoints(Scalar distance) const;
	vector<T> findClosestPoints(vector<T>) const;

	// Smoothed angles for each point; They can be easily interpolated;
	// There are two angles for first point: one at the beginning and another one at the end
	// niceAngles(contour).size() == contour.size() + 1
	ENABLE_IF_SIZE(2) vector<Scalar> niceAngles() const;

	Contour simplify(Scalar theta = 0.999, Scalar max_err = 0.01, Scalar max_distance = inf) const;
	Contour reverse(bool flip_tangents = true) const;
	Contour cubicInterpolate(Scalar step) const;

  private:
	void computeLengths();
	friend class SubContourRef<T>;

	vector<T> m_points;
	vector<Scalar> m_upto_length;
	Scalar m_length;
	bool m_is_looped = false;
	bool m_flip_tangents = false;
};

#undef ENABLE_IF_SIZE

template <class T> class SubContourRef {
  public:
	using Contour = fwk::Contour<T>;
	using Scalar = typename Contour::Scalar;
	using TrackPos = typename Contour::TrackPos;

	// passed Contour has to exist as long as SubContour exists
	SubContourRef(const Contour &contour_ref, TrackPos from, TrackPos to);
	SubContourRef(const Contour &contour_ref, Scalar from, Scalar to);

	Scalar length() const { return m_length; }
	TrackPos trackPos(Scalar pos) const;

	bool isLooped() const { return false; }
	bool empty() const { return m_from == m_to; }
	bool validPos(Scalar pos) const { return pos >= 0.0f && pos < m_length; }

	auto from() const { return m_from; }
	auto to() const { return m_to; }
	const auto &baseRef() const { return m_contour; }

	operator Contour() const;

  private:
	const Contour &m_contour;
	Scalar m_from, m_to;
	Scalar m_length;
	bool m_is_inversed;
};

template <class T> bool isContinuousContour(CSpan<Segment<T>> edges);
}
