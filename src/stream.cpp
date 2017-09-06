// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_xml.h"

#include <cstring>
#include <iostream>
#include <limits>

#ifdef FWK_TARGET_HTML5
#include "emscripten.h"
#endif

namespace fwk {

using namespace std;

Stream::Stream(bool is_loading)
	: m_size(0), m_pos(0), m_exception_thrown(false), m_is_loading(is_loading) {}

void Stream::handleException(const Exception &ex) {
	if(m_exception_thrown)
		throw ex;
	m_exception_thrown = 1;

	TextFormatter out;
	out("While %s stream \"%s\" at position %lld/%lld:\n%s",
		(m_is_loading ? "loading from" : "saving to"), name(), m_pos, m_size, ex.text());

#ifdef FWK_TARGET_HTML5
	printf("%s\n", out.text());
	emscripten_log(EM_LOG_ERROR | EM_LOG_C_STACK, "%s\n", out.text());
	emscripten_force_exit(1);
#else
	throw Exception(out.text(), ex.backtraceData());
#endif
}

void Stream::loadData(void *ptr, int bytes) {
	if(!bytes)
		return;

	try {
		DASSERT(isLoading() && ptr);
		DASSERT(bytes >= 0 && m_pos + bytes <= m_size);
		v_load(ptr, bytes);
	} catch(const Exception &ex) {
		handleException(ex);
	}
}

void Stream::saveData(const void *ptr, int bytes) {
	if(!bytes)
		return;

	try {
		DASSERT(isSaving() && ptr && bytes >= 0);
		v_save(ptr, bytes);
	} catch(const Exception &ex) {
		handleException(ex);
	}
}

void Stream::seek(long long pos) {
	try {
		DASSERT(pos >= 0 && pos <= m_size);
		v_seek(pos);
	} catch(const Exception &ex) {
		handleException(ex);
	}
}

void Stream::signature(u32 sig) {
	if(m_is_loading) {
		u32 tmp;
		loadData(&tmp, sizeof(tmp));

		if(tmp != sig) {
			char sigc[12] = {
				char((sig >> 0) & 0xff), 0, 0, char((sig >> 8) & 0xff), 0, 0,
				char((sig >> 16) & 0xff), 0, 0, char((sig >> 24) & 0xff), 0, 0,
			};

			for(int k = 0; k < 4; k++) {
				if(sigc[k * 3] == 0) {
					sigc[k * 3 + 0] = '\\';
					sigc[k * 3 + 1] = '0';
				}
			}

			char text[128];
			snprintf(text, sizeof(text), "Expected signature: 0x%08x (\"%s%s%s%s\")", sig, sigc + 0,
					 sigc + 3, sigc + 6, sigc + 9);
			handleException(Exception(text));
		}
	} else
		saveData(&sig, sizeof(sig));
}

static int decodeString(const char *str, int strSize, char *buf, int bufSize) {
	int len = 0;
	bufSize--;

	for(int n = 0; n < strSize && len < bufSize; n++) {
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

void Stream::signature(const char *str, int len) {
	char buf[33];
	DASSERT(str && len > 0 && len < (int)sizeof(buf) - 1);

	if(m_is_loading) {
		loadData(buf, len);

		if(memcmp(buf, str, len) != 0) {
			char rsig[256], dsig[256];
			decodeString(str, len, rsig, sizeof(rsig));
			decodeString(buf, len, dsig, sizeof(dsig));
			char buffer[512]; // TODO: buffer sizes
			snprintf(buffer, sizeof(buffer), "Expected signature: \"%s\" got: \"%s\"", rsig, dsig);
			handleException(Exception(buffer));
		}
	} else
		saveData(str, len);
}

int Stream::loadString(char *buffer, int size) {
	DASSERT(buffer && size >= 0);

	i32 length;
	u8 tmp_len;
	*this >> tmp_len;
	if(tmp_len < 255)
		length = tmp_len;
	else
		*this >> length;

	if(length >= size)
		handleException(Exception("Buffer size is too small"));
	loadData(buffer, length);
	buffer[length] = 0;
	return length;
}

void Stream::saveString(const char *ptr, int length) {
	DASSERT(ptr && length >= 0);
	if(length == 0)
		length = strlen(ptr);

	if(length < 255)
		*this << u8(length);
	else
		pack((u8)255, length);
	saveData(ptr, length);
}

void loadFromStream(string &v, Stream &sr) {
	u32 len;
	u8 tmp;

	sr >> tmp;
	if(tmp < 255)
		len = tmp;
	else
		sr >> len;

	try {
		if(len > sr.size() - sr.pos())
			sr.handleException(Exception("Invalid stream data"));
		v.resize(len, 0);
	} catch(const Exception &ex) {
		sr.handleException(ex);
	}

	sr.loadData(&v[0], len);
}

void saveToStream(const string &v, Stream &sr) { sr.saveString(v.c_str(), v.size()); }

FileStream::FileStream(const char *file_name, bool is_loading)
	: Stream(is_loading), m_name(file_name) {
	if(!(m_file = fopen(file_name, is_loading ? "rb" : "wb")))
		THROW("Error while opening file \"%s\"", file_name);

	fseek((FILE *)m_file, 0, SEEK_END);
	auto pos = ftell((FILE *)m_file);

	if(pos < -1) {
		fclose((FILE *)m_file);
		THROW("Trying to open a directory: \"%s\"", file_name);
	}
	m_size = pos;
	fseek((FILE *)m_file, 0, SEEK_SET);
}

FileStream::~FileStream() throw() {
	if(m_file)
		fclose((FILE *)m_file);
}

void FileStream::v_load(void *data, int bytes) {
	if(fread(data, bytes, 1, (FILE *)m_file) != 1)
		THROW("fread failed with errno: %s\n", strerror(errno));
	m_pos += bytes;
}

void FileStream::v_save(const void *data, int bytes) {
	if(fwrite(data, bytes, 1, (FILE *)m_file) != 1)
		THROW("fwrite failed with errno: %s\n", strerror(errno));
	m_pos += bytes;
	if(m_pos > m_size)
		m_size = m_pos;
}

void FileStream::v_seek(long long pos) {
	if(fseek((FILE *)m_file, pos, SEEK_SET) != 0)
		THROW("fseek failed with errno: %s\n", strerror(errno));
	m_pos = pos;
}

const char *FileStream::name() const throw() { return m_name.c_str(); }

MemoryLoader::MemoryLoader(const char *data, long long size) : Stream(true), m_data(data) {
	ASSERT(size >= 0);
	m_pos = 0;
	m_size = size;
}

void MemoryLoader::v_load(void *ptr, int count) {
	memcpy(ptr, m_data + m_pos, count);
	m_pos += count;
}

MemorySaver::MemorySaver(char *data, long long size) : Stream(false), m_data(data) {
	ASSERT(size >= 0);
	m_pos = 0;
	m_size = size;
}

void MemorySaver::v_save(const void *ptr, int count) {
	if(count + m_pos > m_size)
		THROW("Overflowing buffer (MemorySaver buffer has constant size)");
	memcpy(m_data + m_pos, ptr, count);
	m_pos += count;
}
}
