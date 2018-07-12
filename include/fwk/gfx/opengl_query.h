// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/opengl.h"
#include "fwk/gfx_base.h"

namespace fwk {

struct OpenglQuery {
	OpenglQuery();
	~OpenglQuery();
	OpenglQuery(const OpenglQuery &) = delete;
	void operator=(const OpenglQuery &) = delete;

	Maybe<int> getValue() const;
	int waitForValue() const;

	uint handle;
	bool has_value = false;
};

DEFINE_ENUM(OpenglQueryType, samples_passed, any_samples_passed, any_samples_passed_conservative,
			primitives_generated, tf_primitives_written, time_elapsed);

struct OpenglQueryScope {
	OpenglQueryScope(OpenglQuery &query, OpenglQueryType);
	OpenglQueryScope(const OpenglQueryScope &) = delete;
	void operator=(const OpenglQueryScope &) = delete;
	~OpenglQueryScope();

	OpenglQueryType type;
};

struct OpenglScopeTime : public OpenglQueryScope {
	OpenglScopeTime(OpenglQuery &query) : OpenglQueryScope(query, OpenglQueryType::time_elapsed) {}
};
}
