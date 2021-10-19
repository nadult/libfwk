// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/io/stream.h"

#include "fwk/sys/assert.h"
#include "fwk/sys/exception.h"
#include "fwk/sys/expected.h"

namespace fwk {

// TODO: istream / ostream distinction ? but it would be impossible? to use in serialize(Stream&) func
// TODO: more clear distinction about functions which set errors in stream and those which return Ex<>

// TODO: we can save a small header with info about sizes of
// different types (int / long / long long / etc. so that we can early detect discrepancies)
//
// TODO: we can create a small structure for serializing small data elements to memory
// (for caching) and later we can feed it to stream; This way we can minimize virtual func calls / etc.
//
// TODO: we need a control structure which keeps info about endianness and sizes of basic types;
//       it identifies a concrete platform

BaseStream::BaseStream(BaseStream &&rhs)
	: m_pos(rhs.m_pos), m_size(rhs.m_size), m_flags(rhs.m_flags) {
	rhs.m_size = m_pos = 0;
	rhs.m_flags &= ~Flag::invalid;
}

void BaseStream::operator=(BaseStream &&rhs) {
	m_error = move(rhs.m_error);
	m_pos = rhs.m_pos;
	m_size = rhs.m_size;
	m_flags = rhs.m_flags;
	rhs.m_pos = rhs.m_size = 0;
	rhs.m_flags &= ~Flag::invalid;
}

BaseStream::~BaseStream() {
	if(m_error) {
		fwk::print("Unhandled Stream error:\n");
		m_error->print();
	}
}

void BaseStream::saveData(CSpan<char>) { FATAL("Un-implemented saveData called"); }
void BaseStream::loadData(Span<char>) { FATAL("Un-implemented loadData called"); }
void BaseStream::seek(i64 pos) {
	DASSERT_GE(pos, 0);
	DASSERT_LE(pos, m_size);
	m_pos = pos;
}

void BaseStream::reportError(Str text) {
	string message = errorMessage(text);
	if(m_error) {
		m_error->chunks.emplace_back(move(message));
	} else {
		m_error.emplace(move(message));
		if(t_backtrace_enabled)
			m_error->backtrace = Backtrace::get(1, nullptr, true);
	}
	m_flags |= Flag::invalid;
}

Ex<> BaseStream::getValid() {
	Ex<> out;
	if(m_error) {
		out = move(*m_error);
		m_error.reset();
	}
	return out;
}

Ex<> BaseStream::loadSignature(u32 sig) {
	u32 tmp;
	loadData(asPod(tmp));
	if(!isValid())
		return getValid();

	if(tmp != sig) {
		char temp[5];
		memcpy(temp, &sig, sizeof(sig));
		temp[4] = 0;

		auto text =
			stdFormat("Expected signature: 0x%08x (\"%s\")", sig, escapeString(temp).c_str());
		return Error(move(text));
	}

	return {};
}

Ex<> BaseStream::loadSignature(CSpan<char> sig) {
	PASSERT(sig.size() <= max_signature_size);

	char buf[max_signature_size];
	loadData({buf, min(max_signature_size, sig.size())});

	if(!isValid())
		return getValid();
	if(memcmp(buf, sig.data(), sig.size()) != 0) {
		auto text = format("Expected signature: \"%\" got: \"%\"", escapeString(sig),
						   escapeString({buf, sig.size()}));
		return Error(move(text));
	}

	return {};
}

void BaseStream::saveSignature(u32 sig) { saveData(asPod(sig)); }
void BaseStream::saveSignature(CSpan<char> sig) {
	PASSERT(sig.size() <= max_signature_size);
	saveData(sig);
}

Ex<> BaseStream::loadSignature(const char *str) { return loadSignature(cspan(Str(str))); }
void BaseStream::saveSignature(const char *str) { saveSignature(cspan(Str(str))); }

void BaseStream::saveSize(i64 size) {
	DASSERT(size >= 0);
	char bytes[9];
	int num_bytes;

	if(size < 248) {
		num_bytes = 1;
		bytes[0] = (char)u8(size);
	} else {
		u64 usize(size);
		int max_byte = 0;
		for(int j = 7; j >= 0; j--)
			if(usize & (u64(0xff) << (j * 8))) {
				max_byte = j;
				break;
			}
		bytes[0] = (char)u8(248 + max_byte);
		memcpy(bytes + 1, &usize, sizeof(usize));
		num_bytes = max_byte + 2;
	}
	saveData(cspan(bytes, num_bytes));
}

i64 BaseStream::loadSize() {
	if(!isValid())
		return 0;

	char bytes[9];
	loadData(span(bytes, 1));
	u8 small = (u8)bytes[0];

	if(small < 248)
		return i64(small);
	int num_bytes = small - 247;
	fill(span(bytes + 1, 8), 0);
	loadData(span(bytes + 1, num_bytes));
	u64 out;
	memcpy(&out, bytes + 1, 8);
	// Too big values will simply be masked out
	return out > std::numeric_limits<i64>::max() ? 0 : i64(out);
}

void BaseStream::saveString(CSpan<char> str) {
	saveSize(str.size());
	saveData(str);
}

void BaseStream::saveVector(CSpan<char> vec, int element_size) {
	DASSERT(vec.size() % element_size == 0);
	saveSize(vec.size() / element_size);
	saveData(vec);
}

PodVector<char> BaseStream::loadVector(int element_size) {
	auto size = loadSize();
	return loadVector(size, element_size);
}

PodVector<char> BaseStream::loadVector(int vector_size, int element_size) {
	auto num_bytes = i64(vector_size) * element_size;
	if(num_bytes > INT_MAX)
		reportError(format("Too many bytes to load: % > %", num_bytes, INT_MAX));
	PodVector<char> out;
	if(addResources(num_bytes)) {
		out.resize(num_bytes);
		loadData(out);
	}
	return out;
}

string BaseStream::loadString() {
	auto size = loadSize();
	string out;
	if(addResources(size)) {
		out.resize(size, ' ');
		loadData(out);
		if(!isValid())
			out = {};
	}
	return out;
}

int BaseStream::loadString(Span<char> str) {
	PASSERT(str.size() >= 1);
	auto size = loadSize();
	int max_size = str.size() - 1;
	if(size > max_size) {
		reportError(format("String too big: % > %", size, max_size));
		str[0] = 0;
		return 0;
	}

	loadData(span(str.data(), size));
	if(!isValid())
		size = 0;
	str[size] = 0;
	return size;
}

bool BaseStream::addResources(i64 value) {
	if(m_flags & Flag::invalid)
		return false;

	m_resource_counter += value;
	if(m_resource_counter > m_resource_limit) {
		reportError(format("Stream resource limit reached: % + % > %", m_resource_counter - value,
						   value, m_resource_limit));
		return false;
	}
	return true;
}

void BaseStream::setResourceLimit(i64 limit) {
	DASSERT(limit >= 0);
	m_resource_limit = limit;
	addResources(0);
}

template class TStream<BaseStream>;
}
