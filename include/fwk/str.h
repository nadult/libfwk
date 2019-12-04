// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys_base.h"

namespace fwk {

// Simple reference to string data (not owned)
// Str doesn't have to be null-terminated
// TODO: CString for string32 ?
class Str {
  public:
	Str(const string &str) : m_data(str.c_str()), m_size((int)str.size()) {}
	Str(const char *str, int size) : m_data(str ? str : ""), m_size(size) {}
	Str(const char *str) : m_data(str ? str : "") { m_size = strlen(m_data); }
	Str(const char *begin, const char *end) : m_data(begin), m_size(end - begin) {
		PASSERT(end >= begin);
	}
	Str() : m_data(""), m_size(0) {}

	explicit operator bool() const { return m_size > 0; }
	operator string() const { return string(m_data, m_data + m_size); }

	int size() const { return m_size; }
	bool empty() const { return m_size == 0; }
	int compare(const Str &rhs) const;
	int compareIgnoreCase(const Str &rhs) const;
	bool inRange(int pos) const { return pos >= 0 && pos < m_size; }

	const char *data() const { return m_data; }
	const char *begin() const { return m_data; }
	const char *end() const { return m_data + m_size; }

	bool operator==(const Str &rhs) const { return m_size == rhs.m_size && compare(rhs) == 0; }
	bool operator<(const Str &rhs) const { return compare(rhs) < 0; }

	Str advance(int offset) const {
		DASSERT(offset >= 0 && offset <= m_size);
		return Str(m_data + offset, m_size - offset);
	}
	char operator[](int pos) const {
		IF_PARANOID(checkIndex(pos));
		return m_data[pos];
	}

	// Returns pair: line & column
	Pair<int> utf8TextPos(const char *ptr) const;

	Str substr(int pos) const {
		PASSERT(pos >= 0 && pos <= m_size);
		return {m_data + pos, m_size - pos};
	}
	Str substr(int pos, int sub_length) const {
		PASSERT(pos >= 0 && sub_length >= 0 && pos + sub_length <= m_size);
		return {m_data + pos, sub_length};
	}

	string limitSizeBack(int max_size, Str suffix = "...") const;
	string limitSizeFront(int max_size, Str prefix = "...") const;

	// Returns -1 on failure
	int find(char c) const;
	int find(Str) const;

	bool contains(Str str) const { return find(str) != -1; }
	bool contains(char c) const { return find(c) != -1; }

	unsigned hash() const;

	friend bool operator==(const string &lhs, Str rhs) { return Str(lhs) == rhs; }
	friend bool operator<(const string &lhs, Str rhs) { return Str(lhs) < rhs; }

  protected:
	struct NoChecks {};
	Str(const char *data, int size, NoChecks) : m_data(data), m_size(size) {}
	void invalidIndex(int) const;
	void checkIndex(int idx) const {
		if(idx < 0 || idx >= m_size)
			invalidIndex(idx);
	}

	const char *m_data;
	int m_size;
};

// Zero terminated Str
// TODO: add terminating 0 checks ?
class ZStr : public Str {
  public:
	ZStr(const string &str) : Str(str) {}
	ZStr(const char *str, int size) : Str(str, size) {}
	ZStr(const char *str) : Str(str) {}
	ZStr() {}
	ZStr(const Str &) = delete;

	using Str::operator==;
	using Str::operator<;
	ZStr advance(int offset) const {
		DASSERT(offset >= 0 && offset <= m_size);
		return {m_data + offset, m_size - offset};
	}

	const char *c_str() const { return m_data; }
};

vector<string> tokenize(ZStr, char c = ' ');

struct Tokenizer {
	explicit Tokenizer(const char *str, char delim = ' ') : m_str(str), m_delim(delim) {
		DASSERT(delim);
	}

	Str next();
	bool finished() const { return *m_str == 0; }

  private:
	const char *m_str;
	char m_delim;
};

inline bool equalIgnoreCase(Str a, Str b) {
	return a.size() == b.size() && a.compareIgnoreCase(b) == 0;
}
inline bool lessIgnoreCase(Str a, Str b) { return a.compareIgnoreCase(b) < 0; }

Maybe<string32> toUTF32(Str);
Maybe<string> toUTF8(const string32 &);

Maybe<int> utf8CodePointLength(unsigned char);

// Returns size of buffer big enough for conversion
// 0 may be returned is string is invalid
int utf8Length(const string32 &);
int utf32Length(const string &);
string toLower(const string &);
string toUpper(const string &);
}
