// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#if defined(FWK_PLATFORM_MINGW)
#error "This file should only be compiled on linux or HTML targets"
#endif

#include "fwk/io/file_system.h"

#include "fwk/sys/expected.h"
#include "fwk/vector.h"
#include <cstring>
#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

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

Ex<FilePath> FilePath::current() {
	char buffer[512];
	char *name = getcwd(buffer, sizeof(buffer) - 1);
	if(!name)
		return ERROR("Error in getcwd: %", strerror(errno));
	return FilePath(name);
}

Ex<void> FilePath::setCurrent(const FilePath &path) {
	if(chdir(path.c_str()) != 0)
		return ERROR("Error in chdir: %", strerror(errno));
	return {};
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

using Opt = FindFileOpt;

static void findFiles(vector<FileEntry> &out, const FilePath &path, const FilePath &append,
					  FindFileOpts opts) {
#ifdef FWK_PLATFORM_HTML
	// TODO: opendir fails on these...
	if(Str(path).find("/proc/self/fd") == 0)
		return;
#endif

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
}
