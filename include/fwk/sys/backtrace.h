// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/maybe.h"
#include "fwk/sys_base.h"
#include "fwk/vector.h"

namespace fwk {

// TODO: better place for that function
Maybe<int2> consoleDimensions();

enum class BacktraceMode {
	fast, // default
	full,
	disabled
};

class Backtrace {
  public:
	Backtrace(vector<void *> addresses, Pair<string, bool>);
	Backtrace(vector<void *> addresses);
	Backtrace() = default;

	using Mode = BacktraceMode;

	// Setting this will enable LLDB backtraces instead of GDB
	static string g_lldb_command;
	static inline __thread Mode t_default_mode = Mode::fast;
	static inline __thread Mode t_fatal_mode = Mode::full;
	static inline __thread Mode t_assert_mode = Mode::full;
	static inline __thread Mode t_except_mode = Mode::fast;

	// If available, gdb backtraces will be used (which are more accurate)
	static Backtrace get(int skip = 0, void *context = nullptr, Maybe<Mode> = none);

	static Pair<string, bool> fullBacktrace(int skip_frames = 0) NOINLINE;

	// When filter is true, analyzer uses c++filt program to demangle C++
	// names; it also shortens some of the common long class names, like
	// std::basic_string<...> to fwk::string
	string analyze(bool filter) const;
	auto size() const { return m_addresses.size(); }
	bool empty() const { return !m_addresses && m_gdb_result.first.empty(); }

  private:
	static string filter(const string &);

	vector<void *> m_addresses;
	Pair<string, bool> m_gdb_result;
	bool m_use_gdb = false;
};
}
