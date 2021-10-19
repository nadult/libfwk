// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/io/package_file.h"

#include "fwk/io/file_stream.h"
#include "fwk/io/stream.h"
#include "fwk/sys/expected.h"

namespace fwk {

// TODO: error handling when writing to/from streams

PackageFile::PackageFile(vector<FileInfo> infos, PodVector<char> data, u32 off)
	: m_infos(move(infos)), m_data(move(data)), m_data_offset(off) {}

Ex<PackageFile> PackageFile::make(FilePath prefix, CSpan<string> in_files) {
	if(!prefix.isAbsolute())
		prefix = EX_PASS(prefix.absolute());

	vector<FileInfo> files;
	files.reserve(in_files.size());

	u32 offset = 0;
	PodVector<char> data;

	for(auto name : in_files) {
		auto loader = EX_PASS(fileLoader(prefix / name));
		auto size = loader.size();
		EXPECT(size <= max_file_size);
		data.resize(data.size() + size);
		loader.loadData(span(data.data() + offset, size));
		files.emplace_back(name, u32(size), offset);
		offset += size;
	}

	return PackageFile{move(files), move(data)};
}

Ex<PackageFile> PackageFile::load(Stream &sr) {
	DASSERT(sr.isLoading());
	EXPECT(sr.isValid());

	PodVector<char> data;
	EXPECT(sr.loadSignature("PACKAGE"));

	u32 num_files;
	sr >> num_files;
	EXPECT(num_files <= max_files);

	u32 offset = 0;
	vector<FileInfo> infos;
	infos.reserve(num_files);

	for(int n = 0; n < int(num_files); n++) {
		string name;
		u32 size;
		sr >> name >> size;
		EXPECT(size <= max_file_size);
		infos.emplace_back(move(name), size, offset);
		offset += size;
	}

	u32 data_offset = 0;
	data.resize(offset);
	sr.loadData(data);
	EXPECT(sr.getValid());

	return PackageFile{move(infos), move(data), data_offset};
}

Ex<> PackageFile::save(Stream &sr) const {
	DASSERT(sr.isSaving());

	sr.saveSignature("PACKAGE");
	sr << u32(size());
	for(auto &[name, size, offset] : m_infos) {
		sr.saveString(name);
		sr << size;
	}
	sr.saveData(data());
	EXPECT(sr.getValid());
	return {};
}

CSpan<char> PackageFile::data(int idx) const {
	auto &info = m_infos[idx];
	return cspan(m_data.begin() + m_data_offset + info.offset, info.size);
}

CSpan<char> PackageFile::data() const {
	return cspan(m_data.begin() + m_data_offset, m_data.size() - m_data_offset);
}
}
