// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/perf/analyzer.h"

#include "fwk/any_config.h"
#include "fwk/enum_map.h"
#include "fwk/index_range.h"
#include "fwk/math/interval.h"
#include "fwk/menu/helpers.h"
#include "fwk/menu_imgui_internal.h"
#include "fwk/perf/exec_tree.h"
#include "fwk/perf/manager.h"
#include "fwk/perf/thread_context.h"

namespace perf {

ColumnFlags gpuColumns() { return ColumnId::gpu_avg | ColumnId::gpu_max | ColumnId::gpu_min; }
ColumnFlags cpuColumns() { return ColumnId::cpu_avg | ColumnId::cpu_max | ColumnId::cpu_min; }

static Analyzer *s_instance = nullptr;

Analyzer *Analyzer::instance() { return s_instance; }

static int countBits(uint value) {
	uint bit = 1;
	int out = 0;

	while(bit != 0x80000000) {
		if(bit & value)
			out++;
		bit = bit << 1;
	}
	return out;
}

Analyzer::Analyzer()
	: m_manager((ASSERT(Manager::instance()), *Manager::instance())),
	  m_exec_tree(m_manager.execTree()) {
	DASSERT(!s_instance && "Only one instance of Analyzer is allowed");
	s_instance = this;
	m_visible_columns = ColumnId::name | ColumnId::cpu_avg | ColumnId::gpu_avg;
	m_set_menu_rect = IRect(10, 380, 350, 650);
}

Analyzer::~Analyzer() { s_instance = nullptr; }

void Analyzer::doMenu(bool &is_enabled) {
	PERF_SCOPE();

	if(m_set_menu_rect) {
		ImGui::SetNextWindowPos(m_set_menu_rect->min());
		ImGui::SetNextWindowSize(m_set_menu_rect->size());
		m_set_menu_rect = none;
	}

	ImGui::Begin("Performance analyzer", &is_enabled, ImGuiWindowFlags_NoScrollbar);
	auto frames = cspan(m_manager.frames());
	if(!frames) {
		menu::text("No frames\n");
		ImGui::End();
		return;
	}

	CSpan<Frame> sel_frames = getFrames();

	computeRange(m_range, sel_frames);
	showGrid(m_range);

	ImGui::End();

	// Updating selected exec
	if(m_selected_exec_hash) {
		if(m_selected_exec)
			m_selected_exec_hash = none;
		else
			for(int n : intRange(m_range.opened))
				if(m_exec_tree.hashedId(ExecId(n)) == *m_selected_exec_hash) {
					m_selected_exec = ExecId(n);
					m_selected_exec_hash = none;
					break;
				}
	}
}

void Analyzer::computeRange(FrameRange &out, CSpan<Frame> frames) {
	static constexpr auto max_value = std::numeric_limits<i64>::max();
	fill(out.minimum, max_value);
	fill(out.maximum, 0);
	fill(out.average, 0);

	out.minimum.resize(m_exec_tree.size(), max_value);
	out.maximum.resize(m_exec_tree.size(), 0);
	out.average.resize(m_exec_tree.size(), 0);

	vector<Row> rows(m_exec_tree.size());

	vector<ExecTree::ExecValue> values(m_exec_tree.size());
	for(auto &frame : frames) {
		for(auto &value : values)
			value.value = 0;
		m_exec_tree.getExecValues(frame.samples, values);
		values[0].value = (frame.end_time - frame.start_time) * 1000000000;

		for(int n : intRange(values)) {
			auto value = values[n].value;
			out.average[n] += value;
			out.minimum[n] = min(out.minimum[n], value);
			out.maximum[n] = max(out.maximum[n], value);
		}
	}

	for(ExecId eid : indexRange<ExecId>(values)) {
		auto &exec = m_exec_tree[eid];
		int num_inst = values[eid].num_instances;
		if(exec.type == ExecNodeType::scope && num_inst) {
			rows[eid].num_instances = num_inst;
			rows[eid].exec_order = values[eid].begin_time / num_inst;
		}
	}

	if(!out.opened) {
		out.opened.resize(m_exec_tree.size(), false);
		for(int n : intRange(out.opened))
			out.opened[n] = m_exec_tree[ExecId(n)].depth <= 3;
	}
	out.opened.resize(m_exec_tree.size(), false);
	updateOpenedNodes();

	out.empty = m_exec_tree.emptyBranches(out.average);
	out.num_frames = frames.size();

	for(int n : intRange(values)) {
		if(out.minimum[n] == max_value)
			out.minimum[n] = 0;
		out.average[n] /= out.num_frames;
	}

	computeRows(m_exec_tree.root(), rows, out);
	out.rows = move(rows);
	computeExecList(out);
}

string Analyzer::dump(const Frame &frame) const {
	TextFormatter fmt;
	fmt("Frame %: % ms\n", frame.frame_id, (frame.end_time - frame.start_time) * 1000.0);
	int indent = 0;
	u64 last_time = 0;

	for(auto &sample : frame.samples) {
		for(int i : intRange(indent))
			fmt("  ");
		auto &exec = m_exec_tree[sample.id()];
		auto *point = pointInfo(exec.point_id);

		fmt("% %: %[%]: %", point->func.name, point->tag, sample.type(), sample.id(),
			sample.value());

		if(isOneOf(sample.type(), SampleType::scope_end, SampleType::scope_begin)) {
			if(sample.value() < last_time)
				fmt("[INVALID TIME]");
			last_time = sample.value();
			indent += sample.type() == SampleType::scope_end ? -1 : 1;
		}

		fmt("\n");
	}
	return fmt.text();
}

void Analyzer::computeRows(ExecId exec_id, vector<Row> &rows, const FrameRange &range) const {
	auto &exec = m_exec_tree[exec_id];
	auto &row = rows[exec_id];

	for(auto child_id : exec.children)
		computeRows(child_id, rows, range);

	if(exec.type == ExecNodeType::scope) {
		if(exec.gpu_time_id) {
			row.gpu_avg = range.average[exec.gpu_time_id];
			row.gpu_max = range.maximum[exec.gpu_time_id];
			row.gpu_min = range.minimum[exec.gpu_time_id];
		} else {
			for(auto child_id : exec.children) {
				row.gpu_avg += rows[child_id].gpu_avg;
				row.gpu_min += rows[child_id].gpu_min;
				row.gpu_max += rows[child_id].gpu_max;
			}
		}

		row.cpu_avg = range.average[exec_id];
		row.cpu_max = range.maximum[exec_id];
		row.cpu_min = range.minimum[exec_id];
	} else if(exec.type == ExecNodeType::counter) {
		row.cnt_avg = range.average[exec_id];
		row.cnt_min = range.minimum[exec_id];
		row.cnt_max = range.maximum[exec_id];
		row.exec_order = ~0ull;
	}
}

Str Analyzer::execName(ExecId exec_id) const {
	if(!exec_id)
		return "Frame";

	auto *point = pointInfo(m_exec_tree[exec_id].point_id);
	DASSERT(point);
	return point->tag ? point->tag : point->func.name;
}

string Analyzer::execInfo(ExecId exec_id) const {
	if(!exec_id)
		return {};

	auto *point = pointInfo(m_exec_tree[exec_id].point_id);
	auto tag = point->tag ? format("[%]", point->tag) : string();
	return format("% %%%\nLocation: %:%", point->func.return_type, point->func.name,
				  point->func.args, tag, point->file, point->line);
}

void Analyzer::sortRows(Span<ExecId> ids, CSpan<Row> rows) const {
	if(m_sort_var == SortVar::name) {
		std::sort(begin(ids), end(ids),
				  [&](ExecId a, ExecId b) { return execName(a) < execName(b); });
	} else if(m_sort_var == SortVar::execution) {
		std::sort(begin(ids), end(ids),
				  [&](ExecId a, ExecId b) { return rows[a].exec_order < rows[b].exec_order; });
	} else {
		int var_id = int(m_sort_var) - 2;
		std::sort(begin(ids), end(ids), [&](ExecId a, ExecId b) {
			return rows[a].values[var_id] < rows[b].values[var_id];
		});
	}

	if(m_sort_inverse)
		std::reverse(begin(ids), end(ids));
}

void Analyzer::computeExecList(ExecId exec_id, FrameRange &range) const {
	auto &exec = m_exec_tree[exec_id];
	auto &row = range.rows[exec_id];

	if(exec.type == ExecNodeType::gpu_time || (!m_show_empty && range.empty[exec_id]))
		return;

	range.exec_list.emplace_back(exec_id);

	for(auto child_id : exec.children)
		if(m_exec_tree[child_id].type != ExecNodeType::gpu_time) {
			range.has_children[exec_id] = true;
			if(m_show_empty || !range.empty[child_id]) {
				range.has_active_children[exec_id] = true;
				break;
			}
		}

	if(range.opened[exec_id]) {
		auto children = exec.children;
		sortRows(children, range.rows);
		for(auto child_id : children)
			computeExecList(child_id, range);
	}
}

void Analyzer::computeExecList(FrameRange &range) const {
	range.exec_list.clear();
	range.has_children.clear();
	range.has_active_children.clear();
	range.has_children.resize(m_exec_tree.size(), false);
	range.has_active_children.resize(m_exec_tree.size(), false);

	computeExecList(m_exec_tree.root(), range);
}

static const EnumMap<SampleType, int> sample_priority = {{{SampleType::scope_begin, 0},
														  {SampleType::gpu_time, 1},
														  {SampleType::counter, 2},
														  {SampleType::scope_end, 3}}};

void Analyzer::showNameColumn(FrameRange &range) {
	TextFormatter fmt;

	for(int idx : intRange(range.exec_list)) {
		auto exec_id = range.exec_list[idx];
		auto &exec = m_exec_tree[exec_id];
		auto &row = range.rows[exec_id];

		bool has_children = range.has_children[exec_id];
		const auto &opened = range.opened[exec_id];
		auto indent = exec.depth * 10.0f + (has_children ? 0.0f : 14.0f);
		if(indent)
			ImGui::Indent(indent);

		if(has_children) {
			auto pos = ImGui::GetCursorPos();
			fmt("   ##%", exec_id);
			if(ImGui::Button(fmt.c_str()))
				toggleOpen(exec_id);
			fmt.clear();
			bool active = range.has_active_children[exec_id];
			m_triangles.emplace_back(int2(pos) + int2(0, 2), active, opened);
			ImGui::SameLine();
		}

		fmt << execName(exec_id);
		uint flags = 0;
		if(!exec.children)
			flags |= ImGuiTreeNodeFlags_Leaf;

		int num_insts = row.num_instances;
		int num_pinsts =
			exec.parent_id ? range.rows[exec.parent_id].num_instances : range.num_frames;
		if(num_insts != num_pinsts && exec_id && exec.type == ExecNodeType::scope) {
			if(num_insts < num_pinsts)
				fmt(" (% / %)", num_insts, num_pinsts);
			else if(num_insts % num_pinsts == 0)
				fmt("* %", num_insts / num_pinsts);
			else
				fmt.stdFormat("* %.2f", double(num_insts) / num_pinsts);
		}

		int2 start_pos = ImGui::GetCursorPos();
		ImGui::Text("%s", fmt.c_str());
		if(ImGui::IsItemClicked() && has_children)
			toggleOpen(exec_id);
		int2 end_pos = ImGui::GetCursorPos();
		m_intervals[idx] = Interval<int>(start_pos.y, end_pos.y);

		fmt.clear();
		if(indent)
			ImGui::Unindent(indent);
	}
}

DEFINE_ENUM(TimeUnit, seconds, milli, micro, nano);

struct TimeInfo {
	TimeUnit unit;
	int integral, fractional2;
};

TimeInfo timeInfo(i64 ns) {
	PASSERT(ns >= 0);
	if(ns >= 1000000000)
		return {TimeUnit::seconds, int(ns / 1000000000), int((ns / 1000000) % 1000) / 10};
	else if(ns >= 1000000)
		return {TimeUnit::milli, int(ns / 1000000), int((ns / 1000) % 1000) / 10};
	else if(ns >= 1000)
		return {TimeUnit::micro, int(ns / 1000), int(ns % 1000) / 10};
	else
		return {TimeUnit::nano, (int)ns, 0};
}

void formatValue(TextFormatter &out, u64 value) {
	if(value >= 1000000) {
		value /= 1000;
		const char *format = value >= 100000 ? "%.0fM" : value >= 10000 ? "%.1fM" : "%.2fM";
		out.stdFormat(format, double(value) * 0.001);
	} else if(value >= 1000) {
		const char *format = value >= 100000 ? "%.0fK" : value >= 10000 ? "%.1fK" : "%.2fK";
		out.stdFormat(format, double(value) * 0.001);
	} else {
		out << value;
	}
}

void Analyzer::showDataColumn(ColumnId col_id, FrameRange &range) {
	TextFormatter fmt;

	static const EnumMap<TimeUnit, ImColor> time_colors = {
		{(ImColor)float4(0.65f, 0.1f, 0.1f, 1.0f), (ImColor)float4(1.0f, 0.3f, 0.3f, 1.0f),
		 (ImColor)float4(1.0f, 1.0f, 0.3f, 1.0f), (ImColor)float4(0.3f, 1.0f, 0.3f, 1.0f)}};

	static const EnumMap<TimeUnit, const char *> time_units{{"s", "ms", "\xC2\xB5s", "ns"}};

	for(int idx : intRange(range.exec_list)) {
		auto exec_id = range.exec_list[idx];
		auto &exec = m_exec_tree[exec_id];
		auto &row = range.rows[exec_id];

		int value_id = int(col_id) - 1;
		auto value = row.values[value_id];

		if(value) {
			if(exec.type == ExecNodeType::counter) {
				formatValue(fmt, value);

				auto tsize = ImGui::CalcTextSize(fmt.c_str());
				ImGui::Dummy({m_data_width - 10.0f - tsize.x, 1.0f});
				ImGui::SameLine();
				menu::text(fmt.text());
			} else {
				auto tvalue = timeInfo(value);

				fmt << tvalue.integral;
				auto tsize = ImGui::CalcTextSize(fmt.c_str());
				ImGui::Dummy({m_data_width / 3 - tsize.x, 1.0f});
				ImGui::SameLine();
				ImGui::TextColored(time_colors[tvalue.unit], "%s.%02d %s", fmt.c_str(),
								   tvalue.fractional2, time_units[tvalue.unit]);
			}
		} else {
			ImGui::Text(" ");
		}

		fmt.clear();
	}
}

static const array<const char *, 7> column_names[2] = {
	{"", "CPU", "/", "CNT", "", "GPU", ""},
	{"name", "avg", "min", "max", "avg", "min", "max"},
};

CSpan<Frame> Analyzer::getFrames() {
	auto frames = cspan(m_manager.frames());

	switch(m_data_source) {
	case DataSource::custom_range: {
		int first_frame = clamp(m_first_frame, 0, frames.size() - 1);
		int end_frame = clamp(m_end_frame, first_frame + 1, frames.size());
		return frames.subSpan(first_frame, end_frame);
	}
	case DataSource::last_frames: {
		// Grabbing last frames makes no sense? user has to wait until animation is over anyways
		if(m_last_sample_frame + m_num_last_frames * 2 <= frames.size())
			m_last_sample_frame = frames.size() - m_num_last_frames;
		double time = 0.0;
		auto span = frames.subSpan(m_last_sample_frame,
								   min(m_last_sample_frame + m_num_last_frames, frames.size()));
		for(int n : intRange(span)) {
			time += span[n].end_time - span[n].start_time;
			if(time > m_limit_sampling_time) {
				span = {span.begin(), n + 1};
				break;
			}
		}
		return span;
	}
	}

	return {}; //Let's satisfy GCC
}

void Analyzer::miniMenu() {
	selectColumns();
	ImGui::SameLine();
	changeOptions();
	ImGui::SameLine();
	menu::showHelpMarker("Additional controls:\nMMB: select (or deselect) a specific "
						 "row\nRMB: show additional information for given row");
}

void Analyzer::selectColumns() {
	if(ImGui::Button("Select columns"))
		ImGui::OpenPopup("column_selection");

	if(ImGui::BeginPopup("column_selection")) {
		for(auto col_id = next(ColumnId::name); col_id != ColumnId::name; col_id = next(col_id)) {
			bool is_enabled = m_visible_columns & col_id;
			if(ImGui::Checkbox(toString(col_id), &is_enabled))
				m_visible_columns.setIf(col_id, is_enabled);
		}
		ImGui::EndPopup();
	}
}

void Analyzer::changeOptions() {
	if(ImGui::Button("Configure"))
		ImGui::OpenPopup("change_options");

	if(ImGui::BeginPopup("change_options")) {
		auto frames = cspan(m_manager.frames());
		int num_samples = 0;
		for(auto &frame : frames)
			num_samples += frame.samples.size();

		auto data_size = m_manager.usedMemory();
		menu::text("Frames recorded: % (% MB)", frames.size(), data_size / (1024 * 1024));
		menu::text("AVG Samples/frame: %", num_samples / frames.size());
		ImGui::SameLine();
		menu::showHelpMarker("Try to keep this number low (few thousands at most)");

		menu::text("Num GPU queries: %\n", ThreadContext::numGpuQueries());
		ImGui::SameLine();
		menu::showHelpMarker(
			"Be careful with PERF_GPU_SCOPE(): each call will require separate query");
		menu::selectEnum("Data source: ", m_data_source);

		if(m_data_source == DataSource::custom_range) {
			int num_frames = m_end_frame - m_first_frame;
			if(menu::inputValue("First frame", m_first_frame))
				m_first_frame = clamp(m_first_frame, 0, frames.size() - 1);
			menu::inputValue("Num frames", num_frames);
			m_end_frame = clamp(m_first_frame + num_frames, m_first_frame + 1, frames.size());
		} else if(m_data_source == DataSource::last_frames) {
			if(menu::inputValue("Num frames", m_num_last_frames))
				m_num_last_frames = clamp(m_num_last_frames, 1, 10000);
			if(menu::inputValue("Limit sampling time", m_limit_sampling_time))
				m_limit_sampling_time = clamp(m_limit_sampling_time, 1.0 / 60.0, 1000.0);
		}

		if(auto cur_frames = getFrames())
			if(ImGui::Button("Dump first frame"))
				print("\n%\n", dump(cur_frames.front()));

		ImGui::Separator();

		menu::selectEnum("Sort by", m_sort_var);
		ImGui::Checkbox("Inverse order", &m_sort_inverse);
		ImGui::Checkbox("Show empty rows", &m_show_empty);
		ImGui::EndPopup();
	}
}

void Analyzer::showGrid(FrameRange &range) {
	bool update_scroll = m_update_scroll;
	m_update_scroll = false;

	auto padding = ImGui::GetStyle().FramePadding;
	padding.y = 0;
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, padding);

