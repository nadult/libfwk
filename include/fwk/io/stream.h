// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/enum_flags.h"
#include "fwk/pod_vector.h"
#include "fwk/span.h"
#include "fwk/str.h"
#include "fwk/sys_base.h"

namespace fwk {

DEFINE_ENUM(StreamFlag, loading, invalid);
using StreamFlags = EnumFlags<StreamFlag>;

namespace stream_limits {
	inline constexpr int max_signature_size = 32, default_max_string_size = 64 * 1024 * 1024,
						 default_max_vector_size = 1024 * 1024 * 1024;
}

namespace detail {
	template <class S, class T> struct StreamTraits {
		FWK_SFINAE_TEST(saveable, T, DECLVAL(U).save(DECLVAL(S&)));
		FWK_SFINAE_TEST(loadable, T, DECLVAL(U).save(DECLVAL(S&)));
	};
}

// TODO: expand this
template <class TStream, class T>
inline constexpr bool stream_saveable = detail::StreamTraits<TStream, T>::saveable;
template <class TStream, class T>
inline constexpr bool stream_loadable = detail::StreamTraits<TStream, T>::loadable;

// This class provides convenient interface for serialization of data
template <class Base> class TStream : public Base {
  public:
	using Base::Base;
	using Base::loadData;
	using Base::saveData;
	using Base::seek;

	using Base::isValid;

	template <class TSpan, class T = SpanBase<TSpan>> void saveData(const TSpan &data) {
		this->saveData(cspan(data).template reinterpret<char>());
	}
	template <class TSpan, class T = SpanBase<TSpan>, EnableIf<!is_const<T>>...>
	void loadData(TSpan &&data) {
		this->loadData(span(data).template reinterpret<char>());
	}

	// -------------------------------------------------------------------------------------------
	// ---  Saving/loading objects   -------------------------------------------------------------

	template <class T, EnableIf<is_flat_data<T>>...> TStream &operator<<(const T &obj) {
		return saveData(cspan(&obj, 1)), *this;
	}
	template <class T, EnableIf<stream_saveable<TStream, T>>...> TStream &operator<<(const T &obj) {
		obj.save(*this);
		return *this;
	}

	template <class T, EnableIf<is_flat_data<T>>...> TStream &operator>>(T &obj) {
		return loadData(span(&obj, 1)), *this;
	}
	template <class T, EnableIf<stream_loadable<TStream, T>>...> TStream &operator>>(T &obj) {
		obj.load(*this);
		return *this;
	}
	template <class T> TStream &operator>>(Maybe<T> &obj) {
		char exists;
		*this >> exists;
		if(exists) {
			T tmp;
			*this >> tmp;
			obj = move(tmp);
		} else {
			obj = {};
		}
		return *this;
	}

	template <class T> TStream &operator<<(const Maybe<T> &obj) {
		*this << char(obj ? 1 : 0);
		if(obj)
			*this << *obj;
		return *this;
	}

	// TODO: better name
	template <class... Args, EnableIf<(is_flat_data<Args> && ...)>...> void unpack(Args &... args) {
		char buffer[(sizeof(Args) + ...)];
		loadData(buffer);
		int offset = 0;
		((memcpy(&args, buffer + offset, sizeof(Args)), offset += sizeof(Args)), ...);
	}

	template <class... Args, EnableIf<(is_flat_data<Args> && ...)>...>
	void pack(const Args &... args) {
		char buffer[(sizeof(Args) + ...)];
		int offset = 0;
		((memcpy(buffer + offset, &args, sizeof(Args)), offset += sizeof(Args)), ...);
		saveData(buffer);
	}

	template <class T, EnableIf<is_flat_data<T>>...> void saveVector(CSpan<T> vec) {
		saveVector(vec.template reinterpret<char>(), sizeof(T));
	}

	PodVector<char> loadVector(int max_size = stream_limits::default_max_vector_size,
							   int element_size = 1);

	template <class T, EnableIf<is_flat_data<T>>...>
	PodVector<T> loadVector(int max_size = stream_limits::default_max_vector_size) {
		auto out = loadVector(max_size, sizeof(T));
		return move(reinterpret_cast<PodVector<T> &>(out)); // TODO: use PodVector::reinterpet
	}

	TStream &operator<<(Str);
	TStream &operator>>(string &);

	// -------------------------------------------------------------------------------------------
	// ---  Low-level serialization functions   --------------------------------------------------

	// If size < 254: saves single byte, else saves 5 or 9 bytes
	void saveSize(i64);
	void saveString(CSpan<char>);
	void saveVector(CSpan<char>, int element_size = 1);

	i64 loadSize();
	string loadString(int max_size = stream_limits::default_max_string_size);
	// Terminating zero will be added as well
	int loadString(Span<char>);
};

// TODO: może jednak dobrze by było zrobić podział na istream i ostream ?

// TODO: add EXCEPT
// Generic stream class, you can also use other stream classes directly (for performance)
// Cannot be copied, but can be moved
class BaseStream {
  public:
	using Flag = StreamFlag;

	BaseStream(BaseStream &&);
	BaseStream &operator=(BaseStream &&);
	virtual ~BaseStream();

	BaseStream(const BaseStream &) = delete;
	void operator=(const BaseStream &) = delete;

	virtual void saveData(CSpan<char>) EXCEPT;
	virtual void loadData(Span<char>) EXCEPT;

	// It's illegal to seek past the end
	virtual void seek(i64 pos);

	bool isValid() const { return !(m_flags & Flag::invalid); }
	bool isLoading() const { return m_flags & Flag::loading; }
	bool isSaving() const { return !(m_flags & Flag::loading); }

	i64 size() const { return m_size; }
	i64 pos() const { return m_pos; }
	bool atEnd() const { return m_pos == m_size; }
	void clear();

	// Serializes signatures; While saving, it simply writes it to a stream
	// When loading, it will report an error if signature is not exactly matched
	void signature(u32);
	void signature(CSpan<char>);
	void signature(const char *str) { signature(cspan(Str(str))); }

  protected:
	BaseStream(i64 size, bool is_loading)
		: m_pos(0), m_size(size), m_flags(mask(is_loading, Flag::loading)) {}

	virtual void raise(ZStr) EXCEPT;

	i64 m_pos, m_size;
	StreamFlags m_flags;
};

using Stream = TStream<BaseStream>;
}
