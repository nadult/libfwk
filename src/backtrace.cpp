/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_base.h"
#include <cstdio>

namespace fwk {

Backtrace::Backtrace(vector<void *> addresses, vector<string> symbols)
	: m_addresses(std::move(addresses)), m_symbols(std::move(symbols)) {}
}

#ifdef FWK_TARGET_LINUX

#include <execinfo.h>
#include <dlfcn.h>
#include <unistd.h>

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
}

Backtrace Backtrace::get(size_t skip) {
	void *addresses[64];
	size_t size = ::backtrace(addresses, arraySize(addresses));
	char **strings = backtrace_symbols(addresses, size);

	vector<void *> addrs;
	vector<string> symbols;

	try {
		for(size_t i = skip; i < size; i++) {
			addrs.emplace_back(addresses[i]);
			symbols.emplace_back(string(strings[i]));
		}
	} catch(...) {
		free(strings);
		return Backtrace();
	}
	free(strings);

	return Backtrace(std::move(addrs), std::move(symbols));
}

string Backtrace::analyze(bool filter) const {
	// TODO: these are not exactly correct (inlining?)
	TextFormatter command;
	command("addr2line ");
	for(auto address : m_addresses)
		command("%p ", address);
	command("-e %s 2>/dev/null", executablePath().c_str());
	string result = execCommand(command.text()).first;
	vector<string> file_lines;
	while(!result.empty()) {
		auto pos = result.find('\n');
		file_lines.emplace_back(result.substr(0, pos));
		if(pos != string::npos)
			pos++;
		result = result.substr(pos);
	}
	file_lines.resize(m_addresses.size());

	for(auto &file_line : file_lines) {
		auto colon_pos = file_line.find(':');
		string file = nicePath(file_line.substr(0, colon_pos));
		int line = colon_pos == string::npos ? 0 : atoi(&file_line[colon_pos + 1]);
		file_line = file.empty() ? "?" : format("%s:%d", file.c_str(), line);
	}

	int max_len = 0;
	for(const auto &file_line : file_lines)
		max_len = max(max_len, (int)file_line.size());

	TextFormatter formatter;
	for(size_t i = 0; i < size(); i++) {
		string tstring = i < m_symbols.size() ? m_symbols[i] : "";
		const char *file_line = i < file_lines.size() ? file_lines[i].c_str() : "";
		tstring = tstring.substr(0, tstring.find('['));

		string fmt = format("%%%ds %%s\n", max_len);
		formatter(fmt.c_str(), file_line, tstring.c_str());
	}

	string out = formatter.text();
	return filter ? Backtrace::filter(out) : out;
}

namespace {
	void filterString(string &str, const char *src, const char *dst) {
		DASSERT(src && dst);
		int src_len = strlen(src);

		auto pos = str.find(src);
		while(pos != std::string::npos) {
			str = str.substr(0, pos) + dst + str.substr(pos + src_len);
			pos = str.find(src);
		}
	}

	const char *s_filtered_names[] = {
		"unsigned int", "uint",
		"std::basic_string<char, std::char_traits<char>, std::allocator<char> >", "fwk::string",
		"std::", "", "fwk::", "",
	};
}

string Backtrace::filter(const string &input) {
	string command = "echo \"" + input + "\" | c++filt -n";

	FILE *file = popen(command.c_str(), "r");
	if(file) {
		vector<char> buf;
		try {
			while(!feof(file) && buf.size() < 1024 * 16) {
				buf.push_back(fgetc(file));
				if((u8)buf.back() == 255) {
					buf.pop_back();
					break;
				}
			}
		} catch(...) {
			fclose(file);
			throw;
		}
		fclose(file);

		string out(&buf[0], buf.size());
		for(int n = 0; n < (int)arraySize(s_filtered_names); n += 2)
			filterString(out, s_filtered_names[n], s_filtered_names[n + 1]);
		return out;
	}
	return input;
}
}

#else

namespace fwk {

Backtrace Backtrace::get(size_t skip) { return Backtrace(); }
string Backtrace::analyze(bool) const { return "Backtraces in LibFWK are supported only on Linux"; }
}

#endif
