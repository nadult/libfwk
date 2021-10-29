// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/perf_base.h"

#include "fwk/format.h"

namespace perf {

FuncName::FuncName(Str full) {
	int pos = 0, num_paren = 0;

	while(pos < full.size()) {
		auto c = full[pos++];
		if(isOneOf(c, '(', '<', '['))
			num_paren++;
		else if(isOneOf(c, ')', '>', ']'))
			num_paren--;
		if(isspace(c) && num_paren == 0)
			break;
	}

	return_type = {full.data(), pos};

	while(pos < full.size() && isspace(full[pos]))
		pos++;

	int name_start = pos;
	if(name_start == full.size()) {
		return_type = {};
		name_start = pos = 0;
	}

	while(pos < full.size() && full[pos] != '(')
		pos++;
	name = full.substr(name_start, pos - name_start);
	args = full.substr(pos);
}

Str parseTag(Str input) {
	int hash_pos = input.find("#"), last_pos = input.size() - 1;
	if(hash_pos == -1)
		return input;
	return hash_pos == last_pos ? input.substr(0, hash_pos) : input.substr(hash_pos + 1);
}

static vector<PointInfo> *s_points = nullptr;

PointId registerPoint(PointType type, const char *file, const char *func, const char *tag,
					  int line) {
	static vector<PointInfo> points;
	if(points.empty())
		points.emplace_back(PointInfo());

	DASSERT(points.size() < PointId::max);
	PointId out(points.size());
	points.emplace_back(FuncName(func), Str(file), parseTag(tag), line, type);
	s_points = &points;
	return out;
}

int numPoints() { return s_points ? s_points->size() : 0; }

const PointInfo *pointInfo(PointId id) {
	if(!s_points)
		return nullptr;

	DASSERT(id > 0 && id < s_points->size());
	if(id < s_points->size())
		return &(*s_points)[id];
	return nullptr;
}

template <class Id> void Sample<Id>::operator>>(TextFormatter &fmt) const {
	fmt(fmt.isPlain() ? "% % %" : "%:%=%", id(), type(), value());
}
template void Sample<PointId>::operator>>(TextFormatter &) const;
template void Sample<ExecId>::operator>>(TextFormatter &) const;
}
