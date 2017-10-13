// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk_base.h"

namespace fwk {

class FilePath {
  public:
	FilePath(const char *);
	FilePath(const string &);
	FilePath(FilePath &&);
	FilePath(const FilePath &);
	FilePath();

	void operator=(const FilePath &rhs) { m_path = rhs.m_path; }
	void operator=(FilePath &&rhs) { m_path = move(rhs.m_path); }

	bool isRoot() const;
	bool isAbsolute() const;
	bool empty() const { return m_path.empty(); }

	string fileName() const;
	string fileExtension() const;
	bool isDirectory() const;
	bool isRegularFile() const;

	FilePath relative(const FilePath &relative_to = current()) const;
	bool isRelative(const FilePath &ancestor) const;

	FilePath absolute() const;
	FilePath parent() const;

	FilePath operator/(const FilePath &other) const;
	FilePath &operator/=(const FilePath &other);

	static FilePath current();
	static void setCurrent(const FilePath &);

	operator const string &() const { return m_path; }
	const char *c_str() const { return m_path.c_str(); }
	int size() const { return (int)m_path.size(); }

	FWK_ORDER_BY(FilePath, m_path);

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
	static void divide(const char *, vector<Element> &);
	static void simplify(const vector<Element> &src, vector<Element> &dst);
	void construct(const vector<Element> &);

	string m_path; // its always non-empty
};

struct FileEntry {
	FilePath path;
	bool is_dir;

	bool operator<(const FileEntry &rhs) const {
		return is_dir == rhs.is_dir ? path < rhs.path : is_dir > rhs.is_dir;
	}
};

namespace FindFiles {
	enum Flags {
		regular_file = 1,
		directory = 2,

		recursive = 4,

		relative = 8,		 // all paths relative to given path
		absolute = 16,		 // all paths absolute
		include_parent = 32, // include '..'
	};
};

vector<string> findFiles(const string &prefix, const string &suffix);
vector<FileEntry> findFiles(const FilePath &path, int flags = FindFiles::regular_file);
bool removeSuffix(string &str, const string &suffix);
bool removePrefix(string &str, const string &prefix);
void mkdirRecursive(const FilePath &path);
bool access(const FilePath &);
double lastModificationTime(const FilePath &);
FilePath executablePath();
}