// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom/voronoi.h"

#include "fwk/geom/geom_graph.h"
#include "fwk/geom/plane_graph.h"
#include "fwk/geom/plane_graph_builder.h"
#include "fwk/hash_map.h"
#include "fwk/math/box.h"
#include "fwk/math/rotation.h"
#include "fwk/sys/assert.h"
#include "fwk/variant.h"
#include "fwk/vector_map.h"

namespace fwk {

VoronoiDiagram::VoronoiDiagram(Graph arc_segs, Graph arcs, Info info)
	: m_arc_graph(arcs), m_segments_graph(arc_segs), m_info(move(info)) {
	DASSERT_EQ(m_info.points.size(), m_arc_graph.numVerts());
	DASSERT_EQ(m_info.points.size(), m_segments_graph.numVerts());

	DASSERT_EQ(m_info.segments.size(), m_segments_graph.numEdges());
	DASSERT_EQ(m_info.arcs.size(), m_arc_graph.numEdges());

	for(auto &segment : m_info.segments)
		DASSERT(valid(segment.arc));
	for(auto &cell : m_info.cells) {
		for(int n = 0; n < cell.generator.size(); n++)
			DASSERT(valid(cell.generator[n]));
	}
	// TODO: verify arcs
}

vector<vector<ArcSegmentId>> VoronoiDiagram::arcToSegments() const {
	vector<vector<ArcSegmentId>> out;
	out.resize(m_info.arcs.size());
	for(auto id : indexRange<ArcSegmentId>(0, m_info.segments.size()))
		out[m_info.segments[id].arc].emplace_back(id);
	return out;
}

vector<vector<ArcId>> VoronoiDiagram::cellToArcs() const {
	vector<vector<ArcId>> out;
	out.resize(m_info.cells.size());
	for(auto id : indexRange<ArcId>(0, m_info.arcs.size()))
		out[m_info.arcs[id].cell].emplace_back(id);
	return out;
}

vector<CellId> VoronoiDiagram::segmentToCell() const {
	vector<CellId> out;
	out.reserve(arcSegments().size());
	for(auto id : indexRange<ArcSegmentId>(0, m_info.segments.size()))
		out.emplace_back((*this)[(*this)[id].arc].cell);
	return out;
}

Variant<None, double2, Segment2<double>> VoronoiDiagram::operator[](Simplex simplex) const {
	if(simplex.isNode())
		return (*this)[simplex.asNode()];
	if(simplex.isEdge()) {
		auto edge = simplex.asEdge();
		return Segment2<double>((*this)[edge.first], (*this)[edge.second]);
	}
	return none;
}

Maybe<CellId> VoronoiDiagram::findClosestCell(double2 pos) const {
	Maybe<CellId> out;
	double min_dist = inf;
	int min_rank = 0;

	for(auto cell_id : indexRange<CellId>(0, m_info.cells.size())) {
		auto &cell = m_info.cells[cell_id];

		auto simplex = (*this)[cell.generator];
		double dist = inf;
		if(simplex.is<double2>())
			dist = distanceSq(pos, simplex.get<double2>());
		else if(simplex.is<Segment2<double>>())
			dist = simplex.get<Segment2<double>>().distanceSq(pos);
		int rank = cell.generator.size();

		if(dist < min_dist || (dist == min_dist && rank < min_rank)) {
			out = cell_id;
			min_dist = dist;
			min_rank = rank;
		}
	}
	return out;
}
Simplex Simplex::remap(const NodeMapping &mapping) const {
	if(isNode()) {
		if(auto node = mapping.maybeFind(m_nodes[0]))
			return *node;
	}
	if(isEdge()) {
		auto node1 = mapping.maybeFind(m_nodes[0]);
		auto node2 = mapping.maybeFind(m_nodes[1]);
		// TODO: check this
		return node1 && node2 ? Simplex(*node1, *node2) : Simplex();
	}
	return {};
}

static VoronoiCell remapCell(VoronoiCell cell, const NodeMapping &node_mapping) {
	cell.generator = cell.generator.remap(node_mapping);
	return cell;
}

DEFINE_ENUM(RectSide, right, top, left, bottom);

Maybe<RectSide> sideId(const DRect &rect, const double2 &point) {
	if(point.x == rect.ex())
		return RectSide::right;
	if(point.y == rect.ey())
		return RectSide::top;
	if(point.x == rect.x())
		return RectSide::left;
	if(point.y == rect.y())
		return RectSide::bottom;
	return none;
}

// Returns only inner points
vector<double2> borderSegments(const DRect &rect, const double2 &p1, const double2 &p2) {
	auto side1 = sideId(rect, p1), side2 = sideId(rect, p2);
	DASSERT(side1 && side2);

	double2 corners[4] = {rect.max(), {rect.x(), rect.ey()}, rect.min(), {rect.ex(), rect.y()}};

	vector<double2> points;
	while(side1 != side2) {
		points.emplace_back(corners[(int)*side1]);
		side1 = next(*side1);
	}
	return points;
}

VoronoiDiagram VoronoiDiagram::clip(DRect rect) const {
	// TODO: assert that generators are inside rect (and not on the borders)
	PGraphBuilder<double2> inserter;

	vector<pair<ArcSegmentId, ArcSegmentId>> seg_map;
	vector<Pair<VertexId>> new_arcs, new_segs;
	vector<VoronoiArc> new_arcs_data;
	vector<VoronoiArcSegment> new_segs_data;

	vector<pair<ArcId, CellId>> seg2arc_cell;

	// Clipping segments to rect
	for(auto ref : m_segments_graph.edgeRefs()) {
		Segment2<double> segment((*this)[ref.from()], (*this)[ref.to()]);

		ArcId old_arc_id = m_info.segments[ref].arc;
		auto clipped = clipSegment(segment, rect);
		if(clipped) {
			DASSERT(rect.contains(clipped->from));
			DASSERT(rect.contains(clipped->to));
			seg_map.emplace_back((int)ref.id(), (int)new_segs.size());
			seg2arc_cell.emplace_back(old_arc_id, (*this)[old_arc_id].cell);
			new_segs.emplace_back(inserter(clipped->from), inserter(clipped->to));
			new_segs_data.emplace_back(VoronoiArcSegment{old_arc_id});
		}
	}

	{ // Constructing new arcs from segments
		ImmutableGraph tgraph(new_segs);
		DASSERT(tgraph.isUndirected());
		vector<bool> visited(tgraph.numEdges(), false);

		for(int cycles_mode = 0; cycles_mode < 2; cycles_mode++) {
			for(auto vref : tgraph.vertexRefs()) {
				if(tgraph.edgesFrom(vref).size() == 2 && cycles_mode == 0)
					continue;

				for(auto eref : vref.edgesFrom()) {
					if(visited[eref])
						continue;
					visited[eref] = true;

					ArcId old_arc_id = seg2arc_cell[eref].first;
					ArcId new_arc(new_arcs.size());

					VertexId pnode = vref;
					auto nedge = eref;

					while(true) {
						auto nnode = nedge.other(pnode);
						pnode = nnode;
						new_segs_data[nedge].arc = new_arc;

						const auto &edges_from = nnode.edgesFrom();
						if(edges_from.size() != 2)
							break;
						nedge = edges_from[edges_from[0].twin() == nedge ? 1 : 0];
						if(visited[nedge])
							break;
						visited[nedge] = true;
					}

					new_arcs.emplace_back(vref.id(), pnode);
					auto &old_arc = m_info.arcs[old_arc_id];
					new_arcs_data.emplace_back(VoronoiArc{
						seg2arc_cell[eref].second, old_arc.is_primary, old_arc.touches_site});
				}
			}
		}

		DASSERT(allOf(visited, [](auto v) { return v; }));
	}

	// Adding cell generators points to point set
	for(auto &cell : m_info.cells)
		for(int i = 0; i < cell.generator.size(); i++)
			inserter(m_info.points[cell.generator[i]]);

	HashMap<VertexId, vector<CellId>> new_node2cell;
	for(auto id : indexRange<ArcSegmentId>(0, new_segs.size())) {
		auto &pair = new_segs[id];
		auto cell_id = seg2arc_cell[id].second;
		new_node2cell[pair.first].emplace_back(cell_id);
		new_node2cell[pair.second].emplace_back(cell_id);
	}
	for(auto &pair : new_node2cell)
		makeSorted(pair.second); //TODO: makeSortedUnique?

	double2 rect_center = rect.center();

	struct BorderPoint {
		double2 pos;
		VertexId id;
		double angle;

		FWK_ORDER_BY(BorderPoint, angle, pos, id);
	};

	vector<BorderPoint> border_points;
	for(auto node_id : indexRange<VertexId>(inserter.points())) {
		const auto &point = inserter[node_id];
		if(onTheEdge(rect, point) && !new_node2cell[node_id].empty())
			border_points.emplace_back(
				BorderPoint{point, node_id, vectorToAngle(normalize(point - rect_center))});
	}
	makeSorted(border_points);

	// Constructing segments & arcs on the rect borders
	for(int n = 0; n < border_points.size(); n++) {
		const auto &p1 = border_points[n];
		const auto &p2 = border_points[(n + 1) % border_points.size()];
		auto between = borderSegments(rect, p1.pos, p2.pos);

		Maybe<VertexId> n1 = inserter.find(p1.pos), n2 = inserter.find(p2.pos);
		DASSERT(n1 && n2);

		auto cells = setIntersection(new_node2cell[*n1], new_node2cell[*n2]);
		//	print("% - %: [%] [%]: %\n", p1.pos, p2.pos, transform<int>(new_node2cell[n1]),
		//			 transform<int>(new_node2cell[n2]), transform<int>(cells));
		if(!cells.size()) {
			//DASSERT(false);
			continue; // TODO: this shouldn't happen; if it does than return none / InvalidResult ?
		}

		DASSERT_EQ(cells.size(), 1);
		CellId cell_id = cells[0];

		// TODO: make sure that the order (p1-> p2) is correct
		vector<VertexId> new_nodes;
		new_nodes.emplace_back(*n1);
		for(auto &pt : between)
			new_nodes.emplace_back(inserter(pt));
		new_nodes.emplace_back(*n2);

		for(int n = 1; n < new_nodes.size(); n++) {
			VertexId from = new_nodes[n - 1], to = new_nodes[n];
			ArcId arc_id(new_arcs.size());
			ArcSegmentId seg_id(new_segs.size());

			new_arcs.emplace_back(from, to);
			new_arcs_data.emplace_back(VoronoiArc{cell_id, true, false});

			new_segs.emplace_back(from, to);
			new_segs_data.emplace_back(VoronoiArcSegment{arc_id});
		}
	}

	// Update after adding some rect corner points

	auto node_mapping = inserter.mapping(m_info.points);
	auto new_cells =
		fwk::transform(m_info.cells, [&](auto &cell) { return remapCell(cell, node_mapping); });

	VoronoiInfo new_info{inserter.extractPoints(), new_arcs_data, new_segs_data, new_cells};

	// TODO: pass mapping info (old -> new)

	return {Graph(new_segs, new_info.points.size()), Graph(new_arcs, new_info.points.size()),
			new_info};
}

GeomGraph<float2> VoronoiDiagram::merge() const {
	GeomGraph<float2> out;

	for(auto pt : m_info.points) {
		bool is_new = out.fixVertex((float2)pt).is_new;
		PASSERT(is_new);
	}

	for(auto arc : m_arc_graph.edgeRefs())
		out.fixEdge(arc.from(), arc.to(), GraphLayer::l1);
	for(auto seg : m_segments_graph.edgeRefs()) {
		auto eid = out.fixEdge(seg.from(), seg.to(), GraphLayer::l2).id;
		out[eid].ival1 = m_info.segments[seg].arc;
	}

	return out;
}

}