	ImDrawList *draw_list = ImGui::GetWindowDrawList();

	m_triangles.clear();
	m_intervals.resize(range.exec_list.size());

	draw_list->ChannelsSplit(3);
	draw_list->ChannelsSetCurrent(1);
	IRect inner_rect;

	int min_first_width = 140, min_height = 160;
	int2 window_size = ImGui::GetWindowSize(), window_spacing(30, 30);
	int2 grid_size(window_size.x - window_spacing.x,
				   max(window_size.y - window_spacing.y, min_height));

	int num_cpu_cols = countBits(m_visible_columns & cpuColumns());
	int num_gpu_cols = countBits(m_visible_columns & gpuColumns());
	int num_cols = num_cpu_cols + num_gpu_cols;
	int first_width = max(min_first_width, grid_size.x - m_data_width * num_cols);
	int header_height = 0;

	{ // Headers
		const char *titles[3] = {"", "CPU / VAL", "GPU"};

		int sizes[3] = {first_width, m_data_width * num_cpu_cols, m_data_width * num_gpu_cols};
		int height = ImGui::CalcTextSize("X").y;

		for(int n : intRange(3)) {
			if(sizes[n] == 0)
				continue;

			ImGui::BeginChild(titles[n], float2(sizes[n], height), false,
							  ImGuiWindowFlags_NoScrollWithMouse);
			if(n == 0)
				miniMenu();
			else
				menu::centeredText(sizes[n] / 2, titles[n]);
			ImGui::EndChild();
			header_height = ImGui::GetItemRectSize().y;
			ImGui::SameLine();
		}
		ImGui::NewLine();
	}

