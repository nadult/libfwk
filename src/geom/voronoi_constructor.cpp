// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom/voronoi.h"

#include "fwk/geom/geom_graph.h"
#include "fwk/geom/segment_grid.h"
#include "fwk/geom/wide_int.h"
#include "fwk/math/segment.h"
#include "fwk/vector_map.h"

#include "../extern/boost_polygon/voronoi_builder.hpp"
#include "../extern/boost_polygon/voronoi_diagram.hpp"

using namespace boost::polygon;
using namespace boost::polygon::detail;

namespace fwk {

static constexpr auto site_layer = VoronoiDiagram::site_layer,
					  seg_layer = VoronoiDiagram::seg_layer, arc_layer = VoronoiDiagram::arc_layer;

namespace {
	struct type_converter_efpt2 {
		template <int N> extended_exponent_fpt<fpt64> operator()(const WideInt<N> &that) const {
			auto p = that.p();
			return {p.first, p.second};
		}
	};

	struct type_converter_fpt2 {
		template <typename T> fpt64 operator()(const T &that) const { return fpt64(that); }

		template <int N> fpt64 operator()(const WideInt<N> &that) const { return that.d(); }
		fpt64 operator()(const extended_exponent_fpt<fpt64> &that) const { return that.d(); }
	};

	template <int wide_int_size> struct custom_traits {
		typedef int32 int_type;
		typedef int64 int_x2_type;
		typedef uint64 uint_x2_type;
		typedef WideInt<wide_int_size> big_int_type;
		typedef fpt64 fpt_type;
		typedef extended_exponent_fpt<fpt_type> efpt_type;
		typedef ulp_comparison<fpt_type> ulp_cmp_type;
		typedef type_converter_fpt2 to_fpt_converter_type;
		typedef type_converter_efpt2 to_efpt_converter_type;
	};
}

class DelaunayConstructor {
  public:
	using CT = double;
	using Traits = voronoi_diagram_traits<CT>;

	DelaunayConstructor(CSpan<int2> sites) {
		voronoi_builder<int, custom_traits<4>> builder;
		for(auto &pt : sites)
			builder.insert_point(double(pt.x), double(pt.y));
		builder.construct(this);
	}

	vector<Pair<VertexId>> extractSitePairs() { return move(m_site_pairs); }

	void clear() { m_site_pairs.clear(); }
	void _reserve(std::size_t num_sites) { m_site_pairs.reserve(num_sites * 3); }

	template <typename CT> void _process_single_site(const site_event<CT> &site) {}

	template <typename CT>
	pair<void *, void *> _insert_new_edge(const site_event<CT> &site1,
										  const site_event<CT> &site2) {
		m_site_pairs.emplace_back((VertexId)site1.initial_index(), (VertexId)site2.initial_index());
		return {nullptr, nullptr};
	}

	// TODO: maybe there is no need to compute vertex position ?
	template <typename CT1, typename CT2>
	pair<void *, void *>
	_insert_new_edge(const site_event<CT1> &site1, const site_event<CT1> &site3,
					 const circle_event<CT2> &circle, void *data12, void *data23) {
		m_site_pairs.emplace_back((VertexId)site1.initial_index(), (VertexId)site3.initial_index());
		return {nullptr, nullptr};
	}

	void _build() {}

  private:
	vector<Pair<VertexId>> m_site_pairs;
};

// Source: example code for visualizing boost voronoi diagrams
class VoronoiConstructor {
  public:
	using CT = double;
	using PT = double2;
	using ST = Segment2D;
	using VD = voronoi_diagram<CT>;
	using cell_type = VD::cell_type;
	using edge_type = VD::edge_type;

	VoronoiConstructor(const GeomGraph<int2> &graph, IRect rect)
		: m_input_graph(graph), m_rect(rect) {
		// TODO: transformation is not needed?
		for(auto nref : graph.verts())
			if(!nref.numEdges()) {
				m_points.emplace_back((PT)graph(nref));
				m_point_ids.emplace_back(nref.id());
			}

		for(auto nedge : graph.edges()) {
			double2 p1(graph(nedge.from())), p2(graph(nedge.to()));
			if(nedge.from() == nedge.to()) {
				m_points.emplace_back(p1.x, p1.y);
				m_point_ids.emplace_back(nedge.from().id());
			} else {
				m_segments.emplace_back(p1, p2);
				m_segment_ids.emplace_back(nedge.from().id(), nedge.to().id());
			}
		}

		voronoi_builder<int, custom_traits<32>> builder;
		for(auto &pt : m_points)
			builder.insert_point(pt.x, pt.y);
		for(auto &seg : m_segments)
			builder.insert_segment(seg.from.x, seg.from.y, seg.to.x, seg.to.y);
		builder.construct(&m_diagram);
	}

