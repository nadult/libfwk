// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/gfx/gl_ref.h"
#include "fwk/gfx_base.h"

namespace fwk {

DEFINE_ENUM(GlQueryType, samples_passed, any_samples_passed, any_samples_passed_conservative,
			primitives_generated, tf_primitives_written, time_elapsed);

class GlQuery {
	GL_CLASS_DECL(GlQuery)
  public:
	using Type = GlQueryType;
	static PQuery make();

	Maybe<i64> getValue() const;
	i64 waitForValue() const;

	Maybe<Type> type() const { return m_type; }
	void begin(Type);
	void end();
	static void end(Type);

  private:
	Maybe<Type> m_type;
	friend struct GlQueryScope;
};

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
