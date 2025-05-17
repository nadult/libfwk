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
// 1. Enable fragmentStoresAndAtomics or vertexPipelineStoresAndAtomics device features
//    if you want to use the debugging feature in the fragment or vertex pipeline shaders.
// 2. Inside shader:
//    predefine macro: DEBUG_ENABLED
// 	  for compute shaders predefine: DEBUG_COMPUTE
//
//    #include "%shader_debug"
// 	  DEBUG_SETUP(buffer_set_id, buffer_binding_id)
// 	  DEBUG_RECORD(int0, int1, float2, uint3);
// 3. Create debug buffer with shaderDebugBuffer() right before shader is run (outside render pass)
// 4. Run shader
// 5. Retrieve results with shaderDebugDownloadResults()

struct ShaderDebugHeader {
	uint max_records, num_records;
	uint workgroup_size[3];
	uint num_workgroups[3];
};

DEFINE_ENUM(ShaderDebugValueType, vt_int, vt_uint, vt_float);

struct ShaderDebugRecord {
	int line_id;
	uint local_index, work_group_index;
	uint values[4];
	ShaderDebugValueType value_types[4];

	explicit operator bool() const { return anyOf(values); }
	bool operator<(const ShaderDebugRecord &) const;
};

struct ShaderDebugResults {
	ShaderDebugResults() = default;
	ShaderDebugResults(string title, CSpan<u32> buffer_data, Maybe<uint> limit = none);

	int3 localIndexToID(uint) const;
	int3 workGroupIndexToID(uint) const;

	void operator>>(TextFormatter &) const;
	explicit operator bool() const { return !records.empty(); }

	string title;

	bool compute_mode;
	int local_id_size, work_group_id_size;
	int3 local_id_width, work_group_id_width;
	int3 work_group_size, num_work_groups;

	vector<ShaderDebugRecord> records;
};

Ex<VBufferSpan<u32>> shaderDebugBuffer(VulkanDevice &, uint size_bytes = 4 * 1024 * 1024,
									   VMemoryUsage = VMemoryUsage::frame);
void shaderDebugResetBuffer(VulkanCommandQueue &, VBufferSpan<u32>);

// Downloads data from GPU and analyses them. skip_frames is used to not retrieve data every frame,
// but every N frames. title is used both for naming debug results and for labelling GPU downloads.
Maybe<ShaderDebugResults> shaderDebugDownloadResults(VulkanCommandQueue &cmds, VBufferSpan<u32> src,
													 Str title, uint skip_frames = 32);

}