	static EnumMap<ColumnId, const char *> sub_titles = {
		{"Name:", "avg", "min", "max", "avg", "min", "max"}};

	ImGui::PushStyleColor(ImGuiCol_Button, float4(0.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, float4(1.0f, 1.0f, 1.0f, 0.2f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, float4(1.0f, 1.0f, 1.0f, 0.3f));
	for(auto col_id : all<ColumnId>) {
		bool first = col_id == ColumnId::name;
		if(!(m_visible_columns & col_id))
			continue;

		if(!first)
			ImGui::SameLine();

		float width = first ? first_width : m_data_width;
		ImGui::BeginChild(toString(col_id), float2(width, grid_size.y - header_height), false,
						  ImGuiWindowFlags_NoScrollbar);
		if(first)
			ImGui::Text("%s", sub_titles[col_id]);
		else
			menu::centeredText(m_data_width / 2, sub_titles[col_id]);
		ImGui::Separator();

		ImGui::BeginChild(format("%_rows", col_id).c_str(), {}, false,
						  first ? 0 : ImGuiWindowFlags_NoScrollbar);

		int2 window_pos = ImGui::GetWindowPos(), window_size = ImGui::GetWindowSize();
		if(first)
			inner_rect = {window_pos, window_pos + window_size};
		else
			inner_rect.setMax(window_pos + window_size);

		if(update_scroll) {
			ImGui::SetScrollY(m_scroll_pos);
		} else if(!m_update_scroll) {
			int cur_scroll = ImGui::GetScrollY();
			if(cur_scroll != m_scroll_pos) {
				m_scroll_pos = cur_scroll;
				m_update_scroll = true;
				update_scroll = true;
			}
		}

		if(first)
			showNameColumn(range);
		else
			showDataColumn(col_id, range);

		ImGui::EndChild();
		ImGui::EndChild();
	}

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar();

	int hovered_row = -1;
	bool still_tooltip = false;

	{ // Background
		draw_list->ChannelsSetCurrent(2);
		draw_list->PushClipRect(inner_rect.min(), inner_rect.max(), true);

		ImColor color = (ImColor)float4(1.0f, 1.0f, 1.0f, 0.1f);
		ImColor highlight_color = (ImColor)float4(0.7f, 0.7f, 1.0f, 0.2f);
		ImColor sel_color = (ImColor)float4(0.6f, 0.6f, 1.0f, 0.3f);
		int2 offset = inner_rect.min() - int2(0, m_scroll_pos);

		for(int n : intRange(m_intervals)) {
			auto &vert = m_intervals[n];
			IRect rect(0, vert.min, inner_rect.width(), vert.max);
			rect += offset;
			ImGui::SetCursorScreenPos(rect.min());
			ImGui::InvisibleButton("", rect.size());

			auto exec_id = range.exec_list[n];
			bool is_hovered = false, is_selected = range.exec_list[n] == m_selected_exec;
			if(ImGui::IsItemHovered()) {
				is_hovered = true;
				hovered_row = n;
				if(ImGui::IsItemClicked(1) || m_tooltip_exec == exec_id)
					still_tooltip = showTooltip(exec_id);
			}

			if(vert.empty() || ((n & 1) && !is_hovered && !is_selected))
				continue;

			auto col = is_selected ? sel_color : is_hovered ? highlight_color : color;
			draw_list->AddRectFilled(rect.min(), rect.max(), col, 0.0f);
		}

		ImVec4 gray = (ImColor)float4(0.5, 0.5, 0.5, 1.0);

		for(auto [pos, active, is_opened] : m_triangles) {
			if(!active)
				ImGui::PushStyleColor(ImGuiCol_Text, gray);
			ImGui::RenderArrow(pos + offset, is_opened ? ImGuiDir_Down : ImGuiDir_Right);
			if(!active)
				ImGui::PopStyleColor();
		}
		draw_list->PopClipRect();
	}

	if(!still_tooltip)
		m_tooltip_exec = none;

	m_menu_rect = IRect(ImGui::GetWindowSize()) + ImGui::GetWindowPos();
	if(m_menu_rect.contains(ImGui::GetMousePos()) && ImGui::IsMouseClicked(2)) {
		if(hovered_row == -1)
			m_selected_exec = none;
		else
			m_selected_exec = range.exec_list[hovered_row];
	}

	draw_list->ChannelsMerge();
}

bool Analyzer::showTooltip(ExecId exec_id) {
	m_tooltip_exec = exec_id;
	if(auto text = execInfo(exec_id); !text.empty()) {
		menu::showTooltip(text);
		return true;
	}
	return false;
}

void Analyzer::toggleOpen(ExecId exec_id) {
	m_set_opened_nodes = none;
	m_range.opened[exec_id] ^= 1;
}

vector<u64> Analyzer::openedNodes() const {
	vector<u64> out;
	for(int n : intRange(m_range.opened))
		if(m_range.opened[n])
			out.emplace_back(m_exec_tree.hashedId(ExecId(n)));
	return out;
}

void Analyzer::updateOpenedNodes() {
	if(m_set_opened_nodes) {
		fill(m_range.opened, false);
		for(auto n : intRange(m_range.opened)) {
			auto hashed_id = m_exec_tree.hashedId(ExecId(n));
			m_range.opened[n] = isOneOf(hashed_id, *m_set_opened_nodes);
		}
	}
}

void Analyzer::setOpenedNodes(vector<u64> hashed_ids) { m_set_opened_nodes = move(hashed_ids); }

AnyConfig Analyzer::config() const {
	AnyConfig out;
	out.set("window_rect", menuRect());
	out.set("opened_nodes", openedNodes());
	out.set("columns", m_visible_columns);
	out.set("data_source", m_data_source, DataSource::last_frames);
	out.set("sort_var", m_sort_var, SortVar::execution);
	out.set("sort_inverse", m_sort_inverse, false);
	out.set("show_empty", m_show_empty, false);
	out.set("first_frame", m_first_frame, 0);
	out.set("end_frame", m_end_frame, m_first_frame + 1);
	if(m_selected_exec)
		out.set("selected", m_exec_tree.hashedId(*m_selected_exec));
	out.set("num_last_frames", m_num_last_frames, 60);
	out.set("limit_sampling_time", m_limit_sampling_time, 2.0);
	return out;
}

void Analyzer::setConfig(const AnyConfig &config) {
	if(auto rect = config.get<IRect>("window_rect"))
		setMenuRect(*rect);
	if(auto nodes = config.get<vector<u64>>("opened_nodes"))
		setOpenedNodes(*nodes);
	m_visible_columns = config.get("columns", m_visible_columns);
	m_data_source = config.get("data_source", DataSource::last_frames);
	m_sort_var = config.get("sort_var", SortVar::execution);
	m_sort_inverse = config.get("sort_inverse", false);
	m_show_empty = config.get("show_empty", false);
	m_first_frame = config.get("first_frame", 0);
	m_end_frame = config.get("end_frame", m_first_frame + 1);
	// Problem: ładowanie niektórych zmiennych nie ma sensu dopóki nie mamy danych...
	if(auto sel_exec = config.get("selected", u64(0)))
		m_selected_exec_hash = sel_exec;
	m_num_last_frames = config.get("num_last_frames", 60);
	m_limit_sampling_time = config.get("limit_sampling_time", 2.0);
}
}
