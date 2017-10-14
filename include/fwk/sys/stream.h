// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys_base.h"
#include "fwk_vector.h"

namespace fwk {

template <class T> struct SerializeAsPod;

class Stream {
  public:
	Stream(bool is_loading);
	virtual ~Stream() {}

	virtual const char *name() const { return ""; }

	long long size() const { return m_size; }
	long long pos() const { return m_pos; }

	void saveData(const void *ptr, int bytes);
	void loadData(void *ptr, int bytes);
	void seek(long long pos);

	int loadString(char *buffer, int size);
	void saveString(const char *, int size = 0);

	bool isLoading() const { return m_is_loading; }
	bool isSaving() const { return !m_is_loading; }
	bool allOk() const { return !m_error_handled; }

	//! Serializes 32-bit signature; While saving, it simply writes it to stream,
	//! while loading it CHECKs if loaded signature is equal to sig
	void signature(u32 sig);

	// Max length: 32
	void signature(const char *sig, int length);

	template <class... Args> void unpack(Args &... args) {
		char buffer[SumSize<Args...>::value];
		loadData(buffer, sizeof(buffer));
		CopyFrom<Args...>::copy(buffer, args...);
	}

	template <class... Args> void pack(const Args &... args) {
		char buffer[SumSize<Args...>::value];
		CopyTo<Args...>::copy(buffer, args...);
		saveData(buffer, sizeof(buffer));
	}

	template <class T> Stream &operator<<(const T &obj) {
		saveToStream(obj, *this);
		return *this;
	}

	template <class T> Stream &operator>>(T &obj) {
		loadFromStream(obj, *this);
		return *this;
	}

	static string handleError(void *stream_ptr, void *) NOINLINE;

  private:
	template <class... Args> struct SumSize {
		enum { value = 0 };
	};
	template <class T, class... Args> struct SumSize<T, Args...> {
		enum { value = sizeof(T) + SumSize<Args...>::value };
	};
	template <class T> struct SumSize<T> {
		enum { value = sizeof(T) };
	};

	template <class... Args> struct CopyTo {};
	template <class T1> struct CopyTo<T1> {
		static_assert(SerializeAsPod<T1>::value, "Attempt to multi serialize non-pod object");
		static void copy(void *ptr, const T1 &src) { *(T1 *)ptr = src; }
	};
	template <class T1, class... Args> struct CopyTo<T1, Args...> {
		static_assert(SerializeAsPod<T1>::value, "Attempt to multi serialize non-pod object");
		static void copy(void *ptr, const T1 &src, const Args &... args) {
			*(T1 *)ptr = src;
			CopyTo<Args...>::copy((char *)ptr + sizeof(T1), args...);
		}
	};

	template <class... Args> struct CopyFrom {};
	template <class T1> struct CopyFrom<T1> {
		static_assert(SerializeAsPod<T1>::value, "Attempt to multi serialize non-pod object");
		static void copy(void *ptr, T1 &dst) { dst = *(T1 *)ptr; }
	};
	template <class T1, class... Args> struct CopyFrom<T1, Args...> {
		static_assert(SerializeAsPod<T1>::value, "Attempt to multi serialize non-pod object");
		static void copy(void *ptr, T1 &dst, Args &... args) {
			dst = *(T1 *)ptr;
			CopyFrom<Args...>::copy((char *)ptr + sizeof(T1), args...);
		}
	};

	template <class T, bool pod> struct Serialization {
		static void doLoad(T &obj, Stream &sr) { obj.load(sr); }
		static void doSave(const T &obj, Stream &sr) { obj.save(sr); }
	};

	template <class T> struct Serialization<T, true> {
		static void doLoad(T &obj, Stream &sr) { sr.loadData(&obj, sizeof(T)); }
		static void doSave(const T &obj, Stream &sr) { sr.saveData(&obj, sizeof(T)); }
	};

	template <class T, int tsize, bool pod> struct Serialization<T[tsize], pod> {
		static void doLoad(T (&obj)[tsize], Stream &sr) {
			for(size_t n = 0; n < tsize; n++)
				obj[n].load(sr);
		};
		static void doSave(const T (&obj)[tsize], Stream &sr) {
			for(size_t n = 0; n < tsize; n++)
				obj[n].save(sr);
		};
	};

