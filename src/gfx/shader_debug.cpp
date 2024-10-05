// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/shader_debug.h"

#include "fwk/format.h"
#include "fwk/vulkan/vulkan_buffer_span.h"
#include "fwk/vulkan/vulkan_command_queue.h"

namespace fwk {

using ValueType = ShaderDebugValueType;

static const ZStr debug_code = R"(// Shader debug:

#ifdef DEBUG_ENABLED
#define DEBUG_SETUP(buffer_set_id, buffer_binding_id) \
struct ShaderDebugHeader {                            \
	uint max_records, num_records;                    \
	uint workgroup_size[3];                           \
	uint num_workgroups[3];                           \
};                                                    \
                                                      \
layout(std430, set = buffer_set_id, binding = buffer_binding_id) \
restrict buffer _debug_buffer {            \
	ShaderDebugHeader _debug_header;       \
	uvec2 _debug_records[];                \
};                                         \
uint debugValueType(int   v) { return 0; } \
uint debugValueType(uint  v) { return 1; } \
uint debugValueType(float v) { return 2; } \
void debugRecord(uint record_id, int line, uint v1, uint v2, uint v3, uint v4, uint types) { \
  if(record_id >= _debug_header.max_records)                                                 \
    return;                                                                                  \
  uint wg_index = gl_WorkGroupID.x + gl_NumWorkGroups.x *                                    \
                  (gl_WorkGroupID.y + gl_NumWorkGroups.y * gl_WorkGroupID.z);                \
  uint val0 = gl_LocalInvocationIndex | (line << 16), val1 = wg_index | (types << 24);       \
  _debug_records[record_id * 3 + 0] = uvec2(val0, val1);                                     \
  _debug_records[record_id * 3 + 1] = uvec2(v1, v2);                                         \
  _debug_records[record_id * 3 + 2] = uvec2(v3, v4);                                         \
}

#define DEBUG_RECORD(v1, v2, v3, v4) {                                    \
    uint record_id = atomicAdd(_debug_header.num_records, 1);             \
    if(record_id == 0) for(int i = 0; i < 3; i++) {                       \
      _debug_header.workgroup_size[i] = gl_WorkGroupSize[i];              \
      _debug_header.num_workgroups[i] = gl_NumWorkGroups[i];              \
    }                                                                     \
    uint types = (debugValueType(v1) << 0) | (debugValueType(v2) << 2) |  \
                 (debugValueType(v3) << 4) | (debugValueType(v4) << 6);   \
    uint uv1 = ((types >> 0) & 3) == 2? floatBitsToUint(v1) : uint(v1);   \
    uint uv2 = ((types >> 2) & 3) == 2? floatBitsToUint(v2) : uint(v2);   \
    uint uv3 = ((types >> 4) & 3) == 2? floatBitsToUint(v3) : uint(v3);   \
    uint uv4 = ((types >> 6) & 3) == 2? floatBitsToUint(v4) : uint(v4);   \
    debugRecord(record_id, __LINE__, uv1, uv2, uv3, uv4, types);          \
  }
#else
#define DEBUG_SETUP(buffer_set_id, buffer_binding_id)
#define DEBUG_RECORD(v1, v2, v3, v4)
#endif
)";

ZStr getShaderDebugCode() { return debug_code; }

void shaderDebugInitBuffer(VulkanCommandQueue &cmds, VBufferSpan<u32> buf) {
	auto header_size = sizeof(ShaderDebugHeader) / sizeof(u32);
	DASSERT(buf.size() >= header_size);
	auto max_records = buf.size() - header_size;
	cmds.fill(buf.subSpan(0, 1), max_records);
	cmds.fill(buf.subSpan(1, header_size), 0);
	cmds.barrier(VPipeStage::transfer, VPipeStage::compute_shader, VAccess::transfer_write,
				 VAccess::memory_read | VAccess::memory_write);
}

bool ShaderDebugRecord::operator<(const ShaderDebugRecord &rhs) const {
	return tie(file_id, line_id, work_group_index, local_index) <
		   tie(rhs.file_id, rhs.line_id, rhs.work_group_index, rhs.local_index);
}

int3 ShaderDebugInfo::localIndexToID(uint idx) const {
	int x = idx % work_group_size.x;
	int z = idx / (work_group_size.x * work_group_size.y);
	int y = idx / work_group_size.x - z * work_group_size.y;
	return {x, y, z};
}

int3 ShaderDebugInfo::workGroupIndexToID(uint idx) const {
	int x = idx % num_work_groups.x;
	int z = idx / (num_work_groups.x * num_work_groups.y);
	int y = idx / num_work_groups.x - z * num_work_groups.y;
	return {x, y, z};
}

