// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/backtrace.h"

#include "fwk/filesystem.h"
#include "fwk/format.h"
#include "fwk/sys/rollback.h"

#include <cstdio>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#ifdef FWK_TARGET_LINUX
#include <execinfo.h>

#include <dlfcn.h>
#include <unistd.h>
#endif

namespace fwk {

namespace {

	string nicePath(const string &path) {
		if(path[0] == '?')
			return "";
		FilePath file_path(path);
		FilePath relative_path = file_path.relative(FilePath::current());
		if(relative_path.size() < file_path.size())
			file_path = relative_path;
		return file_path;
	}

	string analyzeCommand(CSpan<void *> addresses, bool funcs = false) {
		if(addresses.empty())
			return {};

		TextFormatter command;
		command("addr2line ");
		for(auto address : addresses)
			command.stdFormat("%p ", address);
		command("%-e % 2>/dev/null", funcs ? "-f -p " : "",
				executablePath().relative(FilePath::current()));
		return command.text();
	}

	// Source: https://stackoverflow.com/questions/53849/how-do-i-tokenize-a-string-in-c
	vector<string> split(const char *str, char c = ' ') {
		vector<string> result;

		do {
			const char *begin = str;
			while(*str != c && *str)
				str++;
			if(begin != str)
				result.emplace_back(string(begin, str));
		} while(*str++ != 0);

		return result;
	}

	template <class Iter> string mergeWithSpaces(Iter begin, Iter end) {
		string out;
		while(begin != end) {
			out += (out.empty() ? "" : " ") + *begin;
			begin++;
		}
		return out;
	}

	Maybe<int> consoleColumns() {
		auto result = execCommand("tput cols");
		if(!result.second)
			return none;
		int val = stoi(result.first);
		return val;
	}

	string filterGdb(string input, int skip_frames) {
		auto lines = split(input.c_str(), '\n');
		vector<string> out_lines;
		bool found_first = false;

		struct Entry {
			string file, line;
			string function;

			string simple;
		};

		vector<Entry> entries;

		for(auto line : lines) {
			if(line[0] == '#' && line.find("gdbBacktrace") != string::npos) {
				found_first = true;
				continue;
			}

			if(!found_first || line[0] != '#' || skip_frames-- > 0)
				continue;

			auto tokens = split(line.c_str());
			if(tokens.size() > 3) {
				int count = tokens[2] == "in" ? 3 : 1;
				tokens.erase(tokens.begin(), tokens.begin() + count);
			}

			string function, file_line;

			int split_pos = -1;
			for(int i = tokens.size() - 1; i >= 0; i--)
				if(tokens[i] == "at" && i > 0 && tokens[i - 1].back() == ')') {
					split_pos = i;
					break;
				}

			Entry new_entry;
			new_entry.simple = mergeWithSpaces(begin(tokens), end(tokens));
			if(split_pos != -1) {
				new_entry.function = mergeWithSpaces(begin(tokens), begin(tokens) + split_pos);
				auto file_line = mergeWithSpaces(begin(tokens) + split_pos + 1, end(tokens));

				string file;
				int line;

				tokens = split(file_line.c_str(), ':');
				if(tokens.size() == 2) {
					new_entry.file = tokens[0];
					new_entry.line = tokens[1];
				}
			}

			entries.emplace_back(new_entry);
		}

		int num_columns = 120;
		if(auto cols = consoleColumns())
			num_columns = *cols;

		int limit_line_size = 6;
		int limit_func_size = max(20, num_columns * 3 / 4);
		int limit_file_size = max(16, num_columns - limit_line_size - limit_func_size);

		int max_lsize = 0, max_fsize = 0;
		for(auto &e : entries) {
			e.file = Str(e.file).limitSizeFront(limit_file_size);
			e.line = Str(e.line).limitSizeBack(limit_line_size);
			e.function = Str(e.function).limitSizeBack(limit_func_size);

			max_lsize = max(max_lsize, (int)e.line.size());
			max_fsize = max(max_fsize, (int)e.file.size());
		}

		string out;
		for(auto &e : entries) {
			if(e.file.empty() || e.line.empty() || e.function.empty())
				out += ">> " + e.simple + "\n";
			else
				out += string(max(max_fsize - (int)e.file.size(), 0), ' ') + e.file + ":" + e.line +
					   string(max(max_lsize - (int)e.line.size(), 0), ' ') + " " + e.function +
					   "\n";
		}
		return out;
	}

	vector<string> analyzeAddresses(vector<void *> addresses) {
		string result = execCommand(analyzeCommand(addresses)).first;
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
			string file = nicePath(file_line.substr(0, colon_pos));
			int line = colon_pos == string::npos ? 0 : atoi(&file_line[colon_pos + 1]);
			file_line = file.empty() ? "?" : stdFormat("%s:%d", file.c_str(), line);
		}
		return file_lines;
	}

	void filterString(string &str, const char *src, const char *dst) {
		DASSERT(src && dst);
		int src_len = strlen(src);

		auto pos = str.find(src);
		while(pos != string::npos) {
			str = str.substr(0, pos) + dst + str.substr(pos + src_len);
			pos = str.find(src);
		}
	}

