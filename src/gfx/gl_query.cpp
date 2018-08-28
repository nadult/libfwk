// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_query.h"

#include "fwk/enum_map.h"

namespace fwk {

GlQuery::GlQuery() { glGenQueries(1, &handle); }
GlQuery::~GlQuery() { glDeleteQueries(1, &handle); }

Maybe<int> GlQuery::getValue() const {
	if(!has_value)
		return none;

	int value = -1;
	glGetQueryObjectiv(handle, GL_QUERY_RESULT_NO_WAIT, &value);
	return value == -1 ? Maybe<int>() : value;
}

int GlQuery::waitForValue() const {
	if(!has_value)
		return -1;
	int value = -1;
	glGetQueryObjectiv(handle, GL_QUERY_RESULT, &value);
	return value;
}

static const EnumMap<GlQueryType, int> s_query_types{
	{GL_SAMPLES_PASSED, GL_ANY_SAMPLES_PASSED, GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
	 GL_PRIMITIVES_GENERATED, GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, GL_TIME_ELAPSED}};

GlQueryScope::GlQueryScope(GlQuery &query, GlQueryType type) : type(type) {
	glBeginQuery(s_query_types[type], query.handle);
	query.has_value = true;
}
GlQueryScope::~GlQueryScope() { glEndQuery(s_query_types[type]); }
}
