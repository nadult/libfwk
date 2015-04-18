/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef _WIN32

#include "fwk_base.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>
#include <cstring>


namespace fwk {

FilePath::Element FilePath::extractRoot(const char *str) {
	if(str[0] == '/')
		return Element{str, 1};
	return Element{nullptr, 0};
}

FilePath FilePath::current() {
	char *pwd = getenv("PWD");
	ASSERT(pwd && pwd[0] == '/');
	return FilePath(pwd);
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

#ifdef _DIRENT_HAVE_D_TYPE
static void findFiles(vector<FileEntry> &out, const FilePath &path, const FilePath &append,
					  int flags) {
	DIR *dp = opendir(path.c_str());
	if(!dp)
		THROW("Error while opening directory %s: %s", path.c_str(), strerror(errno));
	bool is_root = path.isRoot();
	bool ignore_parent = !(flags & FindFiles::include_parent) || is_root;

	try {
		struct dirent *dirp;

		while((dirp = readdir(dp))) {
			bool is_parent = strcmp(dirp->d_name, "..") == 0;
			bool is_current = strcmp(dirp->d_name, ".") == 0;

			if(is_current || (ignore_parent && is_parent))
				continue;

			if(dirp->d_type == DT_UNKNOWN) {
				char full_path[FILENAME_MAX];
				struct stat buf;

				snprintf(full_path, sizeof(full_path), "%s/%s", path.c_str(), dirp->d_name);
				stat(full_path, &buf);
				if(S_ISDIR(buf.st_mode))
					dirp->d_type = DT_DIR;
				else if(S_ISREG(buf.st_mode))
					dirp->d_type = DT_REG;
			}

			// TODO: fix this
			//		if(dirp->d_type == DT_LNK)
			//			dirp->d_type = DT_DIR;

			bool do_accept = ((flags & FindFiles::regular_file) && dirp->d_type == DT_REG) ||
							 ((flags & FindFiles::directory) && (dirp->d_type == DT_DIR));
			//		printf("found in %s: %s (%d)\n", path.c_str(), dirp->d_name,
			//(int)dirp->d_type);

		//TODO: check why was this added
		//	if(do_accept && is_root && dirp->d_type == DT_DIR)
		//		do_accept = false;

			if(do_accept) {
				FileEntry entry;
				entry.path = append / FilePath(dirp->d_name);
				entry.is_dir = dirp->d_type == DT_DIR;
				out.push_back(entry);
			}

			if(dirp->d_type == DT_DIR && (flags & FindFiles::recursive) && !is_parent)
				findFiles(out, path / FilePath(dirp->d_name), append / FilePath(dirp->d_name),
						  flags);
		}
	} catch(...) {
		closedir(dp);
		throw;
	}
	closedir(dp);
}

void findFiles(vector<FileEntry> &out, const FilePath &path, int flags) {
	bool is_relative = flags & FindFiles::relative;
	bool is_absolute = flags & FindFiles::absolute;
	FilePath append = is_relative ? "." : is_absolute ? path.absolute() : path;

	findFiles(out, path.absolute(), append, flags);
}
#endif
}

#endif
