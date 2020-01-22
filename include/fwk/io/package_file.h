// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/io/file_system.h"
#include "fwk/pod_vector.h"
#include "fwk/vector.h"

namespace fwk {

// TODO: save times as well ?
// Simple package which groups a bunch of files.
// Designed for small (<1MB) files, so that it can be loaded into memory at once.
struct PackageFile {
	static constexpr int max_files = 64 * 1024;
	static constexpr int max_file_size = 16 * 1024 * 1024;

	struct FileInfo {
		string name;
		u32 size, offset;
	};

	PackageFile(vector<FileInfo>, PodVector<char>, u32 data_offset = 0);
	static Ex<PackageFile> make(FilePath prefix, CSpan<string> file_names);
	static Ex<PackageFile> load(Stream &);

	Ex<void> save(Stream &) const;

	int size() const { return m_infos.size(); }
	CSpan<FileInfo> fileInfos() const { return m_infos; }
	const FileInfo &operator[](int idx) const { return m_infos[idx]; }

	CSpan<char> data(int file_id) const;
	CSpan<char> data() const;

	bool emptyData() const { return !m_data; }

  private:
	vector<FileInfo> m_infos;
	PodVector<char> m_data;
	u32 m_data_offset = 0;
};
}
