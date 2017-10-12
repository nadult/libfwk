// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef _WIN32

#include "fwk_base.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>
#include <cstring>

namespace fwk {

FilePath executablePath() {
	char name[512];
	int ret = readlink("/proc/self/exe", name, sizeof(name) - 1);
	if(ret == -1)
		return "";
	name[ret] = 0;
	return name;
}

FilePath::Element FilePath::extractRoot(const char *str) {
	if(str[0] == '/')
		return Element{str, 1};
	return Element{nullptr, 0};
}

FilePath FilePath::current() {
	char buffer[512];
	char *name = getcwd(buffer, sizeof(buffer) - 1);
	if(!name)
		CHECK_FAILED("Error in getcwd: %s", strerror(errno));
	return FilePath(name);
}

void FilePath::setCurrent(const FilePath &path) {
	if(chdir(path.c_str()) != 0)
		CHECK_FAILED("Error in chdir: %s", strerror(errno));
}

bool FilePath::isRegularFile() const {
	struct stat buf;
	if(lstat(c_str(), &buf) != 0)
		return false;

	return S_ISREG(buf.st_mode);
}

bool FilePath::isDirectory() const {
	struct stat buf;
	if(lstat(c_str(), &buf) != 0)
		return false;

	return S_ISDIR(buf.st_mode) || S_ISLNK(buf.st_mode);
}

static void findFiles(vector<FileEntry> &out, const FilePath &path, const FilePath &append,
					  int flags) {
	DIR *dp = opendir(path.c_str());
	if(!dp)
		return;
	bool is_root = path.isRoot();
	bool ignore_parent = !(flags & FindFiles::include_parent) || is_root;

	{
		struct dirent *dirp;

		while((dirp = readdir(dp))) {
			bool is_parent = strcmp(dirp->d_name, "..") == 0;
			bool is_current = strcmp(dirp->d_name, ".") == 0;

			if(is_current || (ignore_parent && is_parent))
				continue;

			bool is_directory = false, is_regular = false;
#ifdef _DIRENT_HAVE_D_TYPE
			is_directory = dirp->d_type == DT_DIR;
			is_regular = dirp->d_type == DT_REG;
			// TODO: fix this
			//		if(dirp->d_type == DT_LNK)
			//			dirp->d_type = DT_DIR;

			if(dirp->d_type == DT_UNKNOWN)
#endif
			{
				char full_path[FILENAME_MAX];
				struct stat buf;

				snprintf(full_path, sizeof(full_path), "%s/%s", path.c_str(), dirp->d_name);
				lstat(full_path, &buf);
				is_directory = S_ISDIR(buf.st_mode);
				is_regular = S_ISREG(buf.st_mode);
			}

			bool do_accept = ((flags & FindFiles::regular_file) && is_regular) ||
							 ((flags & FindFiles::directory) && is_directory);
			//		printf("found in %s: %s (%d/%d)\n", path.c_str(), dirp->d_name, is_directory,
			// is_regular);

			// TODO: check why was this added
			//	if(do_accept && is_root && is_directory)
			//		do_accept = false;

			if(do_accept) {
				FileEntry entry;
				entry.path = append / FilePath(dirp->d_name);
				entry.is_dir = is_directory;
				out.push_back(entry);
			}

			if(is_directory && (flags & FindFiles::recursive) && !is_parent)
				findFiles(out, path / FilePath(dirp->d_name), append / FilePath(dirp->d_name),
						  flags);
		}
	}

	// TODO: finally: closedir ?
	closedir(dp);
}

vector<FileEntry> findFiles(const FilePath &path, int flags) {
	vector<FileEntry> out;

	bool is_relative = flags & FindFiles::relative;
	bool is_absolute = flags & FindFiles::absolute;
	FilePath append = is_relative ? "." : is_absolute ? path.absolute() : path;

	findFiles(out, path.absolute(), append, flags);
	return out;
}
}

#endif
