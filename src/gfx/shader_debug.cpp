// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/shader_debug.h"

#include "fwk/format.h"
#include "fwk/gfx/gl_buffer.h"
#include "fwk/gfx/gl_format.h"
#include "fwk/gfx/opengl.h"
#include "fwk/gfx/shader_combiner.h"

namespace fwk {

string shaderDebugDefs(bool enable) {
	TextFormatter fmt;
	if(enable) {
		fmt("layout(std430, binding = %) buffer debug_log_ {\n",
			gl_info->limits[GlLimit::max_ssbo_bindings] - 1);
		fmt("  uint debug_log_counter;\n");
		fmt("  uint debug_buffer_size;\n");
		fmt("  uint debug_logs[];\n");
		fmt("};\n");

		// TODO: turn to function
		fmt("#define RECORD_(line, v1, v2, v3, v4) { \\\n");
		fmt("  uint log_id = atomicAdd(debug_log_counter, 1); \\\n");
		fmt("  if(log_id < debug_buffer_size) { \\\n");
		fmt("    debug_logs[log_id * 6 + 0] = gl_LocalInvocationIndex | (line << 16); \\\n");
		fmt("    debug_logs[log_id * 6 + 1] = gl_WorkGroupID.x + gl_NumWorkGroups.x * "
			"(gl_WorkGroupID.y + gl_NumWorkGroups.y * gl_WorkGroupID.z); \\\n");
		fmt("    debug_logs[log_id * 6 + 2] = v1; \\\n");
		fmt("    debug_logs[log_id * 6 + 3] = v2; \\\n");
		fmt("    debug_logs[log_id * 6 + 4] = v3; \\\n");
		fmt("    debug_logs[log_id * 6 + 5] = v4; \\\n");
		fmt("} }\n");
		fmt("#define RECORD(v1, v2, v3, v4) RECORD_(__LINE__, v1, v2, v3, v4)\n");
		fmt("#define SHADER_DEBUG\n");
	} else {
		fmt("#define RECORD(v1, v2, v3, v4) {}\n");
	}

	return fmt.text();
}

void shaderDebugUseBuffer(PBuffer buf) {
	buf->clear(GlFormat::r32ui, 0, 0, 4);
	buf->clear(GlFormat::r32ui, (buf->size<int>() - 2) / 6, 4, 4);
	// TODO: wrong number here does not generate an error
	buf->bindIndex(gl_info->limits[GlLimit::max_ssbo_bindings] - 1);
	testGlError("shader debug stuff");
}

bool ShaderDebugRecord::operator<(const ShaderDebugRecord &rhs) const {
	return tie(line, global_id, local_id) < tie(rhs.line, rhs.global_id, rhs.local_id);
}

void ShaderDebugRecord::operator>>(TextFormatter &out) const {
	out.stdFormat("%s:%3i Local:[%2d,%2d,%2d] Global:[%2d,%2d,%2d] | %u %u %u %u", file.c_str(),
				  line, local_id[0], local_id[1], local_id[2], global_id[0], global_id[1],
				  global_id[2], values[0], values[1], values[2], values[3]);
}

vector<ShaderDebugRecord> shaderDebugRecords(PBuffer buf, int3 lsize, int3 gsize, Maybe<uint> limit,
											 CSpan<Pair<string, int>> source_ranges) {
	auto values = buf->download<uint>();
	uint count = values.front();
	// TODO: download data only if count > 0

	vector<ShaderDebugRecord> out;
	out.reserve(values.size() / 6);

	count = min(count, (uint)max(values.size() - 2, 0) / 6);
	if(limit)
		count = min(count, *limit);

	for(uint n = 0; n < count; n++) {
		const uint *cur = &values[n * 6 + 2];
		uint lix = cur[0] & 0xffff, line = cur[0] >> 16, gix = cur[1];

		int3 lid, gid;
		lid.x = lix % lsize.x;
		gid.x = gix % gsize.x;
		lid.z = lix / (lsize.x * lsize.y);
		gid.z = gix / (gsize.x * gsize.y);
		lid.y = (lix / lsize.x) - lid.z * lsize.y;
		gid.y = (gix / gsize.x) - gid.z * gsize.y;

		ZStr file;
		if(auto label_id = CombinedShaderSource::lineToLabel(source_ranges, line); label_id != -1) {
			auto &label = source_ranges[label_id];
			file = label.first;
			line = line - label.second + 1;
		}

		// TODO: jakaś wizualizacja, które wątki wywalają się na jakim IDku?
		out.emplace_back(ShaderDebugRecord{file, line, lid, gid, {cur[2], cur[3], cur[4], cur[5]}});
	}

	return out;
}
}
