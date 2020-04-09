// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/io/stream.h"

#include "fwk/sys/assert.h"
#include "fwk/sys/exception.h"
#include "stream_inl.h"

namespace fwk {

using namespace stream_limits;

BaseStream::BaseStream(BaseStream &&rhs)
	: m_pos(rhs.m_pos), m_size(rhs.m_size), m_flags(rhs.m_flags) {
	rhs.clear();
}

FWK_MOVE_ASSIGN_RECONSTRUCT(BaseStream)
BaseStream::~BaseStream() = default;

void BaseStream::saveData(CSpan<char>) { FATAL("Unsupported saveData called"); }
void BaseStream::loadData(Span<char>) { FATAL("Unsupported loadData called"); }
void BaseStream::seek(i64 pos) {
	DASSERT(pos >= 0);
	DASSERT_LE(pos, m_size);
	m_pos = pos;
}

void BaseStream::clear() {
	m_size = m_pos = 0;
	m_flags &= ~Flag::invalid;
}

void BaseStream::signature(u32 sig) {
	if(!isValid())
		return;
	if(isSaving()) {
		saveData(asPod(sig));
		return;
	}

	u32 tmp;
	loadData(asPod(tmp));

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

		raise(stdFormat("Expected signature: 0x%08x (\"%s%s%s%s\")", sig, sigc + 0, sigc + 3,
						sigc + 6, sigc + 9));
	}
}

void BaseStream::signature(CSpan<char> sig) {
	PASSERT(sig.size() <= max_signature_size);

	if(!isValid())
		return;
	if(isSaving()) {
		saveData(sig);
		return;
	}

	char buf[max_signature_size];
	loadData({buf, sig.size()});

	if(memcmp(buf, sig.data(), sig.size()) != 0)
		raise(format("Expected signature: \"%\" got: \"%\"", escapeString(sig),
					 escapeString({buf, sig.size()})));
}

void BaseStream::raise(ZStr text) {
	RAISE("Stream % error at position %/%: %", isLoading() ? "loading" : "saving", m_pos, m_size,
		  text);
	m_flags |= Flag::invalid;
}

template class TStream<BaseStream>;
}
