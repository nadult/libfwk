// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/algorithm.h"
#include "fwk/enum.h"
#include "fwk/gfx_base.h"
#include "fwk/math_base.h"
#include "fwk/str.h"
#include "fwk/vector.h"
#include "fwk/vulkan_base.h"

namespace fwk {

// To use shader debug:
// - In shader:
//   predefine macro: DEBUG_ENABLED
//   #include "%shader_debug"
// 	 DEBUG_SETUP(buffer_set_id, buffer_binding_id)
// 	 DEBUG_RECORD(int0, int1, float2, uint3);
//
// - Enable shader_debug in pipeline setup
// - Init and use shaderDebug buffer with these functions:

void shaderDebugInitBuffer(VulkanCommandQueue &, VBufferSpan<u32>);

struct ShaderDebugHeader {
	uint max_records, num_records;
	uint workgroup_size[3];
	uint num_workgroups[3];
};

DEFINE_ENUM(ShaderDebugValueType, vt_int, vt_uint, vt_float);

struct ShaderDebugRecord {
	int line_id, file_id;
	uint local_index, work_group_index;
	uint values[4];
	ShaderDebugValueType value_types[4];

	explicit operator bool() const { return anyOf(values); }
	bool operator<(const ShaderDebugRecord &) const;
};

struct ShaderDebugInfo {
	ShaderDebugInfo() = default;
	ShaderDebugInfo(CSpan<u32> buffer_data, Maybe<uint> limit = none,
					CSpan<Pair<string, int>> source_ranges = {});

	int3 localIndexToID(uint) const;
	int3 workGroupIndexToID(uint) const;

	void operator>>(TextFormatter &) const;
	explicit operator bool() const { return !records.empty(); }

	// Formatting data
	int local_id_size, work_group_id_size;
	int3 local_id_width, work_group_id_width;

	int3 work_group_size;
	int3 num_work_groups;
	vector<string> file_names;
	vector<ShaderDebugRecord> records;
};
}