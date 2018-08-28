// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/opengl.h"
#include "fwk/gfx_base.h"

namespace fwk {

class GlQuery {
  public:
	GlQuery();
	~GlQuery();
	GlQuery(const GlQuery &) = delete;
	void operator=(const GlQuery &) = delete;

	Maybe<int> getValue() const;
	int waitForValue() const;

	uint handle;
	bool has_value = false;
};

DEFINE_ENUM(GlQueryType, samples_passed, any_samples_passed, any_samples_passed_conservative,
			primitives_generated, tf_primitives_written, time_elapsed);

struct GlQueryScope {
	GlQueryScope(GlQuery &query, GlQueryType);
	GlQueryScope(const GlQueryScope &) = delete;
	void operator=(const GlQueryScope &) = delete;
	~GlQueryScope();

	GlQueryType type;
};

struct GlTimeScope : public GlQueryScope {
	GlTimeScope(GlQuery &query) : GlQueryScope(query, GlQueryType::time_elapsed) {}
};
}