	VoronoiDiagram convertDiagram() {
		GeomGraph<double2> out;

		VoronoiInfo info;
		info.cells.reserve(m_diagram.num_cells());

		for(int i : intRange(m_diagram.num_cells())) {
			auto &cell = m_diagram.cells()[i];
			CellId id(i);

			int point_index = cell.source_index();
			int seg_index = cell.source_index() - m_points.size();
			Simplex simplex;

			if(cell.source_category() == SOURCE_CATEGORY_SEGMENT_START_POINT)
				simplex = {m_segment_ids[seg_index].first};
			else if(cell.source_category() == SOURCE_CATEGORY_SEGMENT_END_POINT)
				simplex = {m_segment_ids[seg_index].second};
			else if(cell.source_category() == SOURCE_CATEGORY_SINGLE_POINT)
				simplex = {m_point_ids[point_index]};
			else if(cell.contains_segment())
				simplex = {m_segment_ids[seg_index].first, m_segment_ids[seg_index].second};

			if(!simplex.empty())
				info.cells.emplace_back(simplex, cell.source_category());
		}

		// Copying input graph into site layer
		out.reserveVerts(m_diagram.num_edges() / 2 + m_input_graph.numVerts() + 16);
		for(auto vref : m_input_graph.verts(site_layer))
			out.addVertexAt(vref, m_input_graph(vref), site_layer);
		for(auto eref : m_input_graph.edges(site_layer))
			out.addEdgeAt(eref, eref.from(), eref.to(), site_layer);

		out.reserveEdges(m_diagram.num_edges() * 3 + m_input_graph.numEdges()); // TODO: check this

		vector<double2> points;
		vector<VertexId> nodes;
		vector<CT> stack;

		auto vert_equal = [](const double2 &v1, const double2 &v2) {
			using Traits = voronoi_diagram_traits<CT>;
			enum { ULPS = Traits::vertex_equality_predicate_type::ULPS };
			ulp_comparison<CT> ulp_cmp;

			return (ulp_cmp(v1.x, v2.x, ULPS) == ulp_comparison<CT>::EQUAL) &&
				   (ulp_cmp(v1.y, v2.y, ULPS) == ulp_comparison<CT>::EQUAL);
		};

		HashMap<Pair<VertexId>, Pair<EdgeId>> shared_node_arcs;
		shared_node_arcs.reserve(m_input_graph.numEdges() * 2);

		auto *first_cell = &m_diagram.cells()[0];
		for(int n = 0, ecount = int(m_diagram.num_edges()); n < ecount; n += 2) {
			auto &edge = m_diagram.edges()[n];

			CellId cell_id1(edge.cell() - first_cell);
			CellId cell_id2(edge.twin()->cell() - first_cell);

			const auto &cell1 = info.cells[cell_id1];
			const auto &cell2 = info.cells[cell_id2];

			Maybe<VertexId> shared_node;
			for(auto v1 : cell1.generator)
				for(auto v2 : cell2.generator)
					if(v1 == v2 && m_input_graph.ref(v1).numEdges() != 1) {
						shared_node = v1;
						break;
					}

			points.clear();
			nodes.clear();

			if(!edge.is_finite()) {
				clip_infinite_edge(edge, points);
			} else {
				// TODO: segment points should be after arc points in info.m_points
				points.clear();
				points.emplace_back(edge.vertex0()->x(), edge.vertex0()->y());
				points.emplace_back(edge.vertex1()->x(), edge.vertex1()->y());
				if(edge.is_curved()) {
					CT max_dist = 0.0001 * m_rect.width(); // TODO: better approximation
					DASSERT(!isNan(max_dist));

					PT point = edge.cell()->contains_point() ? getPoint(*edge.cell())
															 : getPoint(*edge.twin()->cell());
					auto segment = edge.cell()->contains_point() ? getSegment(*edge.twin()->cell())
																 : getSegment(*edge.cell());

					discretize(point, segment, max_dist, points, stack);
				}
			}

			if(shared_node) {
				auto shared_point = m_input_graph(*shared_node);
				// Making sure that vertices which lie on cells are exactly the same
				if(vert_equal(points.front(), shared_point))
					points.front() = shared_point;
				if(vert_equal(points.back(), shared_point))
					points.back() = shared_point;
			}

			VertexId v1 = out.fixVertex(points.front(), arc_layer).id;
			VertexId v2 = out.fixVertex(points.back(), arc_layer).id;

			if(shared_node) {
				auto it = shared_node_arcs.find({v1, v2});
				if(it == shared_node_arcs.end())
					it = shared_node_arcs.find({v2, v1});

				if(it != shared_node_arcs.end()) {
					auto &tarc1_cell = out[it->second.first].ival1;
					auto &tarc2_cell = out[it->second.second].ival1;

					// We have two sites s1, s2 sharing a vertex v1
					// We have two identical arcs, one between s1 and v1, other between v1 and s2
					// We're merging them together into single arc between s1 and s2
					if(isOneOf(cell_id1, tarc1_cell, tarc2_cell)) {
						(int(cell_id1) == tarc1_cell ? tarc1_cell : tarc2_cell) = cell_id2;
						continue;
					}
				}
			}

			auto arc_id1 = out.addEdge(v1, v2, arc_layer);
			auto arc_id2 = out.addEdge(v2, v1, arc_layer);

			if(shared_node)
				shared_node_arcs.emplace({v1, v2}, {arc_id1, arc_id2});

			// TODO: wtf is this ?
			bool is_primary = edge.is_primary();
			bool touches_site = !is_primary || shared_node;
			is_primary = !touches_site; // 1 bool is enough ?

			out[arc_id1].ival1 = is_primary; // TODO: cell type
			out[arc_id1].ival2 = cell_id1;

			out[arc_id2].ival1 = is_primary;
			out[arc_id2].ival2 = cell_id2;

			for(int n = 1; n < points.size(); n++) {
				auto v1 = out.fixVertex(points[n - 1], seg_layer).id;
				auto v2 = out.fixVertex(points[n], seg_layer).id;

				auto eid1 = out.addEdge(v1, v2, seg_layer);
				auto eid2 = out.addEdge(v2, v1, seg_layer);

				out[eid1].ival1 = arc_id1;
				out[eid1].ival2 = cell_id1;
				out[eid2].ival1 = arc_id2;
				out[eid2].ival2 = cell_id2;
			}
		}

		return VoronoiDiagram(out, move(info));
	}

