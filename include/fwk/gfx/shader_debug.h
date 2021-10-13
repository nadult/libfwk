// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/algorithm.h"
#include "fwk/gfx_base.h"
#include "fwk/math_base.h"

namespace fwk {

string shaderDebugDefs(bool enable_debugging);
void shaderDebugUseBuffer(PBuffer);
void shaderDebugInitBuffer(PBuffer);

struct ShaderDebugRecord {
	ZStr file;
	uint line;
	int3 local_id, global_id;
	uint values[4];

	explicit operator bool() const { return anyOf(values); }
	bool operator<(const ShaderDebugRecord &) const;
	void operator>>(TextFormatter &) const;
};

vector<ShaderDebugRecord> shaderDebugRecords(PBuffer, int3 local_size, int3 global_size,
											 Maybe<uint> limit = none,
											 CSpan<Pair<string, int>> source_ranges = {});

}
