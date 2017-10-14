// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys_base.h"

namespace fwk {

// Simple reference to string
// User have to make sure that referenced data is alive as long as CString
class CString {
  public:
	CString(const string &str) : m_data(str.c_str()), m_length((int)str.size()) {}
	CString(const char *str, int length) : m_data(str ? str : ""), m_length(length) {
		PASSERT((int)strlen(str) >= length);
	}
	CString(const char *str) {
		if(!str)
			str = "";
		m_data = str;
		m_length = strlen(str);
	}
	// TODO: conversion from CSpan<char>? but what about null-termination
	CString() : m_data(""), m_length(0) {}

	operator string() const { return string(m_data, m_data + m_length); }
	const char *c_str() const { return m_data; }

	int size() const { return m_length; }
	int length() const { return m_length; }
	bool empty() const { return m_length == 0; }
	int compare(const CString &rhs) const;
	int caseCompare(const CString &rhs) const;

	const char *begin() const { return m_data; }
	const char *end() const { return m_data + m_length; }

	bool operator==(const CString &rhs) const {
		return m_length == rhs.m_length && compare(rhs) == 0;
	}
	bool operator<(const CString &rhs) const { return compare(rhs) < 0; }

	CString operator+(int offset) const {
		DASSERT(offset >= 0 && offset <= m_length);
		return CString(m_data + offset, m_length - offset);
	}

  private:
	const char *m_data;
	int m_length;
};

struct Tokenizer {
	explicit Tokenizer(const char *str, char delim = ' ') : m_str(str), m_delim(delim) {}

	CString next();
	bool finished() const { return *m_str == 0; }

  private:
	const char *m_str;
	char m_delim;
};

inline bool caseEqual(const CString a, const CString b) {
	return a.size() == b.size() && a.caseCompare(b) == 0;
}
inline bool caseNEqual(const CString a, const CString b) { return !caseEqual(a, b); }
inline bool caseLess(const CString a, const CString b) { return a.caseCompare(b) < 0; }

// TODO: CString for string32 ?
Maybe<string32> toUTF32(CString);
Maybe<string> toUTF8(const string32 &);

// Returns size of buffer big enough for conversion
// 0 may be returned is string is invalid
int utf8Length(const string32 &);
int utf32Length(const string &);
string toLower(const string &str);
}
