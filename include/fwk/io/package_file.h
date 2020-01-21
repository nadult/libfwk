// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/io/file_system.h"
#include "fwk/vector.h"

namespace fwk {

// TODO: save times as well ?
struct PackageFile {
	static constexpr int max_files = 64 * 1024;
	static constexpr int max_file_size = 1024 * 1024 * 1024;

	// Constructs PackageFile from given prefix path and list of files
	static Ex<PackageFile> make(FilePath prefix, CSpan<string>);
	static Ex<PackageFile> loadHeader(Stream &);

	void saveHeader(Stream &) const;
	// Writes header and file contents to given stream
	Ex<void> saveFull(Stream &) const;

	struct FileInfo {
		string name;
		u32 size;
	};

	FilePath prefix;
	vector<FileInfo> files;
};
}