	template <class T, int tsize> struct Serialization<T[tsize], true> {
		static void doLoad(T (&obj)[tsize], Stream &sr) { sr.loadData(&obj[0], sizeof(T) * tsize); }
		static void doSave(const T (&obj)[tsize], Stream &sr) {
			sr.saveData(&obj[0], sizeof(T) * tsize);
		}
	};

	template <class T> friend void loadFromStream(T &, Stream &);
	template <class T> friend void saveToStream(const T &, Stream &);

	friend class Loader;
	friend class Saver;

  protected:
	virtual void v_load(void *, int);
	virtual void v_save(const void *, int);
	virtual void v_seek(long long pos);

	long long m_size, m_pos;
	bool m_error_handled, m_is_loading;
};

// You can derive your classes from this
struct TraitSerializeAsPod {};

// You can specialize this for your classes
template <class T> struct SerializeAsPod {
	template <typename U, void (U::*)(Stream &)> struct SFINAE {};

	template <typename U> static char test(SFINAE<U, &U::load> *);
	template <typename U> static char test(SFINAE<U, &U::save> *);
	template <typename U> static long test(...);

	enum { has_serialize_method = sizeof(test<T>(0)) == 1 };
	enum {
		value = std::is_base_of<TraitSerializeAsPod, T>::value ||
				(std::is_pod<T>::value && !has_serialize_method)
	};
	enum { arraySize = 1 };
};

template <class T, int size> struct SerializeAsPod<T[size]> {
	enum { value = SerializeAsPod<T>::value };
	enum { arraySize = size };
};

void loadFromStream(string &, Stream &);
void saveToStream(const string &, Stream &);

template <class T> void loadFromStream(vector<T> &v, Stream &sr) {
	u32 size;
	sr.loadData(&size, sizeof(size));
	// TODO: check for vector size ?
	v.resize(size);

	if(SerializeAsPod<T>::value)
		sr.loadData(&v[0], sizeof(T) * size);
	else
		for(u32 n = 0; n < size; n++)
			loadFromStream(v[n], sr);
}

template <class T> void saveToStream(const vector<T> &v, Stream &sr) {
	u32 size;
	size = u32(v.size());
	ASSERT(size == size_t(v.size()));
	sr.saveData(&size, sizeof(size));

	if(SerializeAsPod<T>::value)
		sr.saveData(&v[0], sizeof(T) * size);
	else
		for(u32 n = 0; n < size; n++)
			saveToStream(v[n], sr);
}

template <class T> void loadFromStream(T &obj, Stream &sr) {
	Stream::template Serialization<T, SerializeAsPod<T>::value>::doLoad(obj, sr);
	static_assert(!std::is_pointer<T>::value && !std::is_reference<T>::value,
				  "Automatic serialization of pointers and references is not avaliable");
}

template <class T> void saveToStream(const T &obj, Stream &sr) {
	Stream::template Serialization<T, SerializeAsPod<T>::value>::doSave(obj, sr);
	static_assert(!std::is_pointer<T>::value && !std::is_reference<T>::value,
				  "Automatic serialization of pointers and references is not avaliable");
}

// Buffered, stdio based
class FileStream : public Stream {
  public:
	FileStream(const char *file_name, bool is_loading);
	FileStream(const string &file_name, bool is_loading)
		: FileStream(file_name.c_str(), is_loading) {}
	~FileStream();

	FileStream(const FileStream &) = delete;
	void operator=(const FileStream &) = delete;

	const char *name() const override;

  protected:
	void v_load(void *, int) override;
	void v_save(const void *, int) override;
	void v_seek(long long) override;
	static void rbFree(void *);

	void *m_file;
	string m_name;
	int m_rb_index = -1;
};

class Loader : public FileStream {
  public:
	Loader(const char *file_name) : FileStream(file_name, true) {}
	Loader(const string &file_name) : FileStream(file_name.c_str(), true) {}
};

class Saver : public FileStream {
  public:
	Saver(const char *file_name) : FileStream(file_name, false) {}
	Saver(const string &file_name) : FileStream(file_name.c_str(), false) {}
};

class MemoryLoader : public Stream {
  public:
	MemoryLoader(const char *data, long long size);
	MemoryLoader(vector<char> &data) : MemoryLoader(data.data(), data.size()) {}

  protected:
	void v_load(void *, int) override;

	const char *m_data;
};

class MemorySaver : public Stream {
  public:
	MemorySaver(char *data, long long size);
	MemorySaver(vector<char> &data) : MemorySaver(data.data(), data.size()) {}

  protected:
	void v_save(const void *, int) override;

	char *m_data;
};
}
