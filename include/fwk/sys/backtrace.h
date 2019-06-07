// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/maybe.h"
#include "fwk/sys_base.h"
#include "fwk/vector.h"

namespace fwk {

enum class BacktraceMode {
	fast, // default
	full,
	disabled
};

// TODO: use lib-lldb
class Backtrace {
  public:
	Backtrace(vector<void *> addresses, vector<string> symbols, Pair<string, bool>);
	Backtrace(vector<void *> addresses, vector<string> symbols);
	Backtrace() = default;

	using Mode = BacktraceMode;

	// Setting this will enable LLDB backtraces instead of GDB
	static string g_lldb_command;
	static __thread Mode t_default_mode;

	// If available, gdb backtraces will be used (which are more accurate)
	static Backtrace get(size_t skip = 0, void *context = nullptr, Maybe<Mode> = none);

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
	vector<string> m_symbols;
	Pair<string, bool> m_gdb_result;
	bool m_use_gdb = false;
};
}