	void clip_infinite_edge(const edge_type &edge, vector<double2> &out) const {
		const auto &cell1 = *edge.cell();
		const auto &cell2 = *edge.twin()->cell();
		PT origin, direction;
		// Infinite edges could not be created by two segment sites.
		if(cell1.contains_point() && cell2.contains_point()) {
			PT p1 = getPoint(cell1);
			PT p2 = getPoint(cell2);
			origin.x = (p1.x + p2.x) * 0.5;
			origin.y = (p1.y + p2.y) * 0.5;
			direction.x = p1.y - p2.y;
			direction.y = p2.x - p1.x;
		} else {
			origin = cell1.contains_segment() ? getPoint(cell2) : getPoint(cell1);
			auto segment = cell1.contains_segment() ? getSegment(cell1) : getSegment(cell2);
			CT dx = segment.to.x - segment.from.x;
			CT dy = segment.to.y - segment.from.y;
			if((segment.from == origin) != cell1.contains_point()) {
				direction.x = dy;
				direction.y = -dx;
			} else {
				direction.x = -dy;
				direction.y = dx;
			}
		}
		CT side = m_rect.width();
		CT koef = side / max(fabs(direction.x), fabs(direction.y));
		if(!edge.vertex0()) {
			out.emplace_back(origin.x - direction.x * koef, origin.y - direction.y * koef);
		} else {
			out.emplace_back(edge.vertex0()->x(), edge.vertex0()->y());
		}
		if(!edge.vertex1()) {
			out.emplace_back(origin.x + direction.x * koef, origin.y + direction.y * koef);
		} else {
			out.emplace_back(edge.vertex1()->x(), edge.vertex1()->y());
		}
	}