static void printInt(TextFormatter &fmt, int value, int width) {
	int pos = fmt.size();
	fmt << value;
	width -= fmt.size() - pos;
	for(int i = 0; i < width; i++)
		fmt << ' ';
}

static void printID(TextFormatter &fmt, int3 id, int size, int3 width) {
	if(size > 1)
		fmt << '(';
	for(int i = 0; i < size; i++) {
		printInt(fmt, id[i], width[i]);
		if(i + 1 < size)
			fmt << ',';
	}
	if(size > 1)
		fmt << ')';
}

void ShaderDebugInfo::operator>>(TextFormatter &fmt) const {
	fmt("local_size:");
	printID(fmt, work_group_size, local_id_size, local_id_width);
	fmt(" num_work_groups:");
	printID(fmt, num_work_groups, work_group_id_size, work_group_id_width);
	fmt(" num_records:%\n", records.size());

	for(auto &record : records) {
		if(record.file_id != -1)
			fmt.stdFormat("%s:%04i", file_names[record.file_id].c_str(), record.line_id);
		else
			fmt.stdFormat("%4i", record.line_id);

		auto local_id = localIndexToID(record.local_index);
		auto global_id = workGroupIndexToID(record.work_group_index);

		fmt(" wgid:");
		printID(fmt, global_id, work_group_id_size, work_group_id_width);

		fmt(" lid:");
		printID(fmt, local_id, local_id_size, local_id_width);
		fmt("  ");

		for(int i = 0; i < 4; i++) {
			auto value = record.values[i];
			auto type = record.value_types[i];
			fmt << ' ';
			switch(type) {
			case ValueType::vt_uint:
				fmt << value;
				break;
			case ValueType::vt_int:
				fmt << int(value);
				break;
			case ValueType::vt_float:
				fmt << *reinterpret_cast<float *>(&value);
				break;
			}
		}
		fmt << "\n";
	}
	fmt << "\n";
}

ShaderDebugInfo::ShaderDebugInfo(CSpan<u32> buffer_data, Maybe<uint> limit,
								 CSpan<Pair<string, int>> source_ranges) {
	int header_size = sizeof(ShaderDebugHeader) / sizeof(u32);
	DASSERT(buffer_data.size() >= header_size);
	auto &header = *reinterpret_cast<const ShaderDebugHeader *>(buffer_data.data());
	u32 num_records = min<u32>(header.num_records, (buffer_data.size() - header_size) / 6);
	work_group_size =
		int3(header.workgroup_size[0], header.workgroup_size[1], header.workgroup_size[2]);
	num_work_groups =
		int3(header.num_workgroups[0], header.num_workgroups[1], header.num_workgroups[2]);
	local_id_size = work_group_size.yz() == int2(1) ? 1 : work_group_size.z == 1 ? 2 : 3;
	work_group_id_size = num_work_groups.yz() == int2(1) ? 1 : num_work_groups.z == 1 ? 2 : 3;
	for(int i = 0; i < 3; i++) {
		local_id_width[i] = max(1.0, log10(work_group_size[i]) + 1);
		work_group_id_width[i] = max(1.0, log10(num_work_groups[i]) + 1);
	}

	if(limit)
		num_records = min(num_records, *limit);
	auto values = buffer_data.subSpan(header_size, header_size + num_records * 6);
	records.reserve(num_records);
	file_names.reserve(source_ranges.size());
	for(auto &label : source_ranges)
		file_names.emplace_back(label.first);

	for(uint i = 0; i < num_records; i++) {
		const u32 *cur = &values[i * 6];
		uint lix = values[i * 6 + 0] & 0xffff, line = values[i * 6 + 0] >> 16;
		uint gix = values[i * 6 + 1] & 0xffffff, types = values[i * 6 + 1] >> 24;

		int file_id = -1;
		/*if(auto label_id = CombinedShaderSource::lineToLabel(source_ranges, line); label_id != -1) {
			auto &label = source_ranges[label_id];
			file_id = label_id;
			line = line - label.second + 1;
		}*/

		ShaderDebugRecord record;
		record.line_id = line;
		record.file_id = file_id;
		record.local_index = lix;
		record.work_group_index = gix;

		for(int j = 0; j < 4; j++) {
			uint vt = (types >> (j * 2)) & 3;
			record.values[j] = values[i * 6 + 2 + j];
			record.value_types[j] = vt == 0 ? ValueType::vt_int :
									vt == 2 ? ValueType::vt_float :
											  ValueType::vt_uint;
		}

		records.emplace_back(record);
	}
}
}