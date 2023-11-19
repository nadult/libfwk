// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/array.h"
#include "fwk/dynamic.h"
#include "fwk/enum_flags.h"
#include "fwk/pod_vector.h"
#include "fwk/span.h"
#include "fwk/str.h"
#include "fwk/sys_base.h"

namespace fwk {

DEFINE_ENUM(StreamFlag, loading, invalid);
using StreamFlags = EnumFlags<StreamFlag>;

namespace detail {
	template <class S, class T> struct StreamTraits {
		FWK_SFINAE_TEST(saveable, T, DECLVAL(U).save(DECLVAL(S &)));
		FWK_SFINAE_TEST(loadable, T, DECLVAL(U).load(DECLVAL(S &)));
	};
}

template <class T> auto &asPod(const T &value) { return *(const Array<char, sizeof(T)> *)(&value); }
template <class T> auto &asPod(T &value) { return *(Array<char, sizeof(T)> *)(&value); }

// TODO: expand this
template <class TStream, class T>
inline constexpr bool stream_saveable = detail::StreamTraits<TStream, T>::saveable;
template <class TStream, class T>
inline constexpr bool stream_loadable = detail::StreamTraits<TStream, T>::loadable;

// This class provides convenient interface for serialization of data
template <class Base> class TStream : public Base {
  public:
	using Base::Base;
	using Base::isValid;
	using Base::loadData;
	using Base::saveData;
	using Base::seek;

	// TODO: rename to save/load Span
	template <class TSpan, class T = SpanBase<TSpan>, EnableIf<is_flat_data<T>>...>
	void saveData(const TSpan &);
	template <class TSpan, class T = SpanBase<TSpan>, EnableIf<!is_const<T> && is_flat_data<T>>...>
	void loadData(TSpan &);

	// TODO: support for serializing vector< >, maybe < > of saveable ?
	template <class T, EnableIf<is_flat_data<T>>...> TStream &operator<<(const T &);
	template <class T, EnableIf<is_flat_data<T>>...> TStream &operator>>(T &);

	template <class T, EnableIf<is_flat_data<T>>...> TStream &operator>>(Maybe<T> &);
	template <class T, EnableIf<is_flat_data<T>>...> TStream &operator<<(const Maybe<T> &);

	template <class T, EnableIf<is_flat_data<T>>...> TStream &operator>>(vector<T> &);
	template <class T, EnableIf<is_flat_data<T>>...> TStream &operator<<(const vector<T> &);

	template <class... Args, EnableIf<(is_flat_data<Args> && ...)>...> void unpack(Args &...);
	template <class... Args, EnableIf<(is_flat_data<Args> && ...)>...> void pack(const Args &...);

	template <class T, EnableIf<is_flat_data<T>>...> void saveVector(CSpan<T>);

	template <class T, EnableIf<is_flat_data<T>>...> PodVector<T> loadVector();
	template <class T, EnableIf<is_flat_data<T>>...> PodVector<T> loadVector(int vector_size);

	TStream &operator<<(Str);
	TStream &operator>>(string &);
};

// Generic stream class, with simple interface and error handling. When any kind of error
// happens while performing stream operation it saves it and turns into invalid state.
// In this state it doesn't write anything to stream (in saving mode) and when it reads data,
// it always fills it with zeros (you can use it to simplify error handling in serialization
// functions).
//
// Notes:
// - Stream errors have to be handled by calling getValid(). Unhandled errors will be
//   printed to the console.
// - You can use other stream classes directly (to avoid virtual function call) if
//   you don't care about genericity.
// - It has a simple mechanism for preventing uncontrolled resource consumption caused by
//   loading invalid data: user can specify a resource limit in bytes. It is increased whenever
//   Stream is doing some allocation (when loading vector or string). User can also modify and
//   use this mechanism directly. By default the limit is set to 1024 MB.
class BaseStream {
  public:
	using Flag = StreamFlag;

	BaseStream(BaseStream &&);
	void operator=(BaseStream &&);
	virtual ~BaseStream();

	BaseStream(const BaseStream &) = delete;
	void operator=(const BaseStream &) = delete;

	virtual void saveData(CSpan<char>);
	virtual void loadData(Span<char>);

	// It's illegal to seek past the end
	virtual void seek(i64 pos);

	Ex<> getValid();
	bool isValid() const { return !(m_flags & Flag::invalid); }
	bool isLoading() const { return m_flags & Flag::loading; }
	bool isSaving() const { return !(m_flags & Flag::loading); }

	i64 size() const { return m_size; }
	i64 pos() const { return m_pos; }
	bool atEnd() const { return m_pos == m_size; }

	static inline __thread bool t_backtrace_enabled = true;
	void reportError(Str);

	// ----------- Simple serialization functions for different data types --------------

	static constexpr int max_signature_size = 32;

	Ex<> loadSignature(u32);
	Ex<> loadSignature(CSpan<char>);
	Ex<> loadSignature(const char *);
	void saveSignature(u32);
	void saveSignature(CSpan<char>);
	void saveSignature(const char *);

	// Saves single byte if argument is smaller than 248,
	// else saves control byte and 1 to 8 additional bytes
	void saveSize(i64);
	i64 loadSize();

