/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_base.h"
#include <cstdio>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>

#ifdef FWK_TARGET_LINUX
#include <execinfo.h>
#include <dlfcn.h>
#include <unistd.h>
#endif

#ifdef FWK_TARGET_MINGW
#include <windows.h>
#include <imagehlp.h>
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

	string analyzeCommand(std::vector<void *> addresses, bool funcs = false) {
		TextFormatter command;
		command("addr2line ");
		for(auto address : addresses)
			command("%p ", address);
		command("%s-e %s 2>/dev/null", funcs ? "-f -p " : "",
				executablePath().relative(FilePath::current()).c_str());
		return command.text();
	}

	std::vector<string> analyzeAddresses(std::vector<void *> addresses) {
		string result = execCommand(analyzeCommand(addresses)).first;
		std::vector<string> file_lines;
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
			file_line = file.empty() ? "?" : format("%s:%d", file.c_str(), line);
		}
		return file_lines;
	}

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

Backtrace::Backtrace(std::vector<void *> addresses, std::vector<string> symbols)
	: m_addresses(std::move(addresses)), m_symbols(std::move(symbols)) {}

Backtrace Backtrace::get(size_t skip, void *context_) {
	std::vector<void *> addrs;
	std::vector<string> symbols;

#if defined(FWK_TARGET_MINGW)
	if(!context_) {
		addrs.resize(64);
		int count = CaptureStackBackTrace(max(0, (int)skip - 1), addrs.size(), &addrs[0], 0);
		addrs.resize(count);
	} else {
		CONTEXT *context = static_cast<CONTEXT *>(context_);
		SymInitialize(GetCurrentProcess(), 0, true);

		STACKFRAME frame;
		memset(&frame, 0, sizeof(frame));

#ifdef AMD64
		frame.AddrPC.Offset = context->Rip;
		frame.AddrStack.Offset = context->Rsp;
		frame.AddrFrame.Offset = context->Rsp;
#else
		frame.AddrPC.Offset = context->Eip;
		frame.AddrStack.Offset = context->Esp;
		frame.AddrFrame.Offset = context->Ebp;
#endif
		frame.AddrPC.Mode = AddrModeFlat;
		frame.AddrStack.Mode = AddrModeFlat;
		frame.AddrFrame.Mode = AddrModeFlat;

#ifdef AMD64
		while(StackWalk64(IMAGE_FILE_MACHINE_AMD64,
#else
		while(StackWalk(IMAGE_FILE_MACHINE_I386,
#endif
						  GetCurrentProcess(), GetCurrentThread(), &frame, context, 0,
						  SymFunctionTableAccess, SymGetModuleBase, 0)) {
			addrs.emplace_back((void *)frame.AddrPC.Offset);
		}

		SymCleanup(GetCurrentProcess());
	}

#elif defined(FWK_TARGET_LINUX)

	void *addresses[64];
	size_t size = ::backtrace(addresses, arraySize(addresses));
	char **strings = backtrace_symbols(addresses, size);

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
#endif

	return Backtrace(std::move(addrs), std::move(symbols));
}

string Backtrace::analyze(bool filter) const {
	TextFormatter formatter;
	std::vector<string> file_lines;

#if defined(FWK_TARGET_HTML5)
#elif defined(FWK_TARGET_LINUX)
	file_lines = analyzeAddresses(m_addresses);
#elif defined(FWK_TARGET_MINGW)
	formatter("Please run following command:\n%s | c++filt\n",
			  analyzeCommand(m_addresses, true).c_str());
#endif
	if(!file_lines.empty()) {
		int max_len = 0;
		for(const auto &file_line : file_lines)
			max_len = max(max_len, (int)file_line.size());

		for(size_t i = 0; i < size(); i++) {
			string tstring = i < m_symbols.size() ? m_symbols[i] : "";
			const char *file_line = i < file_lines.size() ? file_lines[i].c_str() : "";
			tstring = tstring.substr(0, tstring.find('['));

			string fmt = format("%%%ds %%s\n", max_len);
			formatter(fmt.c_str(), file_line, tstring.c_str());
		}
	}

	string out = formatter.text();
	return filter ? Backtrace::filter(out) : out;
}

string Backtrace::filter(const string &input) {
#ifdef FWK_TARGET_LINUX
	string command = "echo \"" + input + "\" | c++filt -n";

	FILE *file = popen(command.c_str(), "r");
	if(file) {
		std::vector<char> buf;
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
#endif

	return input;
}
}
