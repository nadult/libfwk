// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

// This file contains snippets of code from backward:
// https://github.com/bombela/backward-cpp.git
// License is available in extern/backward-license.txt

#include "fwk/sys/backtrace.h"

#include "fwk/format.h"
#include "fwk/io/file_system.h"
#include "fwk/sys/expected.h"

#include <cstdio>
#include <cxxabi.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef FWK_DWARF_DISABLED
#include "backtrace_dwarf.h"
#endif

// -------------------------------------------------------------------------------------------
// ---  Getting stack trace addresses  -------------------------------------------------------

#if defined(FWK_PLATFORM_LINUX)

#define USE_UNWIND

#include <dlfcn.h>
#include <execinfo.h>
#include <unistd.h>

#ifdef USE_UNWIND
#include <unwind.h>

struct UnwindContext {
	fwk::Span<void *> addrs;
	int index;
};

static _Unwind_Reason_Code unwind_trampoline(_Unwind_Context *ctx, void *self) {
	auto &out = *(UnwindContext *)self;
	if(out.index >= out.addrs.size())
		return _URC_END_OF_STACK;

	int ip_before_instruction = 0;
	auto ip = _Unwind_GetIPInfo(ctx, &ip_before_instruction);

	if(!ip_before_instruction)
		ip = ip == 0 ? 0 : ip - 1;

	if(out.index >= 0)
		out.addrs[out.index] = (void *)ip;
	if(ip)
		out.index++;
	return _URC_NO_REASON;
}
#endif

static int linuxGetBacktrace(fwk::Span<void *>) NOINLINE;
static int linuxGetBacktrace(fwk::Span<void *> addrs) {
#if defined(USE_UNWIND)
	UnwindContext ctx{addrs, -1};
	_Unwind_Backtrace(&unwind_trampoline, &ctx);
	return ctx.index;
#else
	return ::backtrace(out.data(), out.size());
#endif
}

#endif

// -------------------------------------------------------------------------------------------
// ---  Random backtrace-related functions  --------------------------------------------------

namespace fwk {

Maybe<int2> consoleDimensions() {
#ifdef FWK_PLATFORM_LINUX
	auto result = execCommand("tput cols && tput lines");
	if(result && result->second == 0) {
		const char *ptr = result->first.c_str();
		char *end = nullptr;
		errno = 0;
		auto cols = ::strtol(ptr, &end, 10);
		if(errno != 0 || end == ptr)
			return none;

		ptr = end;
		auto lines = ::strtol(ptr, &end, 10);
		if(errno != 0 || end == ptr)
			return none;
		return int2(cols, lines);
	}
#endif
	return none;
}

string demangle(string str) {
	int status = 0;
	char *result = abi::__cxa_demangle(str.c_str(), nullptr, nullptr, &status);
	if(result) {
		str = result;
		free(result);
	}
	return str;
}

namespace {

	string nicePath(const string &path, const FilePath &current) {
		if(path[0] == '?')
			return "";
		FilePath file_path(path);
		if(file_path.isRelative())
			return file_path;

		FilePath relative_path = file_path.relative(current);
		if(relative_path.size() < file_path.size())
			file_path = relative_path;
		return file_path;
	}

	string analyzeCommand(CSpan<void *> addresses, const FilePath &current, bool funcs = false) {
		if(!addresses)
			return {};

		TextFormatter command;
		command("addr2line ");
		for(auto address : addresses)
			command.stdFormat("%p ", address);
		command("%-e % 2>/dev/null", funcs ? "-f -p " : "", executablePath().relative(current));
		return command.text();
	}

	template <class Iter> string mergeWithSpaces(Iter begin, Iter end) {
		string out;
		while(begin != end) {
			out += (out.empty() ? "" : " ") + *begin;
			begin++;
		}
		return out;
	}

	struct Entry {
		string file, line;
		string function;

		string simple;
	};

