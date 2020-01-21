// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/io/package_file.h"

#include "fwk/io/file_stream.h"
#include "fwk/io/stream.h"
#include "fwk/sys/expected.h"

namespace fwk {

// TODO: error handling when writing to/from streams

Ex<PackageFile> PackageFile::make(FilePath prefix, CSpan<string> in_files) {
	if(!prefix.isAbsolute())
		prefix = EX_PASS(prefix.absolute());

	vector<FileInfo> files;
	files.reserve(in_files.size());

	for(auto name : in_files) {
		auto loader = EX_PASS(fileLoader(prefix / name));
		auto size = loader.size();
		EXPECT(size <= max_file_size);
		files.emplace_back(name, u32(size));
	}
	return PackageFile{prefix, move(files)};
}

Ex<PackageFile> PackageFile::loadHeader(Stream &sr) {
	DASSERT(sr.isLoading());
	EXPECT(sr.isValid());

	sr.signature("PACKAGE");

	u32 num_files;
	sr >> num_files;
	EXPECT(num_files <= max_files);

	vector<FileInfo> out;
	out.reserve(num_files);

	for(int n = 0; n < int(num_files); n++) {
		string name;
		u32 size;
		sr >> name >> size;
		EXPECT(size <= max_file_size);
		out.emplace_back(move(name), size);
	}

	return PackageFile{"", move(out)};
}

void PackageFile::saveHeader(Stream &sr) const {
	sr.signature("PACKAGE");
	sr << u32(files.size());
	for(auto &[name, size] : files) {
		sr.saveString(name);
		sr << size;
	}
}

Ex<void> PackageFile::saveFull(Stream &sr) const {
	DASSERT(sr.isSaving());

	saveHeader(sr);
	for(int n = 0; n < files.size(); n++) {
		auto &[name, size] = files[n];
		auto data = EX_PASS(loadFile(prefix / name, max_file_size));

		if(size != (u32)data.size())
			return ERROR("File '%' size changed: %; expected: %", name, data.size(), size);
		sr.saveData(data);
	}

	return {};
}
}
