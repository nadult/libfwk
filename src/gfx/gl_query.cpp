// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/gl_query.h"

#include "fwk/enum_map.h"
#include "fwk/gfx/opengl.h"
#include "fwk/sys/assert.h"

namespace fwk {

GL_CLASS_IMPL(GlQuery)

PQuery GlQuery::make() { return PQuery(storage.make()); }

Maybe<int> GlQuery::getValue() const {
	if(!m_type)
		return none;

	int value = -1;
	glGetQueryObjectiv(id(), GL_QUERY_RESULT_NO_WAIT, &value);
	return value == -1 ? Maybe<int>() : value;
}

int GlQuery::waitForValue() const {
	if(!m_type)
		return -1;
	int value = -1;
	glGetQueryObjectiv(id(), GL_QUERY_RESULT, &value);
	return value;
}

static const EnumMap<GlQueryType, int> s_query_types{
	{GL_SAMPLES_PASSED, GL_ANY_SAMPLES_PASSED, GL_ANY_SAMPLES_PASSED_CONSERVATIVE,
	 GL_PRIMITIVES_GENERATED, GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN, GL_TIME_ELAPSED}};

void GlQuery::begin(Type type) {
	if(m_type)
		DASSERT_EQ(type, *m_type);
	m_type = type;
	glBeginQuery(s_query_types[type], id());
}

void GlQuery::end() {
	PASSERT(m_type);
	glEndQuery(s_query_types[*m_type]);
}

void GlQuery::end(Type type) { glEndQuery(s_query_types[type]); }

GlQueryScope::GlQueryScope(PQuery query, GlQueryType type) : type(type) {
	DASSERT(query);
	query->begin(type);
}
GlQueryScope::~GlQueryScope() { GlQuery::end(type); }
}
