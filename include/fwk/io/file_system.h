// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/light_tuple.h"
#include "fwk/span.h"
#include "fwk/str.h"
#include "fwk/sys_base.h"

namespace fwk {

class FilePath {
  public:
	FilePath(Str);
	FilePath(const char *);
	FilePath(const string &);
	FilePath(FilePath &&);
	FilePath(const FilePath &);
	FilePath();

	void operator=(const FilePath &rhs) { m_path = rhs.m_path; }
	void operator=(FilePath &&rhs) { m_path = std::move(rhs.m_path); }

	bool isRoot() const;
	bool isAbsolute() const;
	bool isRelative() const { return !isAbsolute(); }
	bool empty() const { return m_path.empty(); }

	Str fileName() const &;
	Maybe<Str> fileExtension() const &;
	Str fileStem() const &;

	bool isDirectory() const;
	bool isRegularFile() const;

	// Path should be absolute
	FilePath relative(const FilePath &relative_to) const;
	Ex<FilePath> relativeToCurrent() const;
	bool isRelative(const FilePath &ancestor) const;

	FilePath absolute(const FilePath &current) const;
	Ex<FilePath> absolute() const;
	FilePath parent() const;

	bool hasTildePrefix() const;
	FilePath replaceTildePrefix(const FilePath &home) const;

	FilePath operator/(const FilePath &other) const;
	FilePath &operator/=(const FilePath &other);

	static Ex<FilePath> current();
	static Ex<FilePath> home();
	static Ex<> setCurrent(const FilePath &);

	operator ZStr() const & { return m_path; }
	operator Str() const & { return m_path; }
	operator const string &() const & { return m_path; }
	const char *c_str() const & { return m_path.c_str(); }
	int size() const { return (int)m_path.size(); }

	FWK_ORDER_BY(FilePath, m_path);

	void operator>>(TextFormatter &) const;

  private:
	struct Element {
		bool isDot() const;
		bool isDots() const;
		bool isRoot() const;
		bool operator==(const Element &rhs) const;

		const char *ptr;
		int size;
	};

	static Element extractRoot(const char *);
	static void divide(Str, vector<Element> &);
	static void simplify(const vector<Element> &src, vector<Element> &dst);
	void construct(const vector<Element> &);

	string m_path; // its always non-empty
};

TextParser &operator>>(TextParser &, FilePath &) EXCEPT;

struct FileEntry {
	FilePath path;
	bool is_dir;
	bool is_link;

	// Directories and links are first
	bool operator<(const FileEntry &rhs) const;
};

// Dot is not included in the extension
// If name has no dot then returns none
Maybe<Str> fileNameExtension(Str);
Str fileNameStem(Str);

// relative: all paths relative to given path
// include_parent: include '..' as well
DEFINE_ENUM(FindFileOpt, regular_file, directory, link, recursive, relative, absolute,
			include_parent);
using FindFileOpts = EnumFlags<FindFileOpt>;

vector<string> findFiles(const string &prefix, const string &suffix);
vector<FileEntry> findFiles(const FilePath &path, FindFileOpts = FindFileOpt::regular_file);
bool access(const FilePath &);

Ex<> mkdirRecursive(const FilePath &);
Ex<double> lastModificationTime(const FilePath &);
Ex<> removeFile(const FilePath &);
Ex<> renameFile(const FilePath &src, const FilePath &dst);

FilePath executablePath();

// Returns pair: output + exit code
Ex<Pair<string, int>> execCommand(const string &cmd);

Ex<string> loadFileString(ZStr, int max_size = 64 * 1024 * 1024);
Ex<vector<char>> loadFile(ZStr, int max_size = 64 * 1024 * 1024);
Ex<> saveFile(ZStr file_name, CSpan<char>);
}
