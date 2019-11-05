// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/geom/contour.h"

#include "fwk/geom/contour_funcs.h"
#include "fwk/math/interpolation.h"
#include "fwk/math/rotation.h"
#include "fwk/variant.h"

namespace fwk {

template <class T> bool isContinuousContour(CSpan<Segment<T>> edges) {
	if(edges.empty())
		return true;
	auto prev = edges[0].from;
	for(auto edge : edges) {
		if(edge.from != prev)
			return false;
		prev = edge.to;
	}
	return true;
}

// TODO: maybe we should assume that edges are proper instead of fixing them?
//       the same in point constructor
template <class T>
Contour<T>::Contour(CSpan<Segment> edges, bool flip_tangents) : m_flip_tangents(flip_tangents) {
	ASSERT(isContinuousContour(edges));
	ASSERT(edges.size() >= 1);

	m_points.reserve(edges.size() + 1);

	for(auto &edge : edges)
		DASSERT(!isNan(edge.from) && !isNan(edge.to));

	m_points.emplace_back(edges[0].from);
	for(auto edge : edges)
		if(edge.from != edge.to)
			m_points.emplace_back(edge.to);
	if(m_points.back() == m_points.front()) {
		m_points.pop_back();
		m_is_looped = true;
	}
	computeLengths();
}

template <class T>
Contour<T>::Contour(CSpan<Point> points, bool is_looped, bool flip_tangents)
	: m_is_looped(is_looped), m_flip_tangents(flip_tangents) {
	m_points.reserve(points.size());
	DASSERT(!isNan(points));
	ASSERT_GE(points.size(), 1);

	// TODO: wtf is this? why is this needed?
	m_points.emplace_back(points.front());
	for(int n = 1; n < points.size(); n++)
		if(points[n] != m_points.back())
			m_points.emplace_back(points[n]);

	if(is_looped && m_points.back() == m_points.front())
		m_points.pop_back();

	ASSERT(!is_looped || m_points.size() >= 2);
	computeLengths();
}

template <class T> Contour<T>::~Contour() = default;
template <class T> Contour<T>::Contour(const Contour &rhs) = default;
template <class T> Contour<T> &Contour<T>::operator=(const Contour &rhs) = default;

template <class T> auto Contour<T>::operator[](EdgeId edge_id) const -> Segment {
	DASSERT_EX(valid(edge_id), edge_id);
	return {m_points[edge_id], m_points[(edge_id + 1) % m_points.size()]};
}

template <class T> Pair<EdgeId> Contour<T>::adjacentEdges(VertexId node_id) const {
	DASSERT_EX(valid(node_id), node_id);

	if(m_is_looped) {
		int prev = node_id == 0 ? numEdges() - 1 : node_id - 1;
		int next = node_id;
		return {EdgeId(prev), EdgeId(next)};
	}

	int prev = node_id - 1;
	int next = node_id + 1 == numNodes() ? -1 : node_id + 1;
	return {EdgeId(prev), EdgeId(next)};
}

template <class T> void Contour<T>::computeLengths() {
	m_upto_length = summedSegmentLengths<T>(m_points, m_is_looped);
	m_length = m_upto_length ? m_upto_length.back() : Scalar(0);
}

template <class T> auto Contour<T>::edgeLength(int edge_id) const -> Scalar {
	DASSERT(edge_id >= 0 && edge_id < m_upto_length.size());
	auto out = m_upto_length[edge_id];
	if(edge_id > 0)
		out -= m_upto_length[edge_id - 1];
	return out;
}

template <class T> auto Contour<T>::segmentAt(Scalar pos) const -> Segment {
	return segmentAt(trackPos(pos));
}
template <class T> auto Contour<T>::segmentAt(TrackPos tpos) const -> Segment {
	return edge(tpos.first);
}

template <class T> auto Contour<T>::clampPos(Scalar pos) const -> Scalar {
	return clamp(pos, Scalar(0), m_length);
}

template <class T> auto Contour<T>::wrapPos(Scalar pos) const -> Scalar {
	if(isPoint())
		return Scalar(0);

	int ilen = (int)(pos / m_length);
	if(pos < Scalar(0))
		ilen -= 1;
	pos -= Scalar(ilen) * m_length;
	return clampPos(pos); // just in case of small errors
}

template <class T> bool Contour<T>::valid(TrackPos tpos) const {
	if(isPoint())
		return tpos == TrackPos();
	return tpos.first >= 0 && tpos.first < m_upto_length.size() && tpos.second >= Scalar(0) &&
		   tpos.second <= Scalar(1);
}

template <class T> auto Contour<T>::trackPos(Scalar pos) const -> TrackPos {
	if(isPoint())
		return {};
	pos = m_is_looped ? wrapPos(pos) : clampPos(pos);
	int index = segmentIndex<Scalar>(m_upto_length, pos);

	Scalar prev_length = index > 0 ? m_upto_length[index - 1] : Scalar(0);
	Scalar edge_length = m_upto_length[index] - prev_length;
	pos -= prev_length;

	return {index, pos / edge_length};
}

template <class T> auto Contour<T>::linearPos(TrackPos tpos) const -> Scalar {
	DASSERT(valid(tpos));
	if(isPoint())
		return Scalar(0);
	auto upto_len = tpos.first == 0 ? Scalar(0) : m_upto_length[tpos.first - 1];
	Scalar elength = m_upto_length[tpos.first] - upto_len;
	return upto_len + elength * tpos.second;
}

template <class T> T Contour<T>::point(TrackPos track_pos) const {
	if(isPoint())
		return m_points.front();
	auto p1 = m_points[track_pos.first];
	auto p2 = m_points[(track_pos.first + 1) % m_points.size()];
	return lerp(p1, p2, track_pos.second);
}

template <class Vec> static Vec perp(const Vec &vec) {
	if constexpr(Vec::vec_size == 2)
		return perpendicular(vec);
	else
		return cross(vec, Vec(0, 1, 0));
}

template <class T> T Contour<T>::tangent(EdgeId edge_id) const {
	DASSERT_EX(valid(edge_id), edge_id);
	auto edge = (*this)[edge_id];
	auto nrm = normalize(perp(edge.to - edge.from));
	return m_flip_tangents ? -nrm : nrm;
}

template <class T> T Contour<T>::tangent(TrackPos track_pos) const {
	if(isPoint())
		return {};
	return tangent(EdgeId(track_pos.first));
}

template <class T> auto Contour<T>::edgeSide(EdgeId edge_id, T point) const -> Scalar {
	auto edge = (*this)[edge_id];
	auto nrm = perp(edge.to - edge.from);
	if(m_flip_tangents)
		nrm = -nrm;
	return dot(nrm, point - edge.from);
}

template <class T> auto Contour<T>::closestPos(T point) const -> TrackPos {
	TrackPos out;

	Scalar min_dist = inf;

	for(int eid = 0; eid < numEdges(); eid++) {
		auto segment = edge(eid);
		auto param = segment.closestPointParam(point);
		auto dist = distanceSq(point, segment.at(param));
		if(dist < min_dist) {
			min_dist = dist;
			out = {eid, param};
		}
	}

	return out;
}

template <class T>
template <class U, EnableInDimension<U, 2>...>
auto Contour<T>::isectParam(const Segment &segment) const -> vector<IsectParam> {
	if(isPoint() && segment.distanceSq(m_points.front()) == Scalar(0))
		return {TrackPos()};

	vector<IsectParam> out;
	for(auto ei : indexRange<EdgeId>(numEdges())) {
		auto isect = (*this)[ei].isectParam(segment);

		if(isect.isPoint())
			out.emplace_back(TrackPos{ei, isect.asPoint()});
		else if(isect.isInterval())
			out.emplace_back(
				make_pair(TrackPos{ei, isect.closest()}, TrackPos{ei, isect.farthest()}));
	}
	return out;
}

template <class T> auto Contour<T>::at(IsectParam param) const -> Isect {
	if(const TrackPos *pos = param)
		return point(*pos);
	if(const pair<TrackPos, TrackPos> *seg = param) {
		// TODO: fixit
		DASSERT(seg->first.first == seg->second.first);
		return Segment(point(seg->first), point(seg->second));
	}
	return none;
}

template <class T> vector<int> Contour<T>::findProtrudingPoints() const {
	vector<int> out;

	if(isLooped()) {
		for(int n : intRange(m_points)) {
			int prev = n == 0 ? m_points.size() - 1 : n - 1;
			int next = n + 1 == m_points.size() ? 0 : n + 1;
			if(m_points[prev] == m_points[next])
				out.emplace_back(n);
		}
	} else {
		out.emplace_back(0);
		for(int n = 1; n < m_points.size() - 1; n++)
			if(m_points[n - 1] == m_points[n + 1])
				out.emplace_back(n);
		out.emplace_back(m_points.size() - 1);
	}

	return out;
}

template <class T> vector<T> Contour<T>::genEvenPoints(Scalar distance) const {
	vector<T> out;
	if(isPoint())
		return out;

	Scalar pos(0);
	while(pos < length()) {
		out.emplace_back(point(trackPos(pos)));
		pos += distance;
	}

	return out;
}

template <class T> vector<T> Contour<T>::findClosestPoints(vector<T> points) const {
	DASSERT(!isPoint());
	for(auto &pt : points)
		pt = point(closestPos(pt));
	return points;
}

// It shouldn't depend on point density, but how to do it ???
template <class T>
template <class U, EnableInDimension<U, 2>...>
auto Contour<T>::niceAngles() const -> vector<Scalar> {
	if(isPoint())
		return {Scalar(0)};

	T first, last;
	if(isLooped()) {
		first = m_points.back();
		last = m_points.front();
	} else {
		first = m_points[0] - (m_points[1] - m_points[0]);
		last = m_points.back() - (m_points[m_points.size() - 2] - m_points.back());
	}

	vector<T> normals;
	normals.reserve(m_points.size());

	for(int n : intRange(m_points)) {
		auto prev = n == 0 ? first : m_points[n - 1];
		auto cur = m_points[n];
		auto next = n + 1 == m_points.size() ? last : m_points[n + 1];

		auto vec1 = normalize(prev - cur), vec2 = normalize(next - cur);
		auto ang_mid = angleBetween(vec1, vec2);
		auto normal = rotateVector(-vec1, ang_mid * Scalar(0.5));
		if(prev == next)
			normal = -vec1;
		normals.emplace_back(normal);
	}

	vector<Scalar> angles;
	angles.reserve(m_points.size());
	double cur_angle = vectorToAngle(normals[0]);

	for(int n : intRange(0, normals.size() + 1)) {
		auto n1 = normals[n % normals.size()];
		auto n2 = normals[(n + 1) % normals.size()];
		double angle_diff = angleTowards(-n1, T(), n2);
		angles.emplace_back((float)cur_angle);
		cur_angle += angle_diff;
	}

	return angles;
}

template <class T>
Contour<T> Contour<T>::simplify(Scalar theta, Scalar max_err_sq, Scalar max_dist_sq) const {
	vector<T> out;
	out.reserve(numPoints());

	max_err_sq = max_err_sq * max_err_sq;
	max_dist_sq = max_dist_sq * max_dist_sq;

	if(numPoints() <= 2)
		return *this;

	int count = m_points.size() - (isLooped() ? 0 : 1);

	T last = m_points.front();
	double terror = 0.0;
	out.emplace_back(last);
	int num_dropped = 0;

	for(int n : intRange(1, count)) {
		const auto &cur = m_points[n];
		const auto &next = m_points[(n + 1) % m_points.size()];

		auto normal = normalize(cur - last);
		auto nnormal = normalize(next - cur);
		auto similarity = std::abs(dot(normal, nnormal));

		Segment new_seg(last, next);
		auto dist_sq = new_seg.lengthSq();

		bool keep_point =
			similarity - terror < theta || num_dropped >= 12 || dist_sq >= max_dist_sq;

		if(!keep_point) {
			double err_sq = 0.0;
			for(int i = 0; i <= num_dropped; i++)
				err_sq = max<double>(err_sq, new_seg.distanceSq(m_points[n - i]));
			keep_point = err_sq >= max_err_sq;
		}

		if(keep_point) {
			out.emplace_back(cur);
			last = cur;
			terror = 0;
			num_dropped = 0;
		} else {
			terror += 1.0 - similarity;
			num_dropped++;
		}
	}

	if(!isLooped())
		out.emplace_back(m_points.back());
#ifdef CONTOUR_TESTING
	static int tot = 0, tot_out = 0;
	tot += size();
	tot_out += out.size();
	print("Simplified % -> %\nTotal: % -> %\n", size(), out.size(), tot, tot_out);

	{
		Contour test{out, isLooped()};
		double max_err = 0.0;
		for(auto p : m_points)
			max_err = max<double>(max_err, distance(p, test.point(test.closestPos(p))));
		print("Max error: %\n", max_err);
		DASSERT_LE(max_err, max_dist);
	}
#endif

	return {out, isLooped()};
}

template <class T> Contour<T> Contour<T>::reverse(bool flip_tangents) const {
	auto points = m_points;
	std::reverse(begin(points), end(points));
	return Contour<T>(points, m_is_looped, m_flip_tangents ^ flip_tangents);
}

template <class T> Contour<T> Contour<T>::smooth(Scalar step) const {
	if(m_points.size() <= 2)
		return *this;

	vector<T> out;

	auto first_vec = m_points[0] - m_points[1];
	auto last_vec = m_points.back() - m_points[m_points.size() - 2];

	vector<T> temp;
	temp.reserve(m_points.size() + 3);
	temp.emplace_back(m_is_looped ? m_points.back() : m_points[0] + first_vec);
	insertBack(temp, m_points);
	temp.emplace_back(m_is_looped ? m_points[0] : m_points.back() + last_vec);
	temp.emplace_back(m_is_looped ? m_points[1] : m_points.back() + last_vec * Scalar(2));

	for(double t = 0.0; t < length(); t += step) {
		auto [i, val] = trackPos(t);
		out.emplace_back(interpCatmullRom(temp[i], temp[i + 1], temp[i + 2], temp[i + 3], val));
	}
	int end = m_points.size() - 1;
	out.emplace_back(interpCatmullRom(temp[end], temp[end + 1], temp[end + 2], temp[end + 3], 1.0));
	return {move(out), isLooped()};
}

template <class T> Contour<T> Contour<T>::blur(int width) const {
	if(m_points.size() <= 3)
		return *this;

	vector<T> out;
	out.resize(m_points.size());
	int num_points = m_points.size();
	width = min(width, num_points / 2 - 1);

	float weight_mul = 1.0f / float(width + 1);
	for(int n : intRange(out)) {
		T sum = m_points[n];
		Scalar weight_sum = 1;

		int left_count = m_is_looped ? width : min(width, n);
		int right_count = m_is_looped ? width : min(width, out.size() - n - 1);

		for(int i : intRange(left_count)) {
			float weight = float(width - i) * weight_mul;
			int idx = n - i - 1;
			sum += m_points[idx < 0 ? idx + num_points : idx] * weight;
			weight_sum += weight;
		}
		for(int i : intRange(right_count)) {
			float weight = float(width - i) * weight_mul;
			int idx = n + i + 1;
			sum += m_points[idx >= num_points ? idx - num_points : idx] * weight;
			weight_sum += weight;
		}

		out[n] = sum / weight_sum;
	}

	if(!isLooped()) {
		out.front() = m_points.front();
		out.back() = m_points.back();
	}

	return {move(out), isLooped()};
}

template <class T>
SubContourRef<T>::SubContourRef(const Contour &contour, TrackPos from, TrackPos to)
	: SubContourRef(contour, contour.linearPos(from), contour.linearPos(to)) {}

template <class T>
SubContourRef<T>::SubContourRef(const Contour &contour, Scalar from, Scalar to)
	: m_contour(contour), m_from(from), m_to(to), m_is_inversed(false) {
	if(m_from > m_to) {
		swap(m_from, m_to);
		m_is_inversed = true;
	}

	if(!contour.isLooped()) {
		m_from = contour.clampPos(m_from);
		m_to = contour.clampPos(m_to);
	}
	// TODO: there is a std function for that...
	while(m_from > m_contour.length()) {
		m_from -= m_contour.length();
		m_to -= m_contour.length();
	}

	m_length = std::abs(m_to - m_from);
}

template <class T> auto SubContourRef<T>::trackPos(Scalar pos) const -> TrackPos {
	return m_contour.trackPos(m_is_inversed ? m_to - pos : m_from + pos);
}

template <class T> SubContourRef<T>::operator Contour() const {
	vector<T> points;

	auto tstart = m_contour.trackPos(m_from);
	auto tend = m_contour.trackPos(m_to);
	T start = m_contour.point(tstart);
	T end = m_contour.point(tend);

	points.emplace_back(start);

	const auto &cpoints = m_contour.points();
	int num_cpoints = cpoints.size();
	int num_cedges = m_contour.numEdges();

	int tpos = tstart.first;
	double pos = m_contour.m_upto_length[tpos];
	while(pos < m_to) {
		points.emplace_back(cpoints[(tpos + 1) % num_cpoints]);
		tpos = (tpos + 1) % num_cedges;
		pos += m_contour.edgeLength(tpos);
	}

	if(points.back() != end)
		points.emplace_back(end);

	if(m_is_inversed)
		std::reverse(begin(points), std::end(points));

	return Contour(points, false, m_is_inversed);
}

template class Contour<float2>;
template class Contour<double2>;
template class Contour<float3>;
template class Contour<double3>;

template auto Contour<float2>::isectParam(const Segment &) const -> vector<IsectParam>;
template auto Contour<double2>::isectParam(const Segment &) const -> vector<IsectParam>;

template vector<float> Contour<float2>::niceAngles() const;
template vector<double> Contour<double2>::niceAngles() const;

template class SubContourRef<float2>;
template class SubContourRef<double2>;
template class SubContourRef<float3>;
template class SubContourRef<double3>;
}
