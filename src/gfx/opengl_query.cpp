// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/opengl_query.h"

#include "fwk/enum_map.h"

namespace fwk {

OpenglQuery::OpenglQuery() { glGenQueries(1, &handle); }
OpenglQuery::~OpenglQuery() { glDeleteQueries(1, &handle); }

Maybe<int> OpenglQuery::getValue() const {
	if(!has_value)
		return none;

	int value = -1;
	glGetQueryObjectiv(handle, GL_QUERY_RESULT_NO_WAIT, &value);
	return value == -1 ? Maybe<int>() : value;
}

int OpenglQuery::waitForValue() const {
	if(!has_value)
		return -1;
	int value = -1;
	glGetQueryObjectiv(handle, GL_QUERY_RESULT, &value);
	return value;
}

static const EnumMap<OpenglQueryType, int> s_query_types{
	{GL_SAMPLES_PASSED, GL_ANY_SAMPLES_PASSED, GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
	 GL_PRIMITIVES_GENERATED, GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, GL_TIME_ELAPSED}};

OpenglQueryScope::OpenglQueryScope(OpenglQuery &query, OpenglQueryType type) : type(type) {
	glBeginQuery(s_query_types[type], query.handle);
	query.has_value = true;
}
OpenglQueryScope::~OpenglQueryScope() { glEndQuery(s_query_types[type]); }
}