	void saveString(CSpan<char>);
	string loadString();
	// Terminating zero will be added as well
	int loadString(Span<char>);

	void saveVector(CSpan<char>, int element_size = 1);
	PodVector<char> loadVector(int element_size);
	PodVector<char> loadVector(int vector_size, int element_size);

	// ---------------------- Resource limit mechanism ----------------------------------
	// Prevents allocation of too much data (in bytes)

	static constexpr i64 default_resource_limit = 1024 * 1024 * 1024;
	bool addResources(i64 value);
	void setResourceLimit(i64 limit);
	Pair<i64> resourceCounter() const { return {m_resource_counter, m_resource_limit}; }

  protected:
	BaseStream(i64 size, bool is_loading)
		: m_size(size), m_flags(mask(is_loading, Flag::loading)) {}
	virtual string errorMessage(Str) const = 0;

	Dynamic<Error> m_error;
	i64 m_pos = 0, m_size;
	i64 m_resource_limit = default_resource_limit;
	i64 m_resource_counter = 0;
	StreamFlags m_flags;
};

using Stream = TStream<BaseStream>;

// ---------------------- TStream template implementation -------------------------------

#define TEMPLATE template <class TBase>
#define TSTREAM TStream<TBase>

TEMPLATE template <class TSpan, class T, EnableIf<is_flat_data<T>>...>
void TSTREAM::saveData(const TSpan &data) {
	this->saveData(cspan(data).template reinterpret<char>());
}

TEMPLATE template <class TSpan, class T, EnableIf<!is_const<T> && is_flat_data<T>>...>
void TSTREAM::loadData(TSpan &data) {
	this->loadData(span(data).template reinterpret<char>());
}

TEMPLATE template <class T, EnableIf<is_flat_data<T>>...>
TSTREAM &TSTREAM::operator<<(const T &obj) {
	saveData(cspan(&obj, 1).template reinterpret<char>());
	return *this;
}

TEMPLATE template <class T, EnableIf<is_flat_data<T>>...> TSTREAM &TSTREAM::operator>>(T &obj) {
	loadData(span(&obj, 1).template reinterpret<char>());
	return *this;
}

TEMPLATE template <class T, EnableIf<is_flat_data<T>>...>
TSTREAM &TSTREAM::operator>>(vector<T> &vec) {
	auto bytes = BaseStream::loadVector(sizeof(T));
	auto elems = bytes.template reinterpret<T>();
	elems.unsafeSwap(vec);
	return *this;
}

TEMPLATE template <class T, EnableIf<is_flat_data<T>>...>
TSTREAM &TSTREAM::operator<<(const vector<T> &vec) {
	BaseStream::saveVector(cspan(vec).template reinterpret<char>(), sizeof(T));
	return *this;
}

TEMPLATE template <class T, EnableIf<is_flat_data<T>>...> void TSTREAM::saveVector(CSpan<T> vec) {
	BaseStream::saveVector(vec.template reinterpret<char>(), sizeof(T));
}

TEMPLATE template <class T, EnableIf<is_flat_data<T>>...> PodVector<T> TSTREAM::loadVector() {
	auto bytes = BaseStream::loadVector(sizeof(T));
	auto out = bytes.template reinterpret<T>();
	return out;
}
TEMPLATE template <class T, EnableIf<is_flat_data<T>>...>
PodVector<T> TSTREAM::loadVector(int vector_size) {
	auto bytes = BaseStream::loadVector(vector_size, sizeof(T));
	auto out = bytes.template reinterpret<T>();
	return out;
}

TEMPLATE template <class T, EnableIf<is_flat_data<T>>...>
TSTREAM &TSTREAM::operator>>(Maybe<T> &obj) {
	char exists;
	*this >> exists;
	if(exists) {
		T tmp;
		*this >> tmp;
		obj = std::move(tmp);
	} else {
		obj = {};
	}
	return *this;
}

TEMPLATE template <class T, EnableIf<is_flat_data<T>>...>
TSTREAM &TSTREAM::operator<<(const Maybe<T> &obj) {
	*this << char(obj ? 1 : 0);
	if(obj)
		*this << *obj;
	return *this;
}

TEMPLATE template <class... Args, EnableIf<(is_flat_data<Args> && ...)>...>
void TSTREAM::unpack(Args &...args) {
	char buffer[(sizeof(Args) + ...)];
	loadData(buffer);
	int offset = 0;
	((memcpy(&args, buffer + offset, sizeof(Args)), offset += sizeof(Args)), ...);
}

TEMPLATE template <class... Args, EnableIf<(is_flat_data<Args> && ...)>...>
void TSTREAM::pack(const Args &...args) {
	char buffer[(sizeof(Args) + ...)];
	int offset = 0;
	((memcpy(buffer + offset, &args, sizeof(Args)), offset += sizeof(Args)), ...);
	saveData(buffer);
}

TEMPLATE TSTREAM &TSTREAM::operator<<(Str str) {
	this->saveString(str);
	return *this;
}

TEMPLATE TSTREAM &TSTREAM::operator>>(string &str) {
	str = this->loadString();
	return *this;
}

#undef TEMPLATE
#undef TSTREAM
}
