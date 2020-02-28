// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/maybe.h"
#include "fwk/sys_base.h"
#include "fwk/vector.h"

namespace fwk {

//TODO: better name
struct BacktraceInfo {
	string obj_file, obj_func;
	string file, function;
	int line, column;
	bool is_inlined;
};

// TODO: better place for that function
Maybe<int2> consoleDimensions();
string demangle(string);

class Backtrace {
  public:
	Backtrace(vector<void *> addrs) : m_addresses(move(addrs)) {}
	Backtrace() = default;

	static inline __thread bool t_is_enabled = true;
	static inline __thread bool t_on_except_enabled = true;

	// TODO: skip by default shouldnt be passed in most situations
	// context is useful only on MinGW platform (typically in case of a segfault)
	static Backtrace get(int skip = 0, void *context = nullptr, bool is_enabled = t_is_enabled);

	vector<BacktraceInfo> analyze() const;

	// By default max_cols is taken from current console or 120
	string format(Maybe<int> max_cols = none) const;
	static string format(vector<BacktraceInfo>, Maybe<int> max_cols = none);

	explicit operator bool() const { return !!m_addresses; }
	auto size() const { return m_addresses.size(); }
	bool empty() const { return !m_addresses; }

	bool operator==(const Backtrace &) const;
	bool operator<(const Backtrace &) const;
	void operator>>(TextFormatter &) const;

  private:
	vector<void *> m_addresses;
};
}