	vector<string> analyzeAddresses(vector<void *> addresses, const FilePath &current) {
		auto cmd_result = execCommand(analyzeCommand(addresses, current));
		if(!cmd_result || cmd_result->second != 0)
			return {};

		string result = move(cmd_result->first);
		vector<string> file_lines;
		while(!result.empty()) {
			auto pos = result.find('\n');
			file_lines.emplace_back(result.substr(0, pos));
			if(pos != string::npos)
				pos++;
			result = result.substr(pos);
		}
		file_lines.resize(addresses.size());

		// TODO: these are not exactly correct (inlining?)
		for(auto &file_line : file_lines) {
			auto colon_pos = file_line.find(':');
			string file = nicePath(file_line.substr(0, colon_pos), current);
			int line = colon_pos == string::npos ? 0 : atoi(&file_line[colon_pos + 1]);
			file_line = file.empty() ? "?" : stdFormat("%s:%d", file.c_str(), line);
		}
		return file_lines;
	}

}

// -------------------------------------------------------------------------------------------
// ---  Backtrace: class implementation  -----------------------------------------------------

int winGetBacktrace(Span<void *> addrs, void *context_);

Backtrace Backtrace::get(int skip, void *context_, Maybe<Mode> mode) {
	if(!mode)
		mode = t_default_mode;
	if(mode == Mode::disabled)
		return {};

	array<void *, 64> buffer;
#ifdef FWK_PLATFORM_LINUX
	int count = linuxGetBacktrace(buffer);
#elif defined(FWK_PLATFORM_MINGW)
	int count = winGetBacktrace(buffer, context_);
#else
	int count = 0;
#endif

	// TODO: Can we be sure that all addresses are ok?
	skip = min(skip, count);
	return {span(buffer.data() + skip, count - skip)};
}

vector<BacktraceInfo> Backtrace::analyze() const {
#ifndef FWK_DWARF_DISABLED

	// TODO: create singleton and keep it in memory?
	DwarfResolver resolver;
	vector<BacktraceInfo> out;
	for(auto addr : m_addresses) {
		ResolvedTrace trace;
		trace.addr = addr;
		resolver.resolve(trace);

		out.emplace_back(trace.source.filename, trace.source.function, int(trace.source.line),
						 int(trace.source.col), false);
		for(auto &inl : trace.inliners)
			out.emplace_back(inl.filename, trace.object_function, int(inl.line), int(inl.col),
							 true);

		if(trace.object_function == "main")
			break; // Makes no sense to resolve after that
	}
	return out;

#elif defined(FWK_PLATFORM_LINUX)

	auto current = FilePath::current();
	if(!current)
		return {};
	auto file_lines = analyzeAddresses(m_addresses, *current);

	vector<BacktraceInfo> out;
	// TODO: fixit, make it not use separate program
	if(file_lines) {
		int max_len = 0;
		for(const auto &file_line : file_lines)
			max_len = max(max_len, (int)file_line.size());

		// backtrace_symbols returns: file(mangled_name+offset)
		char **strings = backtrace_symbols(m_addresses.data(), m_addresses.size());

		formatter("Simple bactrace (don't expect it to be accurate):\n");
		for(int i = 0; i < size(); i++) {
			BacktraceInfo binfo;

			string tstring = strings[i];
			auto p1 = str.find('('), p2 = str.rfind("+0x"), p3 = str.find(')');
			if(!isOneOf(string::npos, p1, p2, p3) && p1 < p2 && p2 < p3) {}
			tstring = demangle(str.substr(p1 + 1, p2 - p1 - 1)) + str.substr(p2, p3 - p2);

			const char *file_line = i < file_lines.size() ? file_lines[i].c_str() : "";
			tstring = tstring.substr(0, tstring.find('['));

			string fmt = stdFormat("%%%ds %%s\n", max_len);
			formatter.stdFormat(fmt.c_str(), file_line, tstring.c_str());
		}
		::free(strings);
	}

#elif defined(FWK_PLATFORM_MINGW)
	// TODO
#endif

	return {};
}

string Backtrace::format(Maybe<int> max_cols) const { return format(analyze(), max_cols); }

string Backtrace::format(vector<BacktraceInfo> infos, Maybe<int> max_cols) {
	string out;

	if(!max_cols)
		if(auto dims = consoleDimensions())
			max_cols = dims->x;
	int num_columns = max_cols.orElse(120);

	auto cur = FilePath::current();

	int limit_line_size = 6;
	int limit_func_size = max(20, num_columns * 3 / 4);
	int limit_file_size = max(16, num_columns - limit_line_size - limit_func_size);

	int max_line = 0, max_fsize = 0;
	for(auto &entry : infos) {
		if(cur) {
			FilePath fpath(entry.file);
			if(fpath.isAbsolute()) {
				auto rpath = string(fpath.relative(*cur));
				if(rpath.size() < entry.file.size())
					entry.file = move(rpath);
			}
		}
		entry.file = Str(entry.file).limitSizeFront(limit_file_size);
		entry.function = Str(entry.function).limitSizeBack(limit_func_size);

		max_line = max(max_line, entry.line);
		max_fsize = max(max_fsize, (int)entry.file.size());
	}
	int max_lsize = toString(max_line).size();

	for(auto &entry : infos) {
		auto line = toString(entry.line);
		if(entry.function.empty())
			entry.function = "???";
		out += string(max(max_fsize - (int)entry.file.size(), 0), ' ') + entry.file + ":" + line +
			   string(max(max_lsize - (int)line.size(), 0), ' ') + (entry.is_inlined ? '^' : ' ') +
			   entry.function + "\n";
	}

	return out;
}

void Backtrace::operator>>(TextFormatter &fmt) const { fmt << format(); }

}
