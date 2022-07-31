// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/gfx/shader_debug.h"

#include "fwk/format.h"

// TODO: fixme
/*

namespace fwk {

using ValueType = ShaderDebugValueType;
static constexpr int debug_header_size = 8;

static const char *debug_funcs =
	R"(// ----------------- Shader Debug -----------------------
uint debugValueType(int v) { return 0; }
uint debugValueType(uint v) { return 1; }
uint debugValueType(float v) { return 2; }
void debugRecord(uint record_id, int line, uint v1, uint v2, uint v3, uint v4, uint types) {
  if(record_id < debug_max_records) {
    uint wg_index = gl_WorkGroupID.x + gl_NumWorkGroups.x * 
                    (gl_WorkGroupID.y + gl_NumWorkGroups.y * gl_WorkGroupID.z);
    debug_records[record_id * 3 + 0] = uvec2(gl_LocalInvocationIndex | (line << 16), wg_index | (types << 24));
    debug_records[record_id * 3 + 1] = uvec2(v1, v2);
    debug_records[record_id * 3 + 2] = uvec2(v3, v4);
  }
}
#define RECORD_(line, v1, v2, v3, v4) {               \
    uint record_id = atomicAdd(debug_num_records, 1); \
    if(record_id == 0) for(int i = 0; i < 3; i++) {   \
      debug_sizes[i] = gl_WorkGroupSize[i];           \
      debug_sizes[i + 3] = gl_NumWorkGroups[i];       \
    }                                                 \
    const uint types = (debugValueType(v1) << 0) | (debugValueType(v2) << 2) | \
                       (debugValueType(v3) << 4) | (debugValueType(v4) << 6);  \
    uint uv1 = ((types >> 0) & 3) == 2? floatBitsToUint(v1) : uint(v1);        \
    uint uv2 = ((types >> 2) & 3) == 2? floatBitsToUint(v2) : uint(v2);        \
    uint uv3 = ((types >> 4) & 3) == 2? floatBitsToUint(v3) : uint(v3);        \
    uint uv4 = ((types >> 6) & 3) == 2? floatBitsToUint(v4) : uint(v4);        \
    debugRecord(record_id, line, uv1, uv2, uv3, uv4, types);                   \
  }
#define RECORD(v1, v2, v3, v4) RECORD_(__LINE__, v1, v2, v3, v4)
#define SHADER_DEBUG)";

string shaderDebugDefs(bool enable) {
	TextFormatter fmt;
	if(enable) {
		fmt("layout(std430, binding = %) buffer debug_records_ {\n",
			gl_info->limits[GlLimit::max_ssbo_bindings] - 1);
		fmt("  uint debug_num_records;\n"
			"  uint debug_max_records;\n"
			"  uint debug_sizes[6];\n"
			"  uvec2 debug_records[];\n"
			"};\n");
		fmt << debug_funcs;
	} else {
		fmt("#define RECORD(v1, v2, v3, v4) {}\n");
	}

	return fmt.text();
}

void shaderDebugUseBuffer(PBuffer buf) {
	int size = buf->size<u32>();
	DASSERT(size >= debug_header_size);
	buf->clear(GlFormat::r32ui, 0, 0, 4);
	buf->clear(GlFormat::r32ui, (size - debug_header_size) / 6, 4, 4);
	// TODO: wrong number here does not generate an error
	buf->bindIndex(gl_info->limits[GlLimit::max_ssbo_bindings] - 1);
	testGlError("shader debug stuff");
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

		fmt(" work_group_id:");
		printID(fmt, global_id, work_group_id_size, work_group_id_width);

		fmt(" local_id:");
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

ShaderDebugInfo::ShaderDebugInfo(PBuffer buf, Maybe<uint> limit,
								 CSpan<Pair<string, int>> source_ranges) {
	auto header = buf->download<u32>(debug_header_size, 0);
	u32 num_records = min<u32>(header[0], (buf->size<u32>() - debug_header_size) / 6);
	work_group_size = int3(header[2], header[3], header[4]);
	num_work_groups = int3(header[5], header[6], header[7]);
	local_id_size = work_group_size.yz() == int2(1) ? 1 : work_group_size.z == 1 ? 2 : 3;
	work_group_id_size = num_work_groups.yz() == int2(1) ? 1 : num_work_groups.z == 1 ? 2 : 3;
	for(int i = 0; i < 3; i++) {
		local_id_width[i] = log10(work_group_size[i]) + 1;
		work_group_id_width[i] = log10(num_work_groups[i]) + 1;
	}

	if(limit)
		num_records = min(num_records, *limit);
	auto values = buf->download<u32>(num_records * 6, debug_header_size);
	records.reserve(num_records);
	file_names.reserve(source_ranges.size());
	for(auto &label : source_ranges)
		file_names.emplace_back(label.first);

	for(uint i = 0; i < num_records; i++) {
		const u32 *cur = &values[i * 6];
		uint lix = values[i * 6 + 0] & 0xffff, line = values[i * 6 + 0] >> 16;
		uint gix = values[i * 6 + 1] & 0xffffff, types = values[i * 6 + 1] >> 24;

		int file_id = -1;
		if(auto label_id = CombinedShaderSource::lineToLabel(source_ranges, line); label_id != -1) {
			auto &label = source_ranges[label_id];
			file_id = label_id;
			line = line - label.second + 1;
		}

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

	makeSorted(records);
}
}
*/