// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/io/file_stream.h"
#include "fwk/io/gzip_stream.h"
#include "fwk/io/memory_stream.h"
#include "fwk/io/package_file.h"
#include "fwk/sys/expected.h"

using namespace fwk;

//#define VERBOSE

Ex<void> packFiles(FilePath path, string output, string suffix) {
	EXPECT(path.isDirectory());
	string prefix = path;
	if(prefix.back() != '/')
		prefix += '/';

	auto files = findFiles(prefix, suffix);
	for(auto &file : files)
		file += suffix;
	auto pkg = EX_PASS(PackageFile::make(prefix, files));
	auto out_stream = EX_PASS(fileSaver(output));
#ifdef VERBOSE
	for(auto [file, size, _] : pkg.fileInfos())
		printf("Adding: %6dKB %s\n", (int)((size + 512) / 1024), file.c_str());
#endif
	return pkg.save(out_stream);
}

Ex<void> unpackFiles(FilePath path, FilePath output_prefix) {
	// TODO: handle compressed
	auto sr = EX_PASS(fileLoader(path));
	auto pkg = EX_PASS(PackageFile::load(sr));
	sr.clear();

	for(int n = 0; n < pkg.size(); n++) {
		auto path = output_prefix / pkg[n].name;
		EXPECT(mkdirRecursive(path.parent()));
		EXPECT(saveFile(path, pkg.data(n)));
	}

	return {};
}

int main(int argc, char **argv) {
	if(argc < 4) {
		printf("Usage:\n%s pack input_path output_file [suffix]\n"
			   "%s unpack input_file output_path_prefix\n\n",
			   argv[0], argv[0]);
		return 0;
	}

	string command = argv[1];
	string input = argv[2];
	string output = argv[3];
	string suffix = argc > 4? argv[4] : "";

	if(command == "pack")
		packFiles(input, output, suffix).check();
	else if(command == "unpack")
		unpackFiles(input, output).check();
	else
		printf("Invalid command: %s\n", command.c_str());

	return 0;
}
