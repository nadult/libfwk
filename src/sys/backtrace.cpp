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
		if(!addresses)
			return {};

		TextFormatter command;
		command("addr2line ");
		for(auto address : addresses)
			command.stdFormat("%p ", address);
		command("%-e % 2>/dev/null", funcs ? "-f -p " : "",
				executablePath().relative(FilePath::current()));
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

	Maybe<int> consoleColumns() {
		auto result = execCommand("tput cols");
		if(!result.second)
			return none;
		int val = stoi(result.first);
		return val;
	}

	struct Entry {
		string file, line;
		string function;

		string simple;
	};

	struct ThreadInfo {
		string header;
		vector<string> lines;
		vector<Entry> entries;
		bool is_main = false;
	};

	vector<ThreadInfo> splitDebuggerInput(string input, bool lldb_mode) {
		auto lines = tokenize(input.c_str(), '\n');

		vector<ThreadInfo> out;
		ThreadInfo current;

		bool skip = lldb_mode;

		for(auto &line : lines) {
			if(lldb_mode && line.find("(lldb) thread backtrace all") == 0)
				skip = false;
			if(skip)
				continue;

			if(line.find(lldb_mode ? "* thread #" : "Thread") == 0) {
				if(current.lines)
					out.emplace_back(move(current));
				current = {};
				current.header = line;
				continue;
			}
			if(line.find(lldb_mode ? "frame #" : "#") == (lldb_mode ? 4 : 0))
				current.lines.emplace_back(line);
		}

		if(current.lines)
			out.emplace_back(move(current));
		return out;
	}

	int mainThreadPos(const ThreadInfo &info) {
		for(int n = 0; n < info.lines.size(); n++) {
			const auto &line = info.lines[n];
			if(line.find("fwk::Backtrace::fullBacktrace") != string::npos)
				return n;
		}
		return -1;
	}

	void processThreadInfo(ThreadInfo &info, int skip_frames, bool lldb_mode) {
		int main_pos = mainThreadPos(info);
		info.is_main = main_pos != -1;
		skip_frames = main_pos == -1 ? 0 : main_pos + skip_frames;

		for(auto line : info.lines) {
			if(skip_frames-- > 0)
				continue;

			auto tokens = tokenize(line.c_str());

			if(lldb_mode) {
				int cut = -1;
				for(int n = 0; n < tokens.size(); n++)
					if(tokens[n] == "frame") {
						cut = n + 2;
						break;
					}
				if(cut <= tokens.size())
					tokens.erase(tokens.begin(), tokens.begin() + cut);
			} else if(tokens.size() > 3) {
				int count = tokens[2] == "in" ? 3 : 1;
				tokens.erase(tokens.begin(), tokens.begin() + count);
			}

			string function, file_line;

			int split_pos = -1;
			for(int i = tokens.size() - 1; i >= 0; i--)
				if(tokens[i] == "at" && i > 0 && (tokens[i - 1].back() == ')' || lldb_mode)) {
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

				tokens = tokenize(file_line.c_str(), ':');
				if(tokens.size() == 2) {
					new_entry.file = tokens[0];
					new_entry.line = tokens[1];
				}
			}

			info.entries.emplace_back(new_entry);
		}
	}

	string filterDebuggerOutput(string input, int skip_frames, bool lldb_mode) {
		string out;

		auto tinfos = splitDebuggerInput(input, lldb_mode);

		int main_thread_pos = -1;
		for(auto &tinfo : tinfos)
			processThreadInfo(tinfo, skip_frames, lldb_mode);
		std::reverse(begin(tinfos), end(tinfos));

		int num_columns = 120;
		if(auto cols = consoleColumns())
			num_columns = *cols;

		int limit_line_size = 6;
		int limit_func_size = max(20, num_columns * 3 / 4);
		int limit_file_size = max(16, num_columns - limit_line_size - limit_func_size);

		int max_lsize = 0, max_fsize = 0;
		for(auto &tinfo : tinfos)
			for(auto &e : tinfo.entries) {
				e.file = Str(e.file).limitSizeFront(limit_file_size);
				e.line = Str(e.line).limitSizeBack(limit_line_size);
				e.function = Str(e.function).limitSizeBack(limit_func_size);

				max_lsize = max(max_lsize, (int)e.line.size());
				max_fsize = max(max_fsize, (int)e.file.size());
			}

		bool show_headers = tinfos.size() > 1;
		for(auto &tinfo : tinfos) {
			string tout;
			if(show_headers)
				tout += string("---- ") + (tinfo.is_main ? "(CURRENT) " : "") + tinfo.header + "\n";

			for(auto &e : tinfo.entries) {
				if(e.file.empty() || e.line.empty() || e.function.empty()) {
					tout += string(max_fsize + max_lsize + 2, ' ') +
							Str(e.simple).limitSizeBack(limit_func_size) + "\n";
				} else {
					tout += string(max(max_fsize - (int)e.file.size(), 0), ' ') + e.file + ":" +
							e.line + string(max(max_lsize - (int)e.line.size(), 0), ' ') + " " +
							e.function + "\n";
				}
			}

			if(&tinfo != &tinfos.back())
				tout += "\n";
			out += tout;
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

string Backtrace::g_lldb_command;
__thread BacktraceMode Backtrace::t_default_mode = Mode::fast;

Backtrace::Backtrace(vector<void *> addresses, vector<string> symbols)
	: m_addresses(move(addresses)), m_symbols(move(symbols)) {}
Backtrace::Backtrace(vector<void *> addresses, vector<string> symbols,
					 Pair<string, bool> gdb_result)
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
		return {move(addrs), move(symbols), fullBacktrace(skip)};
#endif

	return {move(addrs), move(symbols)};
}

Pair<string, bool> Backtrace::fullBacktrace(int skip_frames) {
	bool lldb_mode = !g_lldb_command.empty();
	printf("Generating backtrace with %s; Warning: sometimes debuggers lie...\n",
		   lldb_mode ? "LLDB" : "GDB");

#ifdef FWK_TARGET_LINUX
	auto pid = getpid();
	string cmd;
	if(lldb_mode)
		cmd = format("% 2>&1 -batch -p % -o 'thread backtrace all'", g_lldb_command, (int)pid);
	else
		cmd = format("gdb 2>&1 -batch -p % -ex 'thread apply all bt'", (int)pid);
	auto result = execCommand(cmd);

	// TODO: check for errors from LLDB
	if(!lldb_mode && result.first.find("ptrace: Operation not permitted") != string::npos)
		return {"To use GDB stacktraces, you have to:\n"
				"1) set kernel.yama.ptrace_scope to 0 in: /etc/sysctl.d/10-ptrace.conf\n"
				"2) type: echo 0 > /proc/sys/kernel/yama/ptrace_scope\n",
				false};

	return {filterDebuggerOutput(result.first, skip_frames, lldb_mode), true};
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
	if(m_addresses)
		formatter("Please run following command:\n% | c++filt\n",
				  analyzeCommand(m_addresses, true));
	else
		formatter("Empty backtrace\n");
#endif
	if(file_lines) {
		int max_len = 0;
		for(const auto &file_line : file_lines)
			max_len = max(max_len, (int)file_line.size());

		formatter("Simple bactrace (don't expect it to be accurate):\n");
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
}
