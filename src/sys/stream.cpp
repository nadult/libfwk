// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/stream.h"

#include "fwk/sys/assert.h"
#include "fwk/sys/rollback.h"
#include <cstring>
#include <limits>

#ifdef FWK_TARGET_HTML5
#include "emscripten.h"
#endif

namespace fwk {

using namespace std;

Stream::Stream(bool is_loading)
	: m_size(0), m_pos(0), m_error_handled(false), m_is_loading(is_loading) {}

string Stream::handleError(void *pthis, void *) {
	Stream *stream = (Stream *)pthis;
	if(stream->m_error_handled)
		return {};
	stream->m_error_handled = 1;
	// TODO: test protection from hierarchical inclusions

	return format("While % stream \"%\" at position %/%",
				  (stream->m_is_loading ? "loading from" : "saving to"), stream->name(),
				  stream->m_pos, stream->m_size);
}

void Stream::loadData(void *ptr, int bytes) {
	if(!bytes)
		return;

	DASSERT(isLoading() && ptr);
	DASSERT_GE(bytes, 0);
	if(m_size != -1)
		CHECK(m_pos + bytes <= m_size);
	v_load(ptr, bytes);
}

void Stream::saveData(const void *ptr, int bytes) {
	if(!bytes)
		return;

	DASSERT(isSaving() && ptr && bytes >= 0);
	v_save(ptr, bytes);
}

void Stream::seek(long long pos) {
	DASSERT(m_size != -1 && pos >= 0 && pos <= m_size);
	v_seek(pos);
}

void Stream::signature(u32 sig) {
	if(m_is_loading) {
		u32 tmp;
		loadData(&tmp, sizeof(tmp));

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

			CHECK_FAILED("Expected signature: 0x%08x (\"%s%s%s%s\")", sig, sigc + 0, sigc + 3,
						 sigc + 6, sigc + 9);
		}
	} else {
		saveData(&sig, sizeof(sig));
	}
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
	enum { max_signature_size = 32 };
	char buf[max_signature_size + 1];
	DASSERT(str && len > 0 && len < max_signature_size);

	if(m_is_loading) {
		loadData(buf, len);

		if(memcmp(buf, str, len) != 0) {
			char rsig[256], dsig[256];
			decodeString(str, len, rsig, sizeof(rsig));
			decodeString(buf, len, dsig, sizeof(dsig));
			CHECK_FAILED("Expected signature: \"%s\" got: \"%s\"", rsig, dsig);
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

	CHECK(length < size);
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

void Stream::v_load(void *, int) { FATAL("v_load unimplemented"); }
void Stream::v_save(const void *, int) { FATAL("v_save unimplemented"); }
void Stream::v_seek(long long pos) { m_pos = pos; }

void loadFromStream(string &v, Stream &sr) {
	u32 len;
	u8 tmp;

	sr >> tmp;
	if(tmp < 255)
		len = tmp;
	else
		sr >> len;

	CHECK(len <= sr.size() - sr.pos());
	v.resize(len, 0);
	sr.loadData(&v[0], len);
}

void saveToStream(const string &v, Stream &sr) { sr.saveString(v.c_str(), v.size()); }

FileStream::FileStream(const char *file_name, bool is_loading)
	: Stream(is_loading), m_name(file_name) {
	if(!(m_file = fopen(file_name, is_loading ? "rb" : "wb")))
		CHECK_FAILED("Error while opening file \"%s\"", file_name);

	fseek((FILE *)m_file, 0, SEEK_END);
	auto pos = ftell((FILE *)m_file);

	if(pos < -1) {
		fclose((FILE *)m_file);
		CHECK_FAILED("Trying to open a directory: \"%s\"", file_name);
	}
	m_size = pos;
	fseek((FILE *)m_file, 0, SEEK_SET);
	m_rb_index = RollbackContext::atRollback(rbFree, m_file);
}

FileStream::~FileStream() {
	if(m_file) {
		RollbackContext::removeAtRollback(m_rb_index);
		fclose((FILE *)m_file);
	}
}

void FileStream::rbFree(void *ptr) { fclose((FILE *)ptr); }

void FileStream::v_load(void *data, int bytes) {
	if(fread(data, bytes, 1, (FILE *)m_file) != 1)
		CHECK_FAILED("fread failed with errno: %s\n", strerror(errno));
	m_pos += bytes;
}

void FileStream::v_save(const void *data, int bytes) {
	if(fwrite(data, bytes, 1, (FILE *)m_file) != 1)
		CHECK_FAILED("fwrite failed with errno: %s\n", strerror(errno));
	m_pos += bytes;
	if(m_pos > m_size)
		m_size = m_pos;
}

void FileStream::v_seek(long long pos) {
	if(fseek((FILE *)m_file, pos, SEEK_SET) != 0)
		CHECK_FAILED("fseek failed with errno: %s\n", strerror(errno));
	m_pos = pos;
}

const char *FileStream::name() const { return m_name.c_str(); }

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
		CHECK_FAILED("Overflowing buffer (MemorySaver buffer has constant size)");
	memcpy(m_data + m_pos, ptr, count);
	m_pos += count;
}
}
