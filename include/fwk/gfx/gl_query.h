// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/gl_ref.h"
#include "fwk/gfx_base.h"

namespace fwk {

class GlQuery {
	GL_CLASS_DECL(GlQuery)
  public:
	static PQuery make();

	Maybe<int> getValue() const;
	int waitForValue() const;

  private:
	bool m_has_value = false;
	friend struct GlQueryScope;
};

DEFINE_ENUM(GlQueryType, samples_passed, any_samples_passed, any_samples_passed_conservative,
			primitives_generated, tf_primitives_written, time_elapsed);

struct GlQueryScope {
	GlQueryScope(PQuery, GlQueryType);
	GlQueryScope(const GlQueryScope &) = delete;
	void operator=(const GlQueryScope &) = delete;
	~GlQueryScope();

	GlQueryType type;
};

struct GlTimeScope : public GlQueryScope {
	GlTimeScope(PQuery query) : GlQueryScope(query, GlQueryType::time_elapsed) {}
};
}
