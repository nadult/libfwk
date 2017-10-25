// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/cstring.h"

#include "fwk/pod_vector.h"
#include "fwk/maybe.h"

namespace fwk {

#if defined(FWK_TARGET_MINGW)

static int strcasecmp(const char *a, const char *b) { return _stricmp(a, b); }

static const char *strcasestr(const char *a, const char *b) {
	DASSERT(a && b);

	while(*a) {
		if(strcasecmp(a, b) == 0)
			return a;
		a++;
	}

	return nullptr;
}

#endif

int CString::compare(const CString &rhs) const { return strcmp(m_data, rhs.m_data); }
int CString::caseCompare(const CString &rhs) const { return strcasecmp(m_data, rhs.m_data); }

CString Tokenizer::next() {
	const char *start = m_str;
	while(*m_str && *m_str != m_delim)
		m_str++;
	const char *end = m_str++;

	return {start, (int)(end - start)};
}

// Source: libc++: https://github.com/llvm-mirror/libcxx/blob/master/src/locale.cpp

//                                     Valid UTF ranges
//     UTF-32               UTF-16                          UTF-8               # of code points
//                     first      second       first   second    third   fourth
// 000000 - 00007F  0000 - 007F               00 - 7F                                 127
// 000080 - 0007FF  0080 - 07FF               C2 - DF, 80 - BF                       1920
// 000800 - 000FFF  0800 - 0FFF               E0 - E0, A0 - BF, 80 - BF              2048
// 001000 - 00CFFF  1000 - CFFF               E1 - EC, 80 - BF, 80 - BF             49152
// 00D000 - 00D7FF  D000 - D7FF               ED - ED, 80 - 9F, 80 - BF              2048
// 00D800 - 00DFFF                invalid
// 00E000 - 00FFFF  E000 - FFFF               EE - EF, 80 - BF, 80 - BF              8192
// 010000 - 03FFFF  D800 - D8BF, DC00 - DFFF  F0 - F0, 90 - BF, 80 - BF, 80 - BF   196608
// 040000 - 0FFFFF  D8C0 - DBBF, DC00 - DFFF  F1 - F3, 80 - BF, 80 - BF, 80 - BF   786432
// 100000 - 10FFFF  DBC0 - DBFF, DC00 - DFFF  F4 - F4, 80 - 8F, 80 - BF, 80 - BF    65536

enum Result { ok, partial, error };
static const unsigned long max_code = 0x10ffff;

static int ucs4_to_utf8_length(CSpan<uint32_t> string) {
	int length = 0;

	for(auto wc : string) {
		if((wc & 0xFFFFF800) == 0x00D800 || wc > max_code)
			return 0;

		if(wc < 0x000080)
			length += 1;
		else if(wc < 0x000800)
			length += 2;
		else if(wc < 0x010000)
			length += 3;
		else // if (wc < 0x110000)
			length += 4;
	}

	return length;
}

static Result ucs4_to_utf8(CSpan<uint32_t> string, uint8_t *&to, uint8_t *to_end) {
	for(auto wc : string) {
		if((wc & 0xFFFFF800) == 0x00D800 || wc > max_code)
			return Result::error;

		if(wc < 0x000080) {
			if(to_end - to < 1)
				return Result::partial;
			*to++ = static_cast<uint8_t>(wc);
		} else if(wc < 0x000800) {
			if(to_end - to < 2)
				return Result::partial;
			*to++ = static_cast<uint8_t>(0xC0 | (wc >> 6));
			*to++ = static_cast<uint8_t>(0x80 | (wc & 0x03F));
		} else if(wc < 0x010000) {
			if(to_end - to < 3)
				return Result::partial;
			*to++ = static_cast<uint8_t>(0xE0 | (wc >> 12));
			*to++ = static_cast<uint8_t>(0x80 | ((wc & 0x0FC0) >> 6));
			*to++ = static_cast<uint8_t>(0x80 | (wc & 0x003F));
		} else // if (wc < 0x110000)
		{
			if(to_end - to < 4)
				return Result::partial;
			*to++ = static_cast<uint8_t>(0xF0 | (wc >> 18));
			*to++ = static_cast<uint8_t>(0x80 | ((wc & 0x03F000) >> 12));
			*to++ = static_cast<uint8_t>(0x80 | ((wc & 0x000FC0) >> 6));
			*to++ = static_cast<uint8_t>(0x80 | (wc & 0x00003F));
		}
	}

	return Result::ok;
}

static int utf8_to_ucs4_length(CSpan<uint8_t> string) {
	int length = 0;

	int pos = 0;
	while(pos < string.size()) {
		auto c1 = string[pos];

		if(c1 < 0x80)
			++pos;
		else if(c1 < 0xC2)
			return 0;
		else if(c1 < 0xE0)
			pos += 2;
		else if(c1 < 0xF0)
			pos += 3;
		else if(c1 < 0xF5)
			pos += 4;
		else
			return 0;

		length++;
	}

	return pos == string.size() ? length : 0;
}

static Result utf8_to_ucs4(const uint8_t *&frm, const uint8_t *frm_end, uint32_t *&to,
						   uint32_t *to_end) {
	for(; frm < frm_end && to < to_end; ++to) {
		uint8_t c1 = static_cast<uint8_t>(*frm);
		if(c1 < 0x80) {
			*to = static_cast<uint32_t>(c1);
			++frm;
		} else if(c1 < 0xC2) {
			return Result::error;
		} else if(c1 < 0xE0) {
			if(frm_end - frm < 2)
				return Result::partial;
			uint8_t c2 = frm[1];
			if((c2 & 0xC0) != 0x80)
				return Result::error;
			uint32_t t = static_cast<uint32_t>(((c1 & 0x1F) << 6) | (c2 & 0x3F));
			if(t > max_code)
				return Result::error;
			*to = t;
			frm += 2;
		} else if(c1 < 0xF0) {
			if(frm_end - frm < 3)
				return Result::partial;
			uint8_t c2 = frm[1];
			uint8_t c3 = frm[2];
			switch(c1) {
			case 0xE0:
				if((c2 & 0xE0) != 0xA0)
					return Result::error;
				break;
			case 0xED:
				if((c2 & 0xE0) != 0x80)
					return Result::error;
				break;
			default:
				if((c2 & 0xC0) != 0x80)
					return Result::error;
				break;
			}
			if((c3 & 0xC0) != 0x80)
				return Result::error;
			uint32_t t =
				static_cast<uint32_t>(((c1 & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F));
			if(t > max_code)
				return Result::error;
			*to = t;
			frm += 3;
		} else if(c1 < 0xF5) {
			if(frm_end - frm < 4)
				return Result::partial;
			uint8_t c2 = frm[1];
			uint8_t c3 = frm[2];
			uint8_t c4 = frm[3];
			switch(c1) {
			case 0xF0:
				if(!(0x90 <= c2 && c2 <= 0xBF))
					return Result::error;
				break;
			case 0xF4:
				if((c2 & 0xF0) != 0x80)
					return Result::error;
				break;
			default:
				if((c2 & 0xC0) != 0x80)
					return Result::error;
				break;
			}
			if((c3 & 0xC0) != 0x80 || (c4 & 0xC0) != 0x80)
				return Result::error;
			uint32_t t = static_cast<uint32_t>(((c1 & 0x07) << 18) | ((c2 & 0x3F) << 12) |
											   ((c3 & 0x3F) << 6) | (c4 & 0x3F));
			if(t > max_code)
				return Result::error;
			*to = t;
			frm += 4;
		} else {
			return Result::error;
		}
	}
	return frm < frm_end ? Result::partial : Result::ok;
}

int utf8Length(const string32 &str) {
	return ucs4_to_utf8_length({reinterpret_cast<const uint32_t *>(str.data()), (int)str.size()});
}

int utf32Length(const string &str) {
	return utf8_to_ucs4_length({reinterpret_cast<const uint8_t *>(str.data()), (int)str.size()});
}

Maybe<string32> toUTF32(CString text) {
	const uint8_t *start = reinterpret_cast<const uint8_t *>(text.c_str());
	const uint8_t *end = start + text.size();

	int len = utf8_to_ucs4_length(CSpan<uint8_t>(start, text.size()));
	PodVector<uint32_t> buffer(len);

	auto *ostart = buffer.begin();
	auto *oend = buffer.end();

	auto result = utf8_to_ucs4(start, end, ostart, oend);
	if(result != Result::ok)
		return none;
	return string32(buffer.begin(), buffer.end());
}

Maybe<string> toUTF8(const string32 &text) {
	const uint32_t *start = reinterpret_cast<const uint32_t *>(text.c_str());
	const uint32_t *end = start + text.size();

	int len = ucs4_to_utf8_length(CSpan<uint32_t>(start, text.size()));
	PodVector<uint8_t> buffer(len);

	auto *ostart = buffer.begin();
	auto *oend = buffer.end();

	auto result = ucs4_to_utf8({start, end}, ostart, oend);
	if(result != Result::ok)
		return none;
	return string(buffer.begin(), buffer.end());
}
}
