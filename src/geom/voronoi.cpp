// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom/voronoi.h"

#include "fwk/geom/geom_graph.h"
#include "fwk/math/box.h"
#include "fwk/math/rotation.h"
#include "fwk/math/segment.h"
#include "fwk/sys/assert.h"
#include "fwk/variant.h"
#include "fwk/vector_map.h"

namespace fwk {

VoronoiDiagram::VoronoiDiagram(GeomGraph<double2> graph, Info info)
	: m_graph(move(graph)), m_info(move(info)) {
	// TODO: validate labels?
	// TODO: verify arcs
	for(auto &cell : m_info.cells) {
		for(int n = 0; n < cell.generator.size(); n++)
			DASSERT(m_graph.valid(cell.generator[n]));
	}
}

bool VoronoiDiagram::isArcPrimary(GEdgeId id) const {
	PASSERT(m_graph.layer(id) == arc_layer);
	return m_graph[id].ival1 != 0;
}

GEdgeId VoronoiDiagram::arcId(GEdgeId id) const {
	PASSERT(m_graph.layer(id) == seg_layer);
	return GEdgeId(m_graph[id].ival1);
}

CellId VoronoiDiagram::cellId(GEdgeId id) const {
	auto layer = m_graph.layer(id);
	PASSERT(isOneOf(layer, arc_layer, seg_layer));
	return CellId(m_graph[id].ival2);
}

vector<GEdgeId> VoronoiDiagram::arcSegments(GEdgeId edge) const {
	vector<GEdgeId> out;
	// TODO
	return out;
}

vector<GEdgeId> VoronoiDiagram::cellArcs(CellId) const {
	vector<GEdgeId> out;
	// TODO
	return out;
}

Variant<None, double2, Segment2<double>> VoronoiDiagram::operator[](Simplex simplex) const {
	if(simplex.isNode())
		return m_graph(simplex.asNode());
	if(simplex.isEdge()) {
		auto [v1, v2] = simplex.asEdge();
		return Segment2D(m_graph(v1), m_graph(v2));
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
	// Czy chcemy zachować indeksy różnych elementów? site-y raczej tak
	GeomGraph<double2> new_graph;

	// TODO: a co z site-ami punktowymi ? Chcielibyśmy sie przeiterować po
	// wierzchołkach na danej warstwie
	for(auto vert : m_graph.verts(site_layer))
		new_graph.addVertexAt(vert, m_graph(vert), site_layer);
	for(auto edge : m_graph.edges(site_layer))
		new_graph.addEdgeAt(edge, edge.from(), edge.to(), site_layer);

	// Clipping segments to rect
	for(auto ref : m_graph.edges(seg_layer)) {
		auto segment = m_graph(ref);

		GEdgeId old_arc_id(m_graph[ref].ival1);
		auto clipped = segment.clip(rect);
		if(clipped) {
			DASSERT(rect.contains(clipped->from));
			DASSERT(rect.contains(clipped->to));
			CellId old_cell_id(m_graph[old_arc_id].ival2);
			auto v1 = new_graph.fixVertex(clipped->from, seg_layer).id;
			auto v2 = new_graph.fixVertex(clipped->to, seg_layer).id;
			auto eid = new_graph.addEdge(v1, v2, seg_layer);
			new_graph[eid].ival1 = old_arc_id;
			new_graph[eid].ival2 = old_cell_id;
		}
	}

	{ // Constructing new arcs from segments
		DASSERT(new_graph.isUndirected(seg_layer));
		vector<bool> visited(new_graph.edgesEndIndex(), false);
		vector<GEdgeId> arc;

		for(int cycles_mode = 0; cycles_mode < 2; cycles_mode++) {
			for(auto vert : new_graph.verts(seg_layer)) {
				if(vert.numEdgesFrom(seg_layer) == 2 && cycles_mode == 0)
					continue;

				for(auto edge : vert.edgesFrom(seg_layer)) {
					if(visited[edge])
						continue;
					visited[edge] = true;
					arc.clear();

					GEdgeId old_arc_id(new_graph[edge].ival1);

					VertexId pnode = vert;
					auto nedge = edge;

					while(true) {
						auto nnode = nedge.other(pnode);
						pnode = nnode;
						arc.emplace_back(nedge);
						const auto &edges_from = nnode.edgesFrom(seg_layer);
						if(edges_from.size() != 2)
							break;
						nedge = edges_from[edges_from[0].twin() == nedge ? 1 : 0];
						if(visited[nedge])
							break;
						visited[nedge] = true;
					}

					auto arc_id = new_graph.addEdge(vert.id(), pnode, arc_layer);
					new_graph[arc_id] = m_graph[old_arc_id];
					for(auto seg_id : arc)
						new_graph[seg_id].ival1 = arc_id;
				}
			}
		}

		//DASSERT(allOf(visited, [](auto v) { return v; }));
	}

	// TODO: vector?
	HashMap<VertexId, vector<CellId>> vertex_cells;
	for(auto seg : new_graph.edges(seg_layer)) {
		CellId cell_id(new_graph[seg].ival2);
		vertex_cells[seg.from()].emplace_back(cell_id);
		vertex_cells[seg.to()].emplace_back(cell_id);
	}
	for(auto &pair : vertex_cells)
		makeSorted(pair.second); //TODO: makeSortedUnique?

	double2 rect_center = rect.center();

	struct BorderPoint {
		double2 pos;
		VertexId id;
		double angle;

		FWK_ORDER_BY(BorderPoint, angle, pos, id);
	};

	vector<BorderPoint> border_points;
	for(auto vert : new_graph.verts()) {
		auto point = new_graph(vert);

		if(onTheEdge(rect, point) && vertex_cells[vert]) {
			auto angle = vectorToAngle(normalize(point - rect_center));
			border_points.emplace_back(BorderPoint{point, vert, angle});
		}
	}
	makeSorted(border_points);

	// Constructing segments & arcs on the rect borders
	for(int n = 0; n < border_points.size(); n++) {
		const auto &p1 = border_points[n];
		const auto &p2 = border_points[(n + 1) % border_points.size()];
		auto between = borderSegments(rect, p1.pos, p2.pos);

		auto n1 = new_graph.findVertex(p1.pos), n2 = new_graph.findVertex(p2.pos);
		DASSERT(n1 && n2);

		auto cells = setIntersection(vertex_cells[*n1], vertex_cells[*n2]);
		//	print("% - %: [%] [%]: %\n", p1.pos, p2.pos, transform<int>(vertex_cells[n1]),
		//			 transform<int>(vertex_cells[n2]), transform<int>(cells));
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
			new_nodes.emplace_back(new_graph.fixVertex(pt, seg_layer).id);
		new_nodes.emplace_back(*n2);

		for(int n = 1; n < new_nodes.size(); n++) {
			VertexId from = new_nodes[n - 1], to = new_nodes[n];
			auto arc_id = new_graph.addEdge(from, to, arc_layer);
			auto seg_id = new_graph.addEdge(from, to, seg_layer);
			new_graph[arc_id].ival1 = false;
			new_graph[arc_id].ival2 = cell_id;
			new_graph[seg_id].ival1 = arc_id;
			new_graph[seg_id].ival2 = cell_id;
		}
	}

	// Update after adding some rect corner points; TODO: what?

	return {new_graph, m_info};
}
}
