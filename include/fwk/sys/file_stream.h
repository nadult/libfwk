// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/pod_vector.h"
#include "fwk/str.h"
#include "fwk/sys_base.h"
#include "fwk_range.h"

namespace fwk {

// Simple class for loading/saving plain data from/to file
// It treats all data as plain bytes, so you have to be very careful when using it.
// There are also special functions for loading/saving strings & PodVectors.
//
// FWK exceptions are used for error handling; besides that:
// - when error happens all following operations will not be performed
// - when loading: returned data will be zeroed, vectors & strings will be empty
class EXCEPT FileStream {
  public:
	static constexpr int default_max_string_size = 64 * 1024 * 1024;
	static constexpr int default_max_vector_size = 1024 * 1024 * 1024;

	FileStream(FileStream &&);
	FileStream(const FileStream &) = delete;
	~FileStream();

	void operator=(FileStream &&);
	void operator=(const FileStream &) = delete;

	ZStr name() const { return m_name; }
	i64 size() const { return m_size; }
	i64 pos() const { return m_pos; }

	bool isLoading() const { return m_is_loading; }
	bool isSaving() const { return !m_is_loading; }
	bool valid() const { return m_is_valid; }

	// Co serializujemy:
	// - spany (same dane bez wielkości)
	//   saveData/loadData
	//
	// - stringi
	//
	// - wektory
	//
	// - całe obiekty
	//   pack/unpack
	//
	// interfejs dla innych obiektów musi być zupełnie inny niż dla wektorów/stringów ?
	//   jakie zabezpieczenie przed zapisywaniem wektora wektorów ?

	void saveData(CSpan<char>);
	void loadData(Span<char>);
	void seek(i64 pos);

	template <class TSpan, class T = SpanBase<TSpan>> void saveData(const TSpan &data) {
		saveData(cspan(data).template reinterpret<char>());
	}
	template <class TSpan, class T = SpanBase<TSpan>, EnableIf<!is_const<T>>...>
	void loadData(TSpan &&data) {
		loadData(span(data).template reinterpret<char>());
	}

	// -------------------------------------------------------------------------------------------
	// ---  Saving/loading POD objects   ---------------------------------------------------------
	//
	// is_flat_data<T> has to be true for serialization to work

	template <class T, EnableIf<is_flat_data<T>>...> FileStream &operator<<(const T &obj) {
		return saveData(cspan(&obj, 1)), *this;
	}
	template <class T, EnableIf<is_flat_data<T>>...> FileStream &operator>>(T &obj) {
		return loadData(span(&obj, 1)), *this;
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

	// -------------------------------------------------------------------------------------------
	// ---  Saving/loading strings & vectors of POD objects  -------------------------------------

	// If size < 254: saves single byte, else saves 5 or 9 bytes
	i64 loadSize();
	void saveSize(i64);

	string loadString(i64 max_size = default_max_string_size);

	// Terminating zero will be added as well
	int loadString(Span<char>);
	void saveString(CSpan<char>);

	PodVector<char> loadVector(int max_size = default_max_vector_size, int element_size = 1);
	void saveVector(CSpan<char>, int element_size = 1);

	template <class T, EnableIf<is_flat_data<T>>...>
	PodVector<T> loadVector(int max_size = default_max_vector_size) {
		return loadVector(max_size, sizeof(T)).template reinterpret<T>();
	}
	template <class T, EnableIf<is_flat_data<T>>...> void saveVector(CSpan<T> vec) {
		saveVector(vec.template reinterpret<char>(), sizeof(T));
	}

	// Serializes signatures; While saving, it simply writes it to a stream
	// When loading, it will report an error if signature is not exactly matched
	void signature(u32);
	// Max length: 32
	void signature(Str);

  private:
	void raise(Str) NOINLINE;
	FileStream();
	friend Ex<FileStream> fileStream(ZStr, bool);

	string m_name;
	void *m_file;
	long long m_size, m_pos;
	bool m_is_loading, m_is_valid = true;
};

Ex<FileStream> fileLoader(ZStr file_name);
Ex<FileStream> fileSaver(ZStr file_name);
}
