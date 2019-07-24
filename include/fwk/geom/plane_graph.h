// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/dynamic.h"
#include "fwk/enum_flags.h"
#include "fwk/geom/immutable_graph.h"
#include "fwk/math/segment.h"
#include "fwk/small_vector.h"

namespace fwk {

using IsectFlags = EnumFlags<IsectClass>;

// TODO: what is it doing here ?
template <class R, EnableIfFpt<R>...>
Maybe<Segment2<R>> clipSegment(Segment2<R>, const Box<vec2<R>> &);

// Obsługa 3D czy nie ?
// - operacje wykonywane na xy(), xy() czy xz() ?
// - kontury by sie pewnie przydały, PG też; ale czy nie da się tego
//   samego rozwiązać za pomocą labelek?
//
// Jak chciałbym, żeby to wyglądało z góry ?

DEFINE_ENUM(PlaneGraphOpt, ccw_edge_order, edge_twins, vectors, segment_grid);

// ImmutableGraph fused with Vector2;
// Edges are additionally ordered by CCW order;
// Extened info is always computed
// At some position P there can be only one Node
//
// TODO: support dla grafów 3D (z obsługa warstw)?
// TODO: tylko jeśli miałby obsługiwać 3D, to musielibyśmy zmienić nazwę...
// TODO: chyba lepiej by było odseparować algorytmy od konkretnej reprezentacji?
// jak dodamy tę opcję to zmienimy też nazwę, tylko na co?
template <class T> class PlaneGraph : public ImmutableGraph {
  public:
	static_assert(is_vec<T, 2>, "");
	using Vec = T;
	using Point = Vec;
	using Scalar = fwk::Scalar<T>;
	using Segment = fwk::Segment<T>;
	using Rect = fwk::Box<Vec>;

	using Grid = SegmentGrid<T>;
	using IsectParam = pair<fwk::IsectParam<Scalar>, EdgeId>;
	using Isect = Variant<None, T, Segment>;
	using Builder = PGraphBuilder<T>;

#define ENABLE_IF_REAL template <class U = T, EnableIfFptVec<U>...>

	static void verify(const ImmutableGraph &, CSpan<T> points);

	template <class U, EnableIf<precise_conversion<U, T>>...>
	PlaneGraph(const PlaneGraph<U> &other)
		: PlaneGraph(ImmutableGraph(other), fwk::transform<T>(other.points())) {}

	PlaneGraph(CSpan<Pair<VertexId>> edges, vector<T> points);
	PlaneGraph(ImmutableGraph, vector<T> points);
	PlaneGraph();
	PlaneGraph(const PlaneGraph &);
	PlaneGraph(PlaneGraph &&);
	~PlaneGraph();

	PlaneGraph &operator=(const PlaneGraph &);
	PlaneGraph &operator=(PlaneGraph &&);

	// Tworzenie grida podczas inicjalizacji?
	// - tworzenie na rządanie
	// - funkcje biorące grida jako parametr ?
	// Czasami przekształcamy grafy wiele razy, i nie ma sensu za każdym razem
	// tworzyć grida

	void orderEdges();
	Grid makeGrid() const;
	void addGrid();
	void setGrid(Grid);

	bool hasGrid() const { return !!m_grid; }

	const Grid &grid() const {
		DASSERT(hasGrid());
		return *m_grid.get();
	}

	struct PointTransform {
		vector<Point> new_points;
		vector<VertexId> mapping;
	};

	void apply(PointTransform, bool remove_collapsed_edges);
	ENABLE_IF_REAL static PlaneGraph fromContours(CSpan<Contour<T>>);
	ENABLE_IF_REAL static PlaneGraph fromSegments(CSpan<Segment>);

	const ImmutableGraph &graph() const { return *this; }
	CSpan<T> points() const { return m_points; }

	const T &operator[](VertexId vid) const {
		DASSERT(valid(vid));
		return m_points[vid];
	}

	Segment operator[](EdgeId edge_id) const {
		DASSERT(valid(edge_id));
		auto eref = ref(edge_id);
		return {m_points[eref.from()], m_points[eref.to()]};
	}
	Segment operator[](const Pair<VertexId> &seg) const {
		DASSERT(valid(seg.first) && valid(seg.second));
		return {m_points[seg.first], m_points[seg.second]};
	}

	vector<Segment> segments() const {
		auto &ref = *this;
		return fwk::transform(edgeIds(), [&](auto id) { return ref[id]; });
	}

	vector<vector<EdgeId>> edgeLoops() const;
	ENABLE_IF_REAL vector<Contour<T>> contours() const;
	ENABLE_IF_REAL vector<Contour<T>> contourLoops() const;
	ENABLE_IF_REAL PlaneGraph simplify(Scalar theta = 0.999, Scalar max_err = 0.01,
									   Scalar max_dist = inf) const;
	ENABLE_IF_REAL PlaneGraph splitEdges(Scalar max_length) const;
	ENABLE_IF_REAL vector<T> evenPoints(Scalar dist) const;
	ENABLE_IF_REAL PointTransform joinNearby(Scalar join_dist) const;

	// TODO: what should we do with duplicates ?
	static PlaneGraph merge(CSpan<PlaneGraph>, vector<Pair<VertexId>> *vert_intervals = nullptr);

	// This is not really useful
	template <class Func> auto transform(const Func &func) const {
		using TOut = decltype(func(T()));
		static_assert(is_vec<TOut, 2>, "");

		PGraphBuilder<TOut> builder;
		for(auto tpoint : fwk::transform(m_points, func)) {
			if(builder.find(tpoint))
				FWK_FATAL("Degenerate case detected"); // TODO: Expected<>
			builder(tpoint);
		}
		return PlaneGraph<TOut>{*this, builder.extractPoints()};
	}

	static pair<ImmutableGraph, vector<T>> decompose(PlaneGraph sink);

	vector<EdgeId> findIntersectors(const Grid &) const;
	vector<Variant<VertexId, EdgeId>> findIntersectors(const Grid &, double min_dist) const;
	SmallVector<EdgeId> isectEdges(const Segment &, IsectFlags = ~IsectClass::none) const;
	bool isectAnyEdge(const Grid &, const Segment &, IsectFlags = ~IsectClass::none) const;

	ENABLE_IF_REAL Maybe<VertexId> closestVertex(const Point &, Scalar max_dist = inf) const;

	bool isPlanar(const Grid &) const;
	bool isPlanar() const;
	Expected<void> checkPlanar() const;
	Expected<void> checkPlanar(double min_dist) const;

	ENABLE_IF_REAL vector<Vec> randomPoints(Random &, Scalar min_distance,
											Maybe<Rect> = none) const;

	FWK_ORDER_BY_DECL(PlaneGraph, (const ImmutableGraph &)*this, m_points);

  private:
	ENABLE_IF_REAL static void instantiator();

	vector<T> m_points;
	Dynamic<SegmentGrid<T>> m_grid;
};

// CCW order starting from zero_vector
template <class T, EnableIfVec<T, 2>...>
void orderByDirection(Span<int> indices, CSpan<T> vectors, const T &zero_vector);

double bestIntegralScale(CSpan<double3>, int max_value = 1024 * 1024 * 16);
double bestIntegralScale(CSpan<float3>, int max_value = 1024 * 1024 * 16);
double bestIntegralScale(CSpan<double2>, int max_value = 1024 * 1024 * 16);
double bestIntegralScale(CSpan<float2>, int max_value = 1024 * 1024 * 16);

template <class IT, class T, EnableIfFptVec<T, 2>..., EnableIfIntegralVec<IT, 2>...>
Maybe<vector<IT>> tryToIntegral(CSpan<T>, double scale);

template <class IT, class T, EnableIfFptVec<T, 2>..., EnableIfIntegralVec<IT, 2>...>
PGraph<IT> toIntegral(const PGraph<T> &, double scale, bool remove_collapsed_edges = true);
}
