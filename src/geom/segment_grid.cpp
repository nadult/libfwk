// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom/segment_grid.h"

#include "fwk/index_range.h"
#include "fwk/math/box_iter.h"
#include "fwk/sparse_span.h"

namespace fwk {
SquareBorder::SquareBorder(IRect rect, int2 center, int radius) {
	DASSERT(radius > 0);

	int2 corners[4] = {center + int2(-radius, -radius), center + int2(+radius, -radius),
					   center + int2(+radius, +radius), center + int2(-radius, +radius)};

	for(int dir = 0; dir < 4; dir++) {
		int sign = dir & 2 ? -1 : 1;
		auto from = corners[dir], to = corners[(dir + 1) % 4];

		steps[dir] = 0;
		if(dir & 1) { // vertical
			if(from.x < rect.x() || from.x >= rect.ex())
				continue;

			from.y = clamp(from.y, rect.y(), rect.ey() - 1);
			to.y = clamp(to.y, rect.y(), rect.ey() - 1);
			start[dir] = from;
			steps[dir] = std::abs(to.y - from.y);
		} else {
			if(from.y < rect.y() || from.y >= rect.ey())
				continue;

			from.x = clamp(from.x, rect.x(), rect.ex() - 1);
			to.x = clamp(to.x, rect.x(), rect.ex() - 1);
			start[dir] = from;
			steps[dir] = std::abs(to.x - from.x);
		}
	}

	steps[4] = 0;
}

SquareBorder::Iter::Iter(const SquareBorder &border, int tdir)
	: border(border), dir(tdir), steps(border.steps[dir]) {
	while(dir < 4 && steps <= 0)
		steps = border.steps[++dir];
	if(dir < 4)
		pos = border.start[dir];
}

const SquareBorder::Iter &SquareBorder::Iter::operator++() {
	if(steps <= 0) {
		auto prev_pos = pos;

		while(dir < 4 && steps <= 0)
			steps = border.steps[++dir];
		if(dir == 4)
			return *this;
		pos = border.start[dir];

		if(pos == prev_pos)
			return operator++();
		return *this;
	}

	int sign = dir & 2 ? -1 : 1;
	if(dir & 1)
		pos.y += sign;
	else
		pos.x += sign;
	steps--;

	// TODO: fix it
	//	if(steps == 0 && dir == 3 && pos == border.start[0]) {
	//		dir++;
	//	}

	return *this;
}

template <class T> auto SegmentGrid<T>::bestGrid(SparseSpan<T> points, int num_edges) -> RGrid {
	auto rect = enclose(points);
	// TODO: how to properly handle lines & empty rects ?
	if(rect.width() == 0 && rect.height() == 0)
		return {Box<T>(0, 0, 1, 1), T(1), 1};

	auto num_cells = double(max(points.size(), num_edges)) * 1.5;
	auto wh_ratio = double(rect.width()) / double(rect.height());

	auto num_cells_vert = rect.height() == 0 ? num_cells : std::sqrt(num_cells * wh_ratio);
	auto num_cells_horiz = num_cells / num_cells_vert;
	DASSERT(!isNan(num_cells_vert, num_cells_horiz));

	num_cells_vert = clamp(std::floor(num_cells_vert + 1.0), 1.0, 1024.0);
	num_cells_horiz = clamp(std::floor(num_cells_horiz + 1.0), 1.0, 1024.0);

	T cell_size(double(rect.width()) / num_cells_vert, double(rect.height()) / num_cells_horiz);
	if(cell_size.x == 0)
		cell_size.x = 1;
	if(cell_size.y == 0)
		cell_size.y = 1;
	return {rect, cell_size, 1};
}

template <class T>
void SegmentGrid<T>::initialize(SparseSpan<Pair<VertexId>> edges, SparseSpan<Point> points) {
	m_grid = bestGrid(points, edges.size());
	m_edges = edges;
	m_points = points;
	m_cells.resize(m_grid.width() * m_grid.height());
	for(auto pt : points)
		m_cells[index(toCell(pt))].num_verts++;

	vector<int> seg_cell_indices;
	PodVector<int> seg_cell_offsets(edges.size()), seg_cell_counts(edges.size());
	seg_cell_indices.reserve(edges.size() * 2);

	for(auto eid : edges.indices()) {
		auto &edge = edges[eid];
		Segment seg(points[edge.first], points[edge.second]);
		auto cell_rect = toCell(enclose(seg));
		bool single_cell = cell_rect.width() == 1 || cell_rect.height() == 1;
		seg_cell_offsets[eid] = seg_cell_indices.size();

		for(auto cell_pos : cells(cell_rect)) {
			auto cidx = index(cell_pos);
			PASSERT(cidx >= 0 && cidx < m_cells.size());
			auto &cell = m_cells[cidx];

			if(single_cell || seg.testIsect(m_grid.toWorldRect(cell_pos))) {
				seg_cell_indices.emplace_back(cidx);
				cell.num_edges++;
			}
		}

		seg_cell_counts[eid] = seg_cell_indices.size() - seg_cell_offsets[eid];
	}

	int cur_index = 0;
	for(auto &cell : m_cells) {
		cell.first_index = cur_index;
		cur_index += cell.num_verts + cell.num_edges;
		cell.num_verts = 0;
		cell.num_edges = 0;
	}

	m_cell_indices.resize(cur_index + points.size() + 1);
	for(auto id : points.indices()) {
		auto &cell = m_cells[index(toCell(points[id]))];
		int idx = cell.first_index + cell.num_verts++;
		m_cell_indices[idx] = id;
	}

	for(auto eid : edges.indices()) {
		int first = seg_cell_offsets[eid];
		for(int n : intRange(seg_cell_counts[eid])) {
			int cidx = seg_cell_indices[first + n];
			auto &cell = m_cells[cidx];
			int idx = cell.first_index + int(cell.num_verts) + cell.num_edges++;
			m_cell_indices[idx] = eid;
		}
	}

	//	print("points:% segs:% cells:% (%) insts:%\n", points.size(), edges.size(), m_cells.size(),
	//			 size(), m_cell_indices.size());
}

template <class T>
SegmentGrid<T>::SegmentGrid(SparseSpan<Pair<VertexId>> edges, SparseSpan<Point> points) {
	initialize(edges, points);
}

template <class T>
SegmentGrid<T>::SegmentGrid(SparseSpan<Pair<VertexId>> edges, PodVector<Point> points,
							CSpan<bool> valids, int psize)
	: m_points_buffer(move(points)) {
	SparseSpan<Point> pspan(m_points_buffer.data(), valids, psize);
	initialize(edges, pspan);
}

template <class T> vector<int2> SegmentGrid<T>::traceSlow(const Segment &seg) const {
	vector<int2> out;

	for(auto cell_id : m_grid) {
		if(seg.testIsect(m_grid.toWorldRect(cell_id)))
			if(!empty(cell_id))
				out.emplace_back(cell_id);
	}

	return out;
}

template <class T> int sign(T value) { return value < T(0) ? -1 : value > T(0) ? 1 : 0; }

template <int axis1, class T, class S, EnableIfFptVec<T, 2>...>
auto linePos(const T &dir, const T &start, S k) {
	const int axis2 = 1 - axis1;
	return start[axis2] + (dir[axis2] * (k - start[axis1])) / dir[axis1];
}

template <int axis1, class T, class S, EnableIfIntegralVec<T, 2>...>
auto linePos(const T &dir, const T &start, S k) {
	const int axis2 = 1 - axis1;
	return start[axis2] + int((llint(dir[axis2]) * llint(k - start[axis1])) / llint(dir[axis1]));
}

template <class T> T SegmentGrid<T>::clipSegmentPoint(T point, T vector) const {
	using Scalar = typename T::Scalar;
	auto wrect = m_grid.worldRect();

	if(point.x < wrect.x()) {
		Scalar lpos_minx = linePos<0>(vector, point, wrect.x());
		point = {wrect.x(), lpos_minx};
	}
	if(point.y < wrect.y()) {
		Scalar lpos_miny = linePos<1>(vector, point, wrect.y());
		point = {lpos_miny, wrect.y()};
	}
	if(point.x > wrect.ex()) {
		Scalar lpos_maxx = linePos<0>(vector, point, wrect.ex());
		point = {wrect.ex(), lpos_maxx};
	}
	if(point.y > wrect.ey()) {
		Scalar lpos_maxy = linePos<1>(vector, point, wrect.ey());
		point = {lpos_maxy, wrect.ey()};
	}

	return point;
}

template <class T> Pair<int2> SegmentGrid<T>::clipSegment(Segment seg) const {
	auto vec = seg.to - seg.from;
	seg.from = clipSegmentPoint(seg.from, vec);
	seg.to = clipSegmentPoint(seg.to, -vec);

	auto source_pos = toCell(seg.from);
	auto target_pos = toCell(seg.to);

	source_pos = vmin(source_pos, size() - int2(1, 1));
	target_pos = vmin(target_pos, size() - int2(1, 1));

	// TODO: this may be a bit inaccurate, but only +/- one cell
	// TODO: try to find a proper test case where this may happen
	return {source_pos, target_pos};
}

template <class T, EnableIfFptVec<T, 2>...> int whichSide(const T &vec1, const T &vec2) {
	return sign(cross(vec1, vec2));
}

template <class T, EnableIfIntegralVec<T, 2>...> int whichSide(const T &vec1, const T &vec2) {
	static_assert(sizeof(T) < sizeof(llint2));
	return sign(cross<llint2>(vec1, vec2));
}

template <class T> PoolVector<int2> SegmentGrid<T>::trace(const Segment &iseg) const {
	PoolVector<int2> out;
	Segment seg = iseg;

	int sign_x = sign(seg.to[0] - seg.from[0]);
	int sign_y = sign(seg.to[1] - seg.from[1]);

	auto source_pos = m_grid.toCell(seg.from);
	auto target_pos = m_grid.toCell(seg.to);

	if(source_pos == target_pos) {
		if(!empty(source_pos))
			out.emplace_back(source_pos);
		return out;
	}

	if(sign_x == 0) {
		if(source_pos.x < 0 || source_pos.x >= width())
			return out;

		source_pos.y = clamp(source_pos.y, 0, height() - 1);
		target_pos.y = clamp(target_pos.y, 0, height() - 1);

		out.reserve(std::abs(target_pos.y - source_pos.y) + 1);
		out.emplace_back(source_pos);
		while(source_pos != target_pos) {
			source_pos.y += target_pos.y > source_pos.y ? 1 : -1;
			if(!m_cells[index(source_pos)].empty())
				out.emplace_back(source_pos);
		}
		return out;
	}

	if(sign_y == 0) {
		if(source_pos.y < 0 || source_pos.y >= height())
			return out;

		source_pos.x = clamp(source_pos.x, 0, width() - 1);
		target_pos.x = clamp(target_pos.x, 0, width() - 1);

		out.reserve(std::abs(target_pos.x - source_pos.x) + 1);
		out.emplace_back(source_pos);
		while(source_pos != target_pos) {
			source_pos.x += target_pos.x > source_pos.x ? 1 : -1;
			if(!m_cells[index(source_pos)].empty())
				out.emplace_back(source_pos);
		}
		return out;
	}

	if(!inRange(source_pos)) {
		auto cpos = clipSegmentPoint(seg.from, seg.to - seg.from);
		source_pos = vmin(toCell(cpos), size() - int2(1, 1));
		source_pos[0] -= sign_x;
		source_pos[1] -= sign_y;
	}
	//TODO: handle situations when ray is touching something on the border of a cell

	int2 corner_offset(sign_x > 0, sign_y > 0);

	int2 move_neg, move_pos;
	if(sign_x < 0 && sign_y < 0) {
		move_neg = {-1, 0};
		move_pos = {0, -1};
	}
	if(sign_x > 0 && sign_y < 0) {
		move_neg = {0, -1};
		move_pos = {1, 0};
	}
	if(sign_x > 0 && sign_y > 0) {
		move_neg = {1, 0};
		move_pos = {0, 1};
	}
	if(sign_x < 0 && sign_y > 0) {
		move_neg = {0, 1};
		move_pos = {-1, 0};
	}

	auto cell_id = source_pos;
	auto seg_vec = seg.to - seg.from;

	if(!empty(cell_id))
		out.emplace_back(cell_id);

	while(cell_id != target_pos) {
		auto corner = cellCorner(cell_id + corner_offset);
		auto side = whichSide(corner - seg.from, seg_vec);

		if(side == 0) {
			// TODO: cell_id -> cell
			auto cell1 = cell_id + move_pos;
			auto cell2 = cell_id + move_neg;

			// TODO: handle this situation in a better way
			if(!empty(cell1))
				out.emplace_back(cell1);
			if(!empty(cell2))
				out.emplace_back(cell2);
			cell_id += move_pos + move_neg;
			if(isOneOf(target_pos, cell1, cell2)) {
				if(!empty(cell_id))
					out.emplace_back(cell_id);
				break;
			}
		} else if(side < 0) {
			cell_id += move_neg;
		} else { // side > 0
			cell_id += move_pos;
		}

		if(!empty(cell_id)) {
			out.emplace_back(cell_id);
		} else {
			if(cell_id.x < -1 || cell_id.x > width() || cell_id.y < -1 || cell_id.y > height())
				break;
		}
	}

	return out;
}

template <class T>
Maybe<EdgeId> SegmentGrid<T>::closestEdge(const Point &point, Scalar min_dist) const {
	auto cell_pos = vclamp(toCell(point), int2(0, 0), size() - int2(1, 1));
	using Dist = decltype(Segment().distanceSq(Point()));
	Dist min_dist_sq = Dist(min_dist) * min_dist;
	Pair<Dist, Maybe<EdgeId>> closest = {min_dist_sq, none};

	for(auto eid : cellEdges(cell_pos)) {
		auto [v1, v2] = m_edges[eid];
		closest = minFirst(closest, {Segment{m_points[v1], m_points[v2]}.distanceSq(point), eid});
	}

	int max_radius = max(cell_pos.x, width() - cell_pos.x, cell_pos.y, height() - cell_pos.y) + 1;
	//print("cell: %\n", cell_pos);

	// TODO: small static hashmap for visited segments?
	for(int radius : intRange(1, max_radius)) {
		Scalar cur_dist_sq = inf;

		SquareBorder square(m_grid.cellRect(), cell_pos, radius);
		for(auto sq_pos : square) {
			auto cell_dist_sq = m_grid.toWorldRect(sq_pos).distanceSq(point);
			cur_dist_sq = min(cur_dist_sq, cell_dist_sq);

			if(cell_dist_sq < min_dist)
				for(auto eid : cellEdges(sq_pos))
					if(closest.second != eid) {
						auto [v1, v2] = m_edges[eid];
						closest = minFirst(
							closest, {Segment{m_points[v1], m_points[v2]}.distanceSq(point), eid});
					}
		}

		//	print("radius:% min:% cur:% found:%\n", radius, min_dist, cur_dist, closest);
		if(cur_dist_sq > min_dist)
			break;
	}

	return closest.second;
}

template <class T>
Maybe<VertexId> SegmentGrid<T>::closestVertex(const Point &point, Scalar min_dist,
											  Maybe<VertexId> ignore) const {
	auto cell_pos = vclamp(toCell(point), int2(0, 0), size() - int2(1, 1));
	using PScalar = PromoteIntegral<Scalar>;
	using PPoint = PromoteIntegral<Point>;
	auto min_dist_sq = PScalar(min_dist) * min_dist;
	Pair<PScalar, Maybe<VertexId>> closest = {min_dist_sq, none};

	for(auto nid : cellVerts(cell_pos))
		if(nid != ignore) {
			auto dist_sq = distanceSq<PPoint>(point, m_points[nid]);
			closest = minFirst(closest, {dist_sq, nid});
		}

	int max_radius = max(cell_pos.x, width() - cell_pos.x, cell_pos.y, height() - cell_pos.y) + 1;
	//print("cell: %\n", cell_pos);

	// TODO: small static hashmap for visited segments?

	for(int radius : intRange(1, max_radius)) {
		Scalar cur_dist_sq = inf;

		SquareBorder square(m_grid.cellRect(), cell_pos, radius);
		for(auto sq_pos : square) {
			auto cell_dist_sq = m_grid.toWorldRect(sq_pos).distanceSq(point);
			cur_dist_sq = min(cur_dist_sq, cell_dist_sq);

			if(cell_dist_sq >= closest.first)
				continue;

			for(auto nid : cellVerts(sq_pos))
				if(nid != ignore) {
					auto dist_sq = distanceSq<PPoint>(point, m_points[nid]);
					closest = minFirst(closest, {dist_sq, nid});
				}
		}

		//	print("radius:% min:% cur:% found:%\n", radius, min_dist, cur_dist, closest);
		if(cur_dist_sq > min_dist_sq)
			break;
	}

	return closest.second;
}

template class SegmentGrid<short2>;
template class SegmentGrid<int2>;
template class SegmentGrid<double2>;
template class SegmentGrid<float2>;
}
