// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/io/file_stream.h"
#include "fwk/io/gzip_stream.h"
#include "fwk/io/memory_stream.h"
#include "fwk/io/package_file.h"
#include "fwk/sys/backtrace.h"
#include "fwk/sys/expected.h"

using namespace fwk;

Ex<void> packFiles(FilePath path, string output) {
	EXPECT(path.isDirectory());
	string prefix = path;
	if(prefix.back() != '/')
		prefix += '/';
	DUMP(path, prefix);

	auto files = findFiles(prefix, "");
	auto pkg = EX_PASS(PackageFile::make(prefix, files));
	auto out_stream = EX_PASS(fileSaver(output));
	for(auto [file, size] : pkg.files)
		printf("Adding: %6dKB %s\n", (int)((size + 512) / 1024), file.c_str());
	return pkg.saveFull(out_stream);
}

Ex<void> compressFiles(FilePath path, string output) {
	// TODO: gzip compressor
	return ERROR("finish me");
}

Ex<void> unpackFiles(FilePath path, FilePath output_prefix) {
	// TODO: handle compressed
	auto sr = EX_PASS(fileLoader(path));
	auto pkg = EX_PASS(PackageFile::loadHeader(sr));

	for(auto [name, size] : pkg.files) {
		auto path = output_prefix / name;
		EXPECT(mkdirRecursive(path.parent()));
		PodVector<char> data(size);
		sr.loadData(data);
		EXPECT(saveFile(path, data));
	}

	return {};
}

int main(int argc, char **argv) {
	Backtrace::t_default_mode = BacktraceMode::full;

	if(argc < 4) {
		printf("Usage:\n%s pack input_path output_file\n"
			   "%s compress input_path output_file\n"
			   "%s unpack input_file output_path_prefix\n\n",
			   argv[0], argv[0], argv[0]);
		return 0;
	}

	string command = argv[1];
	string input = argv[2];
	string output = argv[3];

	if(command == "pack")
		packFiles(input, output).check();
	else if(command == "compress")
		compressFiles(input, output).check();
	else if(command == "unpack")
		unpackFiles(input, output).check();
	else
		printf("Invalid command: %s\n", command.c_str());

	return 0;
}
