// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys_base.h"
#include "fwk_vector.h"
#include "fwk/maybe.h"

namespace fwk {

enum class BacktraceMode {
	fast, // default
	full, // using GDB to get accurate backtrace
	disabled
};

// TODO: use lib-lldb
class Backtrace {
  public:
	Backtrace(vector<void *> addresses, vector<string> symbols, pair<string, bool>);
	Backtrace(vector<void *> addresses, vector<string> symbols);
	Backtrace() = default;

	using Mode = BacktraceMode;

	static __thread Mode t_default_mode;

	// If available, gdb backtraces will be used (which are more accurate)
	static Backtrace get(size_t skip = 0, void *context = nullptr, Maybe<Mode> = none);

	static pair<string, bool> gdbBacktrace(int skip_frames = 0) NOINLINE;

	// When filter is true, analyzer uses c++filt program to demangle C++
	// names; it also shortens some of the common long class names, like
	// std::basic_string<...> to fwk::string
	string analyze(bool filter) const;
	auto size() const { return m_addresses.size(); }
	bool empty() const { return m_addresses.empty() && m_gdb_result.first.empty(); }

	void validateMemory();

  private:
	static string filter(const string &);

	vector<void *> m_addresses;
	vector<string> m_symbols;
	pair<string, bool> m_gdb_result;
	bool m_use_gdb = false;
};
}