	// TODO: these don't work too good
	const char *s_filtered_names[] = {
		"unsigned int",
		"uint",
		"std::basic_string<char, std::char_traits<char>, std::allocator<char> >",
		"fwk::string",
		"std::",
		"",
		"fwk::",
		"",
	};
}

void winGetBacktrace(vector<void *> &addrs, int skip, void *context);

__thread BacktraceMode Backtrace::t_default_mode = Mode::fast;

Backtrace::Backtrace(vector<void *> addresses, vector<string> symbols)
	: m_addresses(move(addresses)), m_symbols(move(symbols)) {}
Backtrace::Backtrace(vector<void *> addresses, vector<string> symbols,
					 pair<string, bool> gdb_result)
	: m_addresses(move(addresses)), m_symbols(move(symbols)), m_gdb_result(move(gdb_result)),
	  m_use_gdb(true) {}

Backtrace Backtrace::get(size_t skip, void *context_, Maybe<Mode> mode) {
	vector<void *> addrs;
	vector<string> symbols;

	if(!mode)
		mode = t_default_mode;
	if(mode == Mode::disabled)
		return {};

#if defined(FWK_TARGET_MINGW)
	winGetBacktrace(addrs, skip, context_);
#elif defined(FWK_TARGET_LINUX)
	// TODO: backtrace_symbols should be deferred
	void *addresses[64];
	size_t size = ::backtrace(addresses, arraySize(addresses));
	char **strings = backtrace_symbols(addresses, size);

	for(size_t i = max<int>(0, skip - 1); i < size; i++) {
		addrs.emplace_back(addresses[i]);
		symbols.emplace_back(string(strings[i]));
	}
	::free(strings);
#endif

#ifdef FWK_TARGET_LINUX
	if(*mode == Mode::full)
		return {move(addrs), move(symbols), gdbBacktrace(skip)};
#endif

	return {move(addrs), move(symbols)};
}

pair<string, bool> Backtrace::gdbBacktrace(int skip_frames) {
	printf("Generating GDB backtrace\n");

#ifdef FWK_TARGET_LINUX
	auto pid = getpid();
	char cmd[256];
	snprintf(cmd, arraySize(cmd), "gdb 2>&1 -batch -p %d -ex 'thread apply all bt'", (int)pid);
	auto result = execCommand(cmd);

	if(result.first.find("ptrace: Operation not permitted") != string::npos)
		return {"To use GDB stacktraces, you have to:\n"
				"1) set kernel.yama.ptrace_scope to 0 in: /etc/sysctl.d/10-ptrace.conf\n"
				"2) type: echo 0 > /proc/sys/kernel/yama/ptrace_scope\n",
				false};

	return {filterGdb(result.first, skip_frames), true};
#else
	return {"GDB-based backtraces are only supported on linux (for now)", false};
#endif
}

string Backtrace::analyze(bool filter) const {
	TextFormatter formatter;
	vector<string> file_lines;

#if defined(FWK_TARGET_HTML5)
#elif defined(FWK_TARGET_LINUX)
	file_lines = analyzeAddresses(m_addresses);
#elif defined(FWK_TARGET_MINGW)
	formatter("Please run following command:\n% | c++filt\n", analyzeCommand(m_addresses, true));
#endif
	if(!file_lines.empty()) {
		int max_len = 0;
		for(const auto &file_line : file_lines)
			max_len = max(max_len, (int)file_line.size());

		for(int i = 0; i < size(); i++) {
			string tstring = i < m_symbols.size() ? m_symbols[i] : "";
			const char *file_line = i < file_lines.size() ? file_lines[i].c_str() : "";
			tstring = tstring.substr(0, tstring.find('['));

			string fmt = stdFormat("%%%ds %%s\n", max_len);
			formatter.stdFormat(fmt.c_str(), file_line, tstring.c_str());
		}
	}

	string out;
#ifdef FWK_TARGET_LINUX
	if(m_use_gdb) {
		if(!m_gdb_result.second)
			out += m_gdb_result.first + "\n";
		else
			return m_gdb_result.first;
	}
#endif

	out += formatter.text();
	return filter ? Backtrace::filter(out) : out;
}

string Backtrace::filter(const string &input) {
#ifdef FWK_TARGET_LINUX
	string command = "echo \"" + input + "\" | c++filt -n";

	FILE *file = popen(command.c_str(), "r");
	if(file) {
		vector<char> buf;
		while(!feof(file) && buf.size() < 1024 * 16) {
			buf.push_back(fgetc(file));
			if((u8)buf.back() == 255) {
				buf.pop_back();
				break;
			}
		}
		fclose(file);

		string out(&buf[0], buf.size());
		for(int n = 0; n < (int)arraySize(s_filtered_names); n += 2)
			filterString(out, s_filtered_names[n], s_filtered_names[n + 1]);
		return out;
	}
#endif

	return input;
}

void Backtrace::validateMemory() {
	vector<const void *> pointers;
	pointers.emplace_back(m_addresses.data());
	pointers.emplace_back(m_symbols.data());
	pointers.emplace_back(m_gdb_result.first.data());
	for(auto &symbol : m_symbols)
		pointers.emplace_back(symbol.data());
	if(RollbackContext::willRollback(pointers))
		FATAL("Bactrace improperly allocated: will be freed on rollback");
}
}
