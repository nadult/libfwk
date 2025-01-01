// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/platform.h"

#ifdef FWK_PLATFORM_WINDOWS
#include "../sys/windows.h"
#include <direct.h>
#include <io.h>
#else
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <unistd.h>
#endif

#ifdef FWK_PLATFORM_MSVC
#define S_IFMT 0xF000
#define S_IFREG 0x8000
#define S_IFDIR 0x4000
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif

#include "fwk/io/file_system.h"

#include "fwk/format.h"
#include "fwk/io/file_stream.h"
#include "fwk/parse.h"
#include "fwk/sys/expected.h"
#include "fwk/vector.h"

#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <sys/types.h>

namespace fwk {

// TODO: move to FilePath ?
FilePath executablePath() {
#if defined(FWK_PLATFORM_HTML)
	return "/emscripten_binary";
#elif defined(FWK_PLATFORM_WINDOWS)
	char path[MAX_PATH];
	GetModuleFileName(GetModuleHandleW(0), path, arraySize(path));
	return path;
#else
	char name[512];
	int ret = readlink("/proc/self/exe", name, sizeof(name) - 1);
	if(ret == -1)
		return "";
	name[ret] = 0;
	return name;
#endif
}

FilePath::Element FilePath::extractRoot(const char *str) {
#ifdef FWK_PLATFORM_WINDOWS
	if((str[0] >= 'a' && str[0] <= 'z') || (str[0] >= 'A' && str[0] <= 'Z'))
		if(str[1] == ':' && (str[2] == '/' || str[2] == '\\'))
			return Element{str, 3};

	return Element{nullptr, 0};
#else
	if(str[0] == '/')
		return Element{str, 1};
	return Element{nullptr, 0};
#endif
}

Ex<FilePath> FilePath::current() {
#ifdef FWK_PLATFORM_WINDOWS
	char buf[MAX_PATH];
	if(!GetCurrentDirectory(sizeof(buf), buf))
		return FWK_ERROR("Error in GetCurrentDirectory");
	return FilePath(buf);
#else
	char buffer[512];
	char *name = getcwd(buffer, sizeof(buffer) - 1);
	if(!name)
		return ERROR("Error in getcwd: %", strerror(errno));
	return FilePath(name);
#endif
}

Ex<FilePath> FilePath::home() {
#ifdef FWK_PLATFORM_WINDOWS
	Str part1 = getenv("HOMEDRIVE");
	Str part2 = getenv("HOMEPATH");
	if(!part1 || !part2)
		return FWK_ERROR("Error while reading 'HOMEDRIVE', 'HOMEPATH' environment variables");
	return FilePath(format("%%", part1, part2));
#else
	Str path = getenv("HOME");
	if(!path)
		return FWK_ERROR("Error while reading 'HOME' environment variable");
	return FilePath(path);
#endif
}

Ex<void> FilePath::setCurrent(const FilePath &path) {
#ifdef FWK_PLATFORM_WINDOWS
	if(!SetCurrentDirectory(path.c_str()))
		return FWK_ERROR("Error in SetCurrentDirectory(%)", path);
	return {};
#else
	if(chdir(path.c_str()) != 0)
		return ERROR("Error in chdir: %", strerror(errno));
	return {};
#endif
}

bool FilePath::isRegularFile() const {
#ifdef FWK_PLATFORM_WINDOWS
	struct _stat buf;
	_stat(c_str(), &buf);
	return S_ISREG(buf.st_mode);
#else
	struct stat buf;
	if(lstat(c_str(), &buf) != 0)
		return false;
	return S_ISREG(buf.st_mode);
#endif
}

bool FilePath::isDirectory() const {
#ifdef FWK_PLATFORM_WINDOWS
	struct _stat buf;
	_stat(c_str(), &buf);
	return S_ISDIR(buf.st_mode);
#else
	struct stat buf;
	if(lstat(c_str(), &buf) != 0)
		return false;
	return S_ISDIR(buf.st_mode) || S_ISLNK(buf.st_mode);
#endif
}

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

Str FilePath::fileName() const & {
	auto it = m_path.rfind('/');
	if(it == string::npos || (it == 0 && m_path.size() == 1))
		return m_path;
	return Str(m_path).substr(it + 1);
}

Maybe<Str> FilePath::fileExtension() const & { return fwk::fileNameExtension(m_path); }
Str FilePath::fileStem() const & { return fwk::fileNameStem(m_path); }

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

bool FilePath::hasTildePrefix() const { return m_path[0] == '~' && m_path[1] == '/'; }
FilePath FilePath::replaceTildePrefix(const FilePath &home) const {
	if(hasTildePrefix())
		return home / FilePath(m_path.substr(2));
	return *this;
}

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

bool FileEntry::operator<(const FileEntry &rhs) const {
	if(is_dir != rhs.is_dir || is_link != rhs.is_link) {
		auto lval = is_dir || is_link, rval = rhs.is_dir || rhs.is_link;
		if(lval != rval)
			return lval > rval;
		int val = Str(path).compare(rhs.path);
		if(val != 0)
			return val < 0;
		return tie(is_dir, is_link) < tie(rhs.is_dir, rhs.is_link);
	}
	return path < rhs.path;
}

Maybe<Str> fileNameExtension(Str str) {
	auto slash_pos = str.rfind('/');
	if(slash_pos != -1)
		str = str.substr(slash_pos + 1);
	auto dot_pos = str.rfind('.');
	return dot_pos == -1 ? Maybe<Str>() : str.substr(dot_pos + 1);
}

Str fileNameStem(Str str) {
	auto slash_pos = str.rfind('/');
	if(slash_pos != -1)
		str = str.substr(slash_pos + 1);
	auto dot_pos = str.rfind('.');
	return dot_pos == -1 ? str : str.substr(0, dot_pos);
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
		return FWK_ERROR("stat failed for file %s: %s\n", file_name.c_str(), strerror(errno));
	return double(attribs.st_mtime);
#else
	struct stat attribs;
	if(stat(file_name.c_str(), &attribs) != 0)
		return FWK_ERROR("stat failed for file %s: %s\n", file_name.c_str(), strerror(errno));
	return double(attribs.st_mtim.tv_sec) + double(attribs.st_mtim.tv_nsec) * 0.000000001;
#endif
}

Ex<> mkdirRecursive(const FilePath &path) {
	if(access(path))
		return {};

	FilePath parent = path.parent();
	if(!access(parent))
		if(auto result = mkdirRecursive(parent); !result)
			return result;

#ifdef _WIN32
	int ret = _mkdir(path.c_str());
#else
	int ret = mkdir(path.c_str(), 0775);
#endif

	if(ret != 0)
		return FWK_ERROR("Cannot create directory: \"%s\" error: %s\n", path.c_str(),
						 strerror(errno));
	return {};
}

Ex<> removeFile(const FilePath &path) {
	auto ret = std::remove(path.c_str());
	if(ret != 0)
		return FWK_ERROR("Cannot remove file: \"%s\" error: %s\n", path.c_str(), strerror(errno));
	return {};
}

Ex<> renameFile(const FilePath &src, const FilePath &dst) {
	auto ret = std::rename(src.c_str(), dst.c_str());
	if(ret != 0)
		return FWK_ERROR("Cannot rename/move file: \"%s\" to: \"%s\" error: %s\n", src.c_str(),
						 dst.c_str(), strerror(errno));
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

	auto opts = FindFileOpt::recursive | FindFileOpt::absolute | FindFileOpt::regular_file;
	auto entries = findFiles(path, opts);
	vector<string> out;

	for(auto &entry : entries) {
		string temp = entry.path;
		if(removePrefix(temp, full_prefix) && removeSuffix(temp, suffix))
			out.emplace_back(temp);
	}

	return out;
}

using Opt = FindFileOpt;

static void findFiles(vector<FileEntry> &out, const FilePath &path, const FilePath &append,
					  FindFileOpts opts) {
#ifdef FWK_PLATFORM_WINDOWS
	WIN32_FIND_DATA data;
	char tpath[MAX_PATH];
	snprintf(tpath, sizeof(tpath), "%s/*", path.c_str());
	HANDLE handle = FindFirstFile(tpath, &data);

	bool is_root = path.isRoot();
	bool ignore_parent = !(opts & Opt::include_parent) || is_root;

	if(handle != INVALID_HANDLE_VALUE)
		do {
			bool is_current = strcmp(data.cFileName, ".") == 0;
			bool is_parent = strcmp(data.cFileName, "..") == 0;

			if(is_current || (ignore_parent && is_parent))
				continue;

			bool is_dir = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
			bool is_file = !is_dir;
			bool do_accept =
				((opts & Opt::regular_file) && is_file) || ((opts & Opt::directory) && is_dir);

			if(do_accept) {
				FileEntry entry;
				entry.path = append / FilePath(data.cFileName);
				entry.is_dir = is_dir;
				entry.is_link = false;
				out.push_back(entry);
			}

			if(is_dir && (opts & Opt::recursive) && !is_parent)
				findFiles(out, path / FilePath(data.cFileName), append / FilePath(data.cFileName),
						  opts);
		} while(FindNextFile(handle, &data));
#else
	if constexpr(platform == Platform::html) {
		// TODO: opendir fails on these...
		if(Str(path).find("/proc/self/fd") == 0)
			return;
	}

	DIR *dp = opendir(path.c_str());
	if(!dp)
		return;
	bool is_root = path.isRoot();
	bool ignore_parent = !(opts & Opt::include_parent) || is_root;

	{
		struct dirent *dirp;

		while((dirp = readdir(dp))) {
			bool is_parent = strcmp(dirp->d_name, "..") == 0;
			bool is_current = strcmp(dirp->d_name, ".") == 0;

			if(is_current || (ignore_parent && is_parent))
				continue;

			bool is_directory = false, is_regular = false, is_link = false;
#ifdef _DIRENT_HAVE_D_TYPE
			is_directory = dirp->d_type == DT_DIR;
			is_regular = dirp->d_type == DT_REG;
			is_link = dirp->d_type == DT_LNK;
			// TODO: fix this
			// if(dirp->d_type == DT_LNK)
			// 	dirp->d_type = DT_DIR;

			if(dirp->d_type == DT_UNKNOWN)
#endif
			{
				char full_path[FILENAME_MAX];
				struct stat buf;

				snprintf(full_path, sizeof(full_path), "%s/%s", path.c_str(), dirp->d_name);
				lstat(full_path, &buf);
				is_directory = S_ISDIR(buf.st_mode);
				is_regular = S_ISREG(buf.st_mode);
				is_link = S_ISLNK(buf.st_mode);
			}

			bool do_accept = ((opts & Opt::regular_file) && is_regular) ||
							 ((opts & Opt::directory) && is_directory) ||
							 ((opts & Opt::link) && is_link);

			// TODO: check why was this added
			//	if(do_accept && is_root && is_directory)
			//		do_accept = false;

			if(do_accept)
				out.emplace_back(append / FilePath(dirp->d_name), is_directory, is_link);

			if(is_directory && (opts & Opt::recursive) && !is_parent)
				findFiles(out, path / FilePath(dirp->d_name), append / FilePath(dirp->d_name),
						  opts);
		}
	}

	// TODO: finally: closedir ?
	closedir(dp);
#endif
}

vector<FileEntry> findFiles(const FilePath &path, FindFileOpts opts) {
	vector<FileEntry> out;

	auto abs_path = path.absolute();
	if(!abs_path)
		return {};

	auto append = opts & Opt::relative ? "." : opts & Opt::absolute ? *abs_path : path;
	findFiles(out, *abs_path, append, opts);
	return out;
}

// TODO: stdout and stderr returned separately?
Ex<Pair<string, int>> execCommand(const string &cmd) {
#ifdef FWK_PLATFORM_HTML
	return FWK_ERROR("execCommand not supported on HTML platform");
#else
#ifdef FWK_PLATFORM_MSVC
	FILE *pipe = _popen(cmd.c_str(), "r");
#else
	FILE *pipe = popen(cmd.c_str(), "r");
#endif
	if(!pipe)
		return FWK_ERROR("Error on popen '%': %", cmd, strerror(errno));
	char buffer[1024];
	std::string result = "";
	while(!feof(pipe)) {
		if(fgets(buffer, sizeof(buffer), pipe))
			result += buffer;
	}
#ifdef FWK_PLATFORM_MSVC
	int ret = _pclose(pipe);
#else
	int ret = pclose(pipe);
#endif

	if(ret == -1)
		return FWK_ERROR("Error on pclose '%': %", cmd, strerror(errno));
	return pair{result, ret};
#endif
}

Ex<string> loadFileString(ZStr file_name, int max_size) {
	auto file = EX_PASS(fileLoader(file_name));

	if(file.size() > max_size)
		return FWK_ERROR("File '%' size too big: % > %", file_name, file.size(), max_size);
	string out(file.size(), ' ');
	file.loadData(out);
	return out;
}

Ex<vector<char>> loadFile(ZStr file_name, int max_size) {
	auto file = EX_PASS(fileLoader(file_name));

	if(file.size() > max_size)
		return FWK_ERROR("File '%' size too big: % > %", file_name, file.size(), max_size);
	PodVector<char> out(file.size());
	file.loadData(out);
	EXPECT(file.getValid());
	vector<char> vout;
	out.unsafeSwap(vout);
	return vout;
}

Ex<> saveFile(ZStr file_name, CSpan<char> data) {
	auto file = EX_PASS(fileSaver(file_name));
	file.saveData(data);
	return {};
}
}
