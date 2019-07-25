// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/math/box.h"
#include "fwk/perf_base.h"

namespace perf {

DEFINE_ENUM(AnalyzerDataSource, custom_range, last_frames);
DEFINE_ENUM(SortVar, execution, name, cpu_avg, cpu_min, cpu_max, gpu_avg, gpu_min, gpu_max);

// TODO: perf nie rozpoznaje dobrze nazw funkcji zaczynających się od static (ShadowRenderer::costam)
// TODO: możliwość wypisywania wartości w procentach?
// TODO(opt): jak dany punkt nie jest widoczny to nie ma sensu zbierać dla niego sampli
//            powinna być opcja wyłączenia próbkowania dla niewidocznych sampli
DEFINE_ENUM(ColumnId, name, cpu_avg, cpu_min, cpu_max, gpu_avg, gpu_min, gpu_max);
using ColumnFlags = EnumFlags<ColumnId>;

ColumnFlags gpuColumns();
ColumnFlags cpuColumns();

// GUI for perf-data analysis. Requires ImGui.
class Analyzer {
  public:
	using DataSource = AnalyzerDataSource;

	Analyzer();
	~Analyzer();

	Analyzer(const Analyzer &) = delete;
	void operator=(const Analyzer &) = delete;

	static Analyzer *instance();
	void doMenu(bool &is_enabled);

	struct Row {
		Row() {}

		explicit operator bool() const { return !empty(); }
		bool empty() const {
			return !cpu_min && !cpu_max && !cpu_avg && !gpu_min && !gpu_max && !gpu_avg;
		}

		union {
			struct {
				u64 cpu_avg = 0, cpu_min = 0, cpu_max = 0;
				u64 gpu_avg = 0, gpu_min = 0, gpu_max = 0;
			};
			struct {
				u64 cnt_avg, cnt_min, cnt_max;
			};
			u64 values[6];
		};
		u64 exec_order = ~0ull;
		int num_instances = 0;
	};

	struct FrameRange {
		vector<i64> average, minimum, maximum;
		int num_frames;

		// Accesed with ExecId:
		vector<bool> opened, empty;
		vector<Row> rows;

		vector<bool> has_children, has_active_children; // mapped with ExecId
		vector<ExecId> exec_list; // visible & ordered list of execs
	};

	string dump(const Frame &) const;

	void setMenuRect(IRect rect) { m_set_menu_rect = rect; }
	IRect menuRect() const { return m_menu_rect; }

	vector<u64> openedNodes() const;
	void setOpenedNodes(vector<u64>);
	void toggleOpen(ExecId);

	AnyConfig config() const;
	void setConfig(const AnyConfig &);

  private:
	Str execName(ExecId) const;
	string execInfo(ExecId) const;

	void computeRange(FrameRange &, CSpan<Frame>);
	void showTree(ExecId, const FrameRange &);
	void showNameColumn(FrameRange &);
	void showDataColumn(ColumnId, FrameRange &);
	void showGrid(FrameRange &);
	bool showTooltip(ExecId);

	void computeExecList(ExecId, FrameRange &) const;
	void computeExecList(FrameRange &) const;

	void computeRows(ExecId, vector<Row> &, const FrameRange &) const;
	void sortRows(Span<ExecId>, CSpan<Row>) const;

	CSpan<Frame> getFrames();
	void miniMenu();
	void selectColumns();
	void changeOptions();

	void updateOpenedNodes();

	DataSource m_data_source = DataSource::last_frames;

	int m_num_last_frames = 60;
	int m_last_sample_frame = 0;

	int m_first_frame = 0, m_end_frame = 0;
	SortVar m_sort_var = SortVar::execution;
	bool m_sort_inverse = false;
	bool m_show_empty = false;

	int m_data_width = 60;
	int m_scroll_pos = 0;
	bool m_update_scroll = false;
	int m_menu_height;
	IRect m_menu_rect;
	Maybe<IRect> m_set_menu_rect;

	struct Triangle {
		int2 pos;
		bool active;
		bool is_opened;
	};

	FrameRange m_range;
	ColumnFlags m_visible_columns;
	Maybe<ExecId> m_selected_exec, m_tooltip_exec;
	vector<Triangle> m_triangles;
	vector<Interval<int>> m_intervals;
	Maybe<vector<u64>> m_set_opened_nodes;
	Manager &m_manager;
	ExecTree &m_exec_tree;
};
}