	static void discretize(const double2 &point, const ST &segment, const CT max_dist,
						   vector<PT> &out, vector<CT> &point_stack) {
		// Apply the linear transformation to move start point of the segment to
		// the point with coordinates (0, 0) and the direction of the segment to
		// coincide the positive direction of the x-axis.
		CT segm_vec_x = cast(segment.to.x) - cast(segment.from.x);
		CT segm_vec_y = cast(segment.to.y) - cast(segment.from.y);
		CT sqr_segment_length = segm_vec_x * segm_vec_x + segm_vec_y * segm_vec_y;
		if(segment.from == segment.to)
			return;

		// Compute x-coordinates of the endpoints of the edge
		// in the transformed space.
		CT projection_start = sqr_segment_length * get_point_projection(out[0], segment);
		CT projection_end = sqr_segment_length * get_point_projection(out[1], segment);

		// Compute parabola parameters in the transformed space.
		// Parabola has next representation:
		// f(x) = ((x-rot_x)^2 + rot_y^2) / (2.0*rot_y).
		CT point_vec_x = cast(point.x) - cast(segment.from.x);
		CT point_vec_y = cast(point.y) - cast(segment.from.y);
		CT rot_x = segm_vec_x * point_vec_x + segm_vec_y * point_vec_y;
		CT rot_y = segm_vec_x * point_vec_y - segm_vec_y * point_vec_x;

		DASSERT(!isNan(segm_vec_x) && !isNan(segm_vec_y));
		DASSERT(!isNan(projection_start) && !isNan(projection_end));
		DASSERT(!isNan(point_vec_x) && !isNan(point_vec_y));
		DASSERT(!isNan(rot_x) && !isNan(rot_y));

		// Save the last point.
		PT last_point = out[1];
		out.pop_back();

		// Use stack to avoid recursion.
		point_stack.clear();
		point_stack.push_back(projection_end);
		CT cur_x = projection_start;
		CT cur_y = parabola_y(cur_x, rot_x, rot_y);

		// Adjust max_dist parameter in the transformed space.
		const CT max_dist_transformed = max_dist * max_dist * sqr_segment_length;
		while(!point_stack.empty()) {
			CT new_x = point_stack.back();
			CT new_y = parabola_y(new_x, rot_x, rot_y);

			// Compute coordinates of the point of the parabola that is
			// furthest from the current line segment.
			CT mid_x = (new_y - cur_y) / (new_x - cur_x) * rot_y + rot_x;
			CT mid_y = parabola_y(mid_x, rot_x, rot_y);

			// Compute maximum distance between the given parabolic arc
			// and line segment that discretize it.
			CT dist = (new_y - cur_y) * (mid_x - cur_x) - (new_x - cur_x) * (mid_y - cur_y);
			dist = dist * dist /
				   ((new_y - cur_y) * (new_y - cur_y) + (new_x - cur_x) * (new_x - cur_x));

			DASSERT(!isNan(new_x) && !isNan(new_y));

			if(dist <= max_dist_transformed) {
				// Distance between parabola and line segment is less than max_dist.
				point_stack.pop_back();
				CT inter_x = (segm_vec_x * new_x - segm_vec_y * new_y) / sqr_segment_length +
							 cast(segment.from.x);
				CT inter_y = (segm_vec_x * new_y + segm_vec_y * new_x) / sqr_segment_length +
							 cast(segment.from.y);
				out.push_back(PT(inter_x, inter_y));
				cur_x = new_x;
				cur_y = new_y;
			} else {
				point_stack.push_back(mid_x);
			}
		}

		// Update last point.
		out.back() = last_point;
	}

  private:
	// Compute y(x) = ((x - a) * (x - a) + b * b) / (2 * b).
	static CT parabola_y(CT x, CT a, CT b) { return ((x - a) * (x - a) + b * b) / (b + b); }

	// Get normalized length of the distance between:
	//   1) point projection onto the segment
	//   2) start point of the segment
	// Return this length divided by the segment length. This is made to avoid
	// sqrt computation during transformation from the initial space to the
	// transformed one and vice versa. The assumption is made that projection of
	// the point lies between the start-point and endpoint of the segment.
	template <class InCT, template <class> class Point, template <class> class Segment>
	static CT get_point_projection(const Point<CT> &point, const Segment<InCT> &segment) {
		CT segment_vec_x = cast(segment.to.x) - cast(segment.from.x);
		CT segment_vec_y = cast(segment.to.y) - cast(segment.from.y);
		CT point_vec_x = point.x - cast(segment.from.x);
		CT point_vec_y = point.y - cast(segment.from.y);
		CT sqr_segment_length = segment_vec_x * segment_vec_x + segment_vec_y * segment_vec_y;
		CT vec_dot = segment_vec_x * point_vec_x + segment_vec_y * point_vec_y;
		return vec_dot / sqr_segment_length;
	}

	template <typename InCT> static CT cast(const InCT &value) { return static_cast<CT>(value); }

	PT getPoint(const cell_type &cell) const {
		auto index = cell.source_index();
		auto category = cell.source_category();
		if(category == SOURCE_CATEGORY_SINGLE_POINT)
			return m_points[index];
		index -= m_points.size();

		if(category == SOURCE_CATEGORY_SEGMENT_START_POINT)
			return m_segments[index].from;
		else
			return m_segments[index].to;
	}

	const ST &getSegment(const cell_type &cell) const {
		return m_segments[cell.source_index() - m_points.size()];
	}

	const GeomGraph<int2> &m_input_graph;
	vector<PT> m_points;
	vector<VertexId> m_point_ids;

	vector<ST> m_segments;
	vector<Pair<VertexId>> m_segment_ids;

	DRect m_rect;
	VD m_diagram;
};

vector<Pair<VertexId>> VoronoiDiagram::delaunay(CSpan<int2> sites) {
	DelaunayConstructor constructor(sites);
	return constructor.extractSitePairs();
}

Ex<VoronoiDiagram> VoronoiDiagram::construct(const GeomGraph<int2> &graph) {
	EXPECT(graph.checkPlanar(graph.makeGrid()));

	IRect rect = enclose(graph.points()).enlarge(1);
	// TODO: how can we be sure that rect can be enlarged ?
	VoronoiConstructor constructor(graph, rect);
	return constructor.convertDiagram();
}
}
