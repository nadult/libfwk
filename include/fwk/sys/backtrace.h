// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/maybe.h"
#include "fwk/sys_base.h"
#include "fwk/vector.h"

namespace fwk {

//TODO: better name
struct BacktraceInfo {
	string file, function;
	int line, column;
	bool is_inlined;
};

// TODO: better place for that function
Maybe<int2> consoleDimensions();
string demangle(string);

// TODO: mode no longer needed? We can either disable it or don't get it at all?
enum class BacktraceMode {
	fast, // default
	full,
	disabled
};

class Backtrace {
  public:
	Backtrace(vector<void *> addrs) : m_addresses(move(addrs)) {}
	Backtrace() = default;

	using Mode = BacktraceMode;

	static inline __thread Mode t_default_mode = Mode::fast;
	static inline __thread Mode t_fatal_mode = Mode::full;
	static inline __thread Mode t_assert_mode = Mode::full;
	static inline __thread Mode t_except_mode = Mode::fast;

	// TODO: skip by default shouldnt be passed in most situations
	static Backtrace get(int skip = 0, void *context = nullptr, Maybe<Mode> = none);

	vector<BacktraceInfo> analyze() const;

	// By default max_cols is taken from current console or 120
	string format(Maybe<int> max_cols = none) const;
	static string format(vector<BacktraceInfo>, Maybe<int> max_cols = none);

	auto size() const { return m_addresses.size(); }
	bool empty() const { return !m_addresses; }

	void operator>>(TextFormatter &) const;

  private:
	vector<void *> m_addresses;
};
}
