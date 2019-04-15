// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/file_stream.h"

#include "fwk/sys/assert.h"
#include "fwk/sys/error.h"
#include "fwk/sys/rollback.h"
#include <cstring>
#include <limits>

#ifdef FWK_TARGET_HTML5
#include "emscripten.h"
#endif

namespace fwk {

using namespace std;

FileStream::FileStream() = default;

FileStream::FileStream(FileStream &&rhs)
	: m_name(move(rhs.m_name)), m_file(rhs.m_file), m_size(rhs.m_size), m_pos(rhs.m_pos),
	  m_is_loading(rhs.m_is_loading), m_is_valid(rhs.m_is_valid) {
	rhs.m_file = nullptr;
	rhs.m_is_valid = false;
}

void FileStream::operator=(FileStream &&rhs) {
	this->~FileStream();
	new(this) FileStream(move(rhs));
}

FileStream::~FileStream() {
	if(m_file)
		fclose((FILE *)m_file);
}

void FileStream::reportError(string message) {
	REG_ERROR("While % file '%' at position %/%: %", m_is_loading ? "loading from" : "saving to",
			  m_name, m_pos, m_size, message);
	m_is_valid = false;
}

void FileStream::loadData(Span<char> data) {
	PASSERT(m_is_loading);
	if(!m_is_valid || !data) {
		fill(data, 0);
		return;
	}

	if(m_pos + data.size() > m_size) {
		reportError(format("Reading past the end: % + % > %", m_name, m_pos, data.size(), m_size));
		fill(data, 0);
		return;
	}

	if(fread(data.data(), data.size(), 1, (FILE *)m_file) != 1) {
		reportError(format("fread failed: %", strerror(errno)));
		fill(data, 0);
		return;
	}

	m_pos += data.size();
}

void FileStream::saveData(CSpan<char> data) {
	PASSERT(!m_is_loading);
	if(!m_is_valid || !data)
		return;

	DASSERT(!m_is_loading);

	if(fwrite(data.data(), data.size(), 1, (FILE *)m_file) != 1) {
		reportError(format("fwrite failed: %", strerror(errno)));
		return;
	}

	m_pos += data.size();
	if(m_pos > m_size)
		m_size = m_pos;
}

void FileStream::seek(long long pos) {
	DASSERT(pos >= 0 && pos <= m_size);
	if(!m_is_valid)
		return;

	if(fseek((FILE *)m_file, pos, SEEK_SET) != 0) {
		reportError(format("fseek failed: %", strerror(errno)));
		return;
	}

	m_pos = pos;
}

i64 FileStream::loadSize() {
	if(!m_is_valid)
		return 0;

	i64 out = 0;
	{
		u8 small;
		*this >> small;
		if(small == 254) {
			u32 len32;
			*this >> len32;
			out = len32;
		} else if(small == 255) {
			*this >> out;
		} else {
			out = small;
		}
	}

	if(out < 0) {
		REG_ERROR("Invalid length: %", out);
		m_is_valid = false;
		return 0;
	}

	return out;
}

void FileStream::saveSize(i64 size) {
	PASSERT(size >= 0);
	if(size < 254)
		*this << u8(size);
	else if(size <= UINT_MAX)
		pack(u8(254), u32(size));
	else
		pack(u8(255), size);
}

string FileStream::loadString(i64 max_size) {
	auto size = loadSize();
	if(size > max_size) {
		reportError(format("String too big: % > %", size, max_size));
		return {};
	}

	string out(size, ' ');
	loadData(out);
	if(!m_is_valid)
		out = {};
	return out;
}

int FileStream::loadString(Span<char> str) {
	PASSERT(str.size() >= 1);
	auto size = loadSize();
	int max_size = str.size() - 1;
	if(size > max_size) {
		reportError(format("String too big: % > %", size, max_size));
		str[0] = 0;
		return 0;
	}

	loadData(span(str.data(), size));
	if(!m_is_valid)
		size = 0;
	str[size] = 0;
	return size;
}

void FileStream::saveString(CSpan<char> str) {
	saveSize(str.size());
	saveData(str);
}

PodVector<char> FileStream::loadVector(int max_size, int element_size) {
	PASSERT(max_size >= 0 && element_size >= 1);
	auto size = loadSize();

	if(size > max_size) {
		reportError(format("Vector too big: % > %", size, max_size));
		return {};
	}
	auto byte_size = size * element_size;
	ASSERT(byte_size < INT_MAX);

	PodVector<char> out(byte_size);
	loadData(out);
	if(!m_is_valid)
		out.clear();
	return out;
}

void FileStream::saveVector(CSpan<char> vec, int element_size) {
	DASSERT(vec.size() % element_size == 0);
	saveSize(vec.size() / element_size);
	saveData(vec);
}

static int decodeString(Str str, Span<char> buf) {
	int len = 0;
	int bufSize = buf.size() - 1;

	for(int n = 0; n < str.size() && len < bufSize; n++) {
		if(str[n] == '\\') {
			if(bufSize - len < 2)
				goto END;

			buf[len++] = '\\';
			buf[len++] = '\\';
		} else if(str[n] >= 32 && str[n] < 127) {
			buf[len++] = str[n];
		} else {
			if(bufSize - len < 4)
				goto END;

			buf[len++] = '\\';
			unsigned int code = (u8)str[n];
			buf[len++] = '0' + code / 64;
			buf[len++] = '0' + (code / 8) % 8;
			buf[len++] = '0' + code % 8;
		}
	}

END:
	buf[len++] = 0;
	return len;
}

void FileStream::signature(u32 sig) {
	if(!m_is_valid)
		return;

	if(m_is_loading) {
		u32 tmp;
		*this >> tmp;

		if(tmp != sig) {
			char sigc[12] = {
				char((sig >> 0) & 0xff),  0, 0, char((sig >> 8) & 0xff),  0, 0,
				char((sig >> 16) & 0xff), 0, 0, char((sig >> 24) & 0xff), 0, 0,
			};

			for(int k = 0; k < 4; k++) {
				if(sigc[k * 3] == 0) {
					sigc[k * 3 + 0] = '\\';
					sigc[k * 3 + 1] = '0';
				}
			}

			reportError(stdFormat("Expected signature: 0x%08x (\"%s%s%s%s\")", sig, sigc + 0,
								  sigc + 3, sigc + 6, sigc + 9));
		}
	} else {
		*this << sig;
	}
}

void FileStream::signature(Str str) {
	enum { max_signature_size = 32 };
	DASSERT(str.size() <= max_signature_size);

	if(!m_is_valid)
		return;

	if(m_is_loading) {
		char buf[max_signature_size + 1];
		loadData(span(buf, str.size()));

		if(memcmp(buf, str.data(), str.size()) != 0) {
			char rsig[256], dsig[256];
			decodeString(str, rsig);
			decodeString(Str(buf, str.size()), dsig);
			reportError(format("Expected signature: \"%\" got: \"%\"", rsig, dsig));
		}
	} else {
		saveData(str);
	}
}

Ex<FileStream> fileStream(ZStr file_name, bool is_loading) {
	auto *file = fopen(file_name.c_str(), is_loading ? "rb" : "wb");
	if(!file)
		return ERROR("Error while opening file \"%\"", file_name);

	fseek(file, 0, SEEK_END);
	auto size = ftell(file);

	// TODO: that's not a proper way to detect a directory
	if(size < -1) {
		fclose(file);
		return ERROR("Trying to open a directory: \"%\"", file_name);
	}
	fseek(file, 0, SEEK_SET);

	FileStream out;
	out.m_name = file_name;
	out.m_file = file;
	out.m_size = size;
	out.m_pos = 0;
	out.m_is_loading = is_loading;
	return move(out);
}

Ex<FileStream> fileLoader(ZStr file_name) { return fileStream(file_name, true); }
Ex<FileStream> fileSaver(ZStr file_name) { return fileStream(file_name, false); }
}
