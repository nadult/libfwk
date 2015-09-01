/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_base.h"
#include <cstdio>

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
		"std::basic_string<char, std::char_traits<char>, std::allocator<char> >", "fwk::string",
		"std::", "", "fwk::", "",
	};
}

string backtrace(size_t skip) {
	TextFormatter out;

	void *addresses[64];
	size_t size = ::backtrace(addresses, arraySize(addresses));
	char **strings = backtrace_symbols(addresses, size);

	// TODO: these are not exactly correct (inlining?)
	string file_lines[64];
	string exec_name = executablePath();
	for(size_t i = skip; i < size; i++) {
		string file_line = execCommand(format("addr2line %p -e %s 2>/dev/null", addresses[i],
											  exec_name.c_str())).first;
		auto colon_pos = file_line.find(':');
		string file = nicePath(file_line.substr(0, colon_pos));
		int line = colon_pos == string::npos ? 0 : atoi(&file_line[colon_pos + 1]);
		file_lines[i] = file.empty() ? "?" : format("%s:%d", file.c_str(), line);
	}

	int max_len = 0;
	for(const auto &file_line : file_lines)
		max_len = max(max_len, (int)file_line.size());

	for(size_t i = skip; i < size; i++) {
		string tstring = strings[i];
		tstring = tstring.substr(0, tstring.find('['));

		string fmt = format("%%%ds %%s\\n", max_len);
		out(fmt.c_str(), file_lines[i].c_str(), tstring.c_str());
	}
	free(strings);

	return out.text();
}

string cppFilterBacktrace(const string &input) {
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

string backtrace(size_t skip) { return "Backtraces in LibFWK are supported only on Linux"; }
string cppFilterBacktrace(const string &input) { return input; }
}

#endif
