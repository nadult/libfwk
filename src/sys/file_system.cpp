// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/file_system.h"

#include "fwk/format.h"
#include "fwk/parse.h"
#include "fwk/sys/expected.h"
#include "fwk/sys/file_stream.h"
#include "fwk/vector.h"

#include <cstdio>
#include <cstring>

#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32

#include <direct.h>
#include <io.h>

#else

#include <errno.h>
#include <unistd.h>

#endif

namespace fwk {

bool FilePath::Element::isDot() const { return size == 1 && ptr[0] == '.'; }

bool FilePath::Element::isDots() const { return size == 2 && ptr[0] == '.' && ptr[1] == '.'; }

bool FilePath::Element::isRoot() const {
	return size && (ptr[size - 1] == '/' || ptr[size - 1] == '\\');
}

bool FilePath::Element::operator==(const Element &rhs) const {
	return size == rhs.size && strncmp(ptr, rhs.ptr, size) == 0;
}

FilePath::FilePath(Str path) {
	vector<Element> elements;
	elements.reserve(32);
	divide(path, elements);
	construct(elements);
}

FilePath::FilePath(FilePath &&ref) { ref.m_path.swap(m_path); }
FilePath::FilePath(const char *zstr) : FilePath(Str(zstr)) {}
FilePath::FilePath(const string &str) : FilePath(Str(str)) {}
FilePath::FilePath(const FilePath &rhs) : m_path(rhs.m_path) {}
FilePath::FilePath() : m_path(".") {}

void FilePath::divide(Str str, vector<Element> &out) {
	auto ptr = str.begin(), end = str.end();

	Element root = extractRoot(ptr);
	if(root.ptr) {
		ptr += root.size;
		out.push_back(root);
	}

	const char *prev = ptr;
	do {
		if(*ptr == '/' || *ptr == '\\' || ptr >= end) {
			if(ptr - prev > 0)
				out.push_back(Element{prev, (int)(ptr - prev)});
			if(!*ptr)
				break;
			prev = ptr + 1;
		}
		ptr++;
	} while(true);
}

void FilePath::simplify(const vector<Element> &src, vector<Element> &dst) {
	for(int n = 0; n < (int)src.size(); n++) {
		if(src[n].isDot())
			continue;
		if(src[n].isDots() && dst && !dst.back().isDots()) {
			DASSERT(!dst.back().isRoot());
			dst.pop_back();
		} else
			dst.push_back(src[n]);
	}
}

void FilePath::construct(const vector<Element> &input) {
	vector<Element> elements;
	elements.reserve(input.size());
	simplify(input, elements);

	if(!elements) {
		m_path = ".";
		return;
	}

	int length = 0;
	for(int n = 0; n < (int)elements.size(); n++)
		length += elements[n].size;
	length += (int)elements.size() - 1;
	if(elements.size() > 1 && elements.front().isRoot())
		length--;

	string new_path(length, ' ');
	length = 0;
	for(int n = 0; n < (int)elements.size(); n++) {
		const Element &elem = elements[n];
		memcpy(&new_path[length], elem.ptr, elem.size);
		if(elem.isRoot()) {
			for(int i = 0; i < elem.size; i++) {
				char c = new_path[length + i];
				if(c >= 'a' && c <= 'z')
					c = c + 'A' - 'a';
				if(c == '\\')
					c = '/';
				new_path[length + i] = c;
			}
		}
		length += elem.size;

		if(n + 1 < (int)elements.size() && !elem.isRoot())
			new_path[length++] = '/';
	}

	new_path[length] = 0;
	m_path.swap(new_path);
}

string FilePath::fileName() const {
	auto it = m_path.rfind('/');
	if(it == string::npos || (it == 0 && m_path.size() == 1))
		return m_path;
	return m_path.substr(it + 1);
}

string FilePath::fileExtension() const {
	string name = fileName();
	auto dot_pos = name.rfind('.');
	return dot_pos == string::npos ? "" : name.substr(dot_pos + 1);
}

bool FilePath::isRoot() const {
	// Ending backslash is stripped from folder paths
	return m_path.back() == '/';
}

bool FilePath::isAbsolute() const { return extractRoot(c_str()).size != 0; }

FilePath FilePath::relative(const FilePath &ref) const {
	DASSERT(ref.isAbsolute());

	vector<Element> celems, relems;
	celems.reserve(32);
	relems.reserve(32);
	divide(m_path.c_str(), celems);
	divide(ref.m_path.c_str(), relems);

	vector<Element> oelems;
	oelems.reserve(32);

	int n = 0;
	for(int count = min(celems.size(), relems.size()); n < count; n++)
		if(!(celems[n] == relems[n]))
			break;
	for(int i = n; i < (int)relems.size(); i++)
		oelems.push_back(Element{"..", 2});
	for(int i = n; i < (int)celems.size(); i++)
		oelems.push_back(celems[i]);

	FilePath out;
	out.construct(oelems);
	return out;
}

Ex<FilePath> FilePath::relativeToCurrent() const {
	auto cur = current();
	return cur ? relative(*cur) : Ex<FilePath>(cur.error());
}

bool FilePath::isRelative(const FilePath &ref) const {
	DASSERT(ref.isAbsolute() && isAbsolute());

	vector<Element> celems, relems;
	celems.reserve(32);
	relems.reserve(32);
	divide(m_path.c_str(), celems);
	divide(ref.m_path.c_str(), relems);

	for(int count = min(celems.size(), relems.size()), n = 0; n < count; n++)
		if(!(celems[n] == relems[n]))
			return false;
	return true;
}

FilePath FilePath::absolute(const FilePath &current) const {
	return isAbsolute() ? *this : current / *this;
}
Ex<FilePath> FilePath::absolute() const {
	auto cur = current();
	return cur ? absolute(*cur) : Ex<FilePath>(cur.error());
}

FilePath FilePath::parent() const { return *this / ".."; }

FilePath FilePath::operator/(const FilePath &other) const {
	FilePath out = *this;
	out /= other;
	return out;
}

FilePath &FilePath::operator/=(const FilePath &other) {
	DASSERT(!other.isAbsolute());

	vector<Element> elems;
	elems.reserve(32);
	divide(m_path.c_str(), elems);
	divide(other.m_path.c_str(), elems);

	construct(elems);
	return *this;
}

void FilePath::operator>>(TextFormatter &fmt) const { fmt << operator ZStr(); }

TextParser &operator>>(TextParser &parser, FilePath &path) {
	string text;
	parser >> text;
	path = text;
	return parser;
}

bool removeSuffix(string &str, const string &suffix) {
	if(str.size() >= suffix.size() && suffix == str.c_str() + str.size() - suffix.size()) {
		str = str.substr(0, str.size() - suffix.size());
		return true;
	}

	return false;
}

bool removePrefix(string &str, const string &prefix) {
	if(str.size() >= prefix.size() && memcmp(str.c_str(), prefix.c_str(), prefix.size()) == 0) {
		str = str.substr(prefix.size());
		return true;
	}

	return false;
}

bool access(const FilePath &path) {
#ifdef _WIN32
	return _access(path.c_str(), 0) == 0;
#else
	return ::access(path.c_str(), F_OK) == 0;
#endif
}

Ex<double> lastModificationTime(const FilePath &file_name) {
#ifdef _WIN32
	struct _stat64 attribs;
	if(_stat64(file_name.c_str(), &attribs) != 0)
		return Error(format("stat failed for file %: %\n", file_name, strerror(errno)));
	return double(attribs.st_mtime);
#else
	struct stat attribs;
	if(stat(file_name.c_str(), &attribs) != 0)
		return Error(format("stat failed for file %: %\n", file_name, strerror(errno)));
	return double(attribs.st_mtim.tv_sec) + double(attribs.st_mtim.tv_nsec) * 0.000000001;
#endif
}

Ex<void> mkdirRecursive(const FilePath &path) {
	if(access(path))
		return {};

	FilePath parent = path.parent();
	if(!access(parent))
		if(auto result = mkdirRecursive(parent); !result)
			return result;

#ifdef _WIN32
	int ret = mkdir(path.c_str());
#else
	int ret = mkdir(path.c_str(), 0775);
#endif

	if(ret != 0)
		return Error(format("Cannot create directory: \"%\" error: %\n", path, strerror(errno)));
	return {};
}

vector<string> findFiles(const string &prefix, const string &suffix) {
	auto abs_path = FilePath(prefix).absolute();
	if(!abs_path)
		return {};
	string full_prefix = *abs_path;
	if(prefix.back() == '/')
		full_prefix.push_back('/');
	FilePath path(prefix);
	if(path.isRegularFile())
		path = path.parent();

	auto entries =
		findFiles(path, FindFiles::recursive | FindFiles::absolute | FindFiles::regular_file);
	vector<string> out;

	for(auto &entry : entries) {
		string temp = entry.path;
		if(removePrefix(temp, full_prefix) && removeSuffix(temp, suffix))
			out.emplace_back(temp);
	}

	return out;
}

// TODO: stdout and stderr returned separately?
Ex<Pair<string, int>> execCommand(const string &cmd) {
	FILE *pipe = popen(cmd.c_str(), "r");
	if(!pipe)
		return ERROR("Error on popen '%': %", cmd, strerror(errno));
	char buffer[1024];
	std::string result = "";
	while(!feof(pipe)) {
		if(fgets(buffer, sizeof(buffer), pipe))
			result += buffer;
	}
	int ret = pclose(pipe);
	if(ret == -1)
		return ERROR("Error on pclose '%': %", cmd, strerror(errno));
	return pair{result, ret};
}

Ex<string> loadFileString(ZStr file_name, int max_size) {
	auto file = EXPECT_PASS(fileLoader(file_name));

	if(file.size() > max_size)
		return ERROR("File '%' size too big: % > %", file_name, file.size(), max_size);
	string out(file.size(), ' ');
	file.loadData(out);
	return out;
}

Ex<vector<char>> loadFile(ZStr file_name, int max_size) {
	auto file = EXPECT_PASS(fileLoader(file_name));

	if(file.size() > max_size)
		return ERROR("File '%' size too big: % > %", file_name, file.size(), max_size);
	PodVector<char> out(file.size());
	file.loadData(out);
	EXPECT_CATCH();
	vector<char> vout;
	out.unsafeSwap(vout);
	return vout;
}

Ex<void> saveFile(ZStr file_name, CSpan<char> data) {
	auto file = EXPECT_PASS(fileSaver(file_name));
	file.saveData(data);
	return {};
}
}
