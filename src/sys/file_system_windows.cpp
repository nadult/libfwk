// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#if !defined(FWK_TARGET_MINGW)
#error "This file should only be compiled for MinGW target"
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef ERROR

#include "fwk/sys/file_system.h"

#include "fwk/sys/expected.h"
#include "fwk/vector.h"
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

// TODO: verify that it works on different drives?

namespace fwk {

FilePath executablePath() {
	char path[MAX_PATH];
	GetModuleFileName(GetModuleHandleW(0), path, arraySize(path));
	return path;
}

FilePath::Element FilePath::extractRoot(const char *str) {
	if((str[0] >= 'a' && str[0] <= 'z') || (str[0] >= 'A' && str[0] <= 'Z'))
		if(str[1] == ':' && (str[2] == '/' || str[2] == '\\'))
			return Element{str, 3};

	return Element{nullptr, 0};
}

Ex<FilePath> FilePath::current() {
	char buf[MAX_PATH];
	if(!GetCurrentDirectory(sizeof(buf), buf))
		return ERROR("Error in GetCurrentDirectory");
	return FilePath(buf);
}

Ex<void> FilePath::setCurrent(const FilePath &path) {
	if(!SetCurrentDirectory(path.c_str()))
		return ERROR("Error in SetCurrentDirectory(%)", path);
	return {};
}

bool FilePath::isRegularFile() const {
	struct _stat buf;
	_stat(c_str(), &buf);
	return S_ISREG(buf.st_mode);
}

bool FilePath::isDirectory() const {
	struct _stat buf;
	_stat(c_str(), &buf);
	return S_ISDIR(buf.st_mode);
}

static void findFiles(vector<FileEntry> &out, const FilePath &path, const FilePath &append,
					  int flags) {
	WIN32_FIND_DATA data;
	char tpath[MAX_PATH];
	snprintf(tpath, sizeof(tpath), "%s/*", path.c_str());
	HANDLE handle = FindFirstFile(tpath, &data);

	bool is_root = path.isRoot();
	bool ignore_parent = !(flags & FindFiles::include_parent) || is_root;

	if(handle != INVALID_HANDLE_VALUE)
		do {
			bool is_current = strcmp(data.cFileName, ".") == 0;
			bool is_parent = strcmp(data.cFileName, "..") == 0;

			if(is_current || (ignore_parent && is_parent))
				continue;

			bool is_dir = data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;
			bool is_file = !is_dir;

			bool do_accept = ((flags & FindFiles::regular_file) && is_file) ||
							 ((flags & FindFiles::directory) && is_dir);

			if(do_accept) {
				FileEntry entry;
				entry.path = append / FilePath(data.cFileName);
				entry.is_dir = is_dir;
				out.push_back(entry);
			}

			if(is_dir && (flags & FindFiles::recursive) && !is_parent)
				findFiles(out, path / FilePath(data.cFileName), append / FilePath(data.cFileName),
						  flags);
		} while(FindNextFile(handle, &data));
}

vector<FileEntry> findFiles(const FilePath &path, int flags) {
	vector<FileEntry> out;

	auto abs_path = path.absolute();
	if(!abs_path)
		return {};

	FilePath append =
		flags & FindFiles::relative ? "." : flags & FindFiles::absolute ? *abs_path : path;

	findFiles(out, *abs_path, append, flags);
	return out;
}
}

#endif