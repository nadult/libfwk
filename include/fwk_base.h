/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef FWK_BASE_H
#define FWK_BASE_H

#include <vector>
#include <string>
#include <exception>
#include <type_traits>
#include <memory>
#include <map>
#include <cstring>
#include <cstdlib>

namespace fwk {

using std::swap;
using std::pair;
using std::make_pair;
using std::string;
using std::vector;
using std::unique_ptr;

// TODO: use types from cstdint
using uint = unsigned int;
using u8 = unsigned char;
using i8 = char;
using u16 = unsigned short;
using i16 = short;
using u32 = unsigned;
using i32 = int;

#if __cplusplus < 201402L
template <typename T, typename... Args> unique_ptr<T> make_unique(Args &&... args) {
	return unique_ptr<T>(new T(std::forward<Args>(args)...));
}
#else
using std::make_unique;
#endif

// Compile your program with -rdynamic to get some interesting info
// Currently not avaliable on mingw32 platform
const string backtrace(size_t skip = 0);

// Uses c++filt program to demangle C++ names; also it shortens some of the common
// long class names, like std::basic_string<...> to fwk::string
const string cppFilterBacktrace(const string &);

class Exception : public std::exception {
  public:
	Exception(const char *);
	Exception(const string &);
	Exception(const string &, const string &);
	~Exception() noexcept {}
	const char *what() const noexcept { return m_data.c_str(); }
	const char *backtrace() const noexcept { return m_backtrace.c_str(); }

  protected:
	string m_data, m_backtrace;
};

#define FWK_STRINGIZE(something) FWK_STRINGIZE_(something)
#define FWK_STRINGIZE_(something) #something

void throwException(const char *file, int line, const char *fmt, ...);
void doAssert(const char *file, int line, const char *str);
void handleCtrlC(void (*handler)());
void sleep(double sec);
double getTime();

// TODO: maybe FILE / LINE is not required if we have backtraces?

#define THROW(...) fwk::throwException(__FILE__, __LINE__, __VA_ARGS__)

// TODO: add fatal exception and use it for asserts
// TODO: add special assert for verifying input files, which throws normal exception

#define ASSERT(expr)                                                                               \
	({                                                                                             \
		if(!(expr))                                                                                \
			fwk::doAssert(__FILE__, __LINE__, FWK_STRINGIZE(expr));                                \
	})

#ifdef NDEBUG
#define DASSERT(expr) ({})
#else
#define DASSERT(expr) ASSERT(expr)
#endif

template <class T> inline constexpr bool isOneOf(const T &value) { return false; }

template <class T, class Arg1, class... Args>
inline constexpr bool isOneOf(const T &value, const Arg1 &arg1, const Args &... args) {
	return value == arg1 || isOneOf(value, args...);
}

template <class T1, class T2> inline bool isOneOf(const T1 &value, const vector<T2> &vec) {
	for(const auto &item : vec)
		if(value == item)
			return true;
	return false;
}

template <class T> inline T max(T a, T b) { return a < b ? b : a; }
template <class T> inline T min(T a, T b) { return a > b ? b : a; }

template <class T> bool operator!=(const T &a, const T &b) { return !(a == b); }

template <class T, int size> constexpr int arraySize(T (&)[size]) noexcept { return size; }

void logError(const string &error);

// TODO: move to string_ref.cpp
// Simple reference to string
// User have to make sure that referenced data is alive as long as StringRef
class StringRef {
  public:
	StringRef(const string &str) : m_ptr(str.c_str()), m_len((int)str.size()) {}
	StringRef(const char *str, int len) : m_ptr(str), m_len(len) {
		DASSERT((int)strlen(str) == len);
	}
	StringRef(const char *str) : m_ptr(str), m_len(strlen(str)) {}
	StringRef() : m_ptr(nullptr), m_len(0) {}
	explicit operator const char *() const { return m_ptr; }
	const char *c_str() const { return m_ptr; }
	bool isValid() const { return m_ptr != nullptr; }
	int size() const { return m_len; }
	bool isEmpty() const { return m_len == 0; }

	StringRef operator+(int offset) const {
		DASSERT(offset >= 0 && offset <= m_len);
		return StringRef(m_ptr + offset, m_len - offset);
	}

  private:
	const char *m_ptr;
	int m_len;
};

inline bool operator==(const StringRef a, const StringRef b) {
	DASSERT(a.isValid() && b.isValid());
	return a.size() == b.size() && strcmp(a.c_str(), b.c_str()) == 0;
}

inline bool operator<(const StringRef a, const StringRef b) {
	DASSERT(a.isValid() && b.isValid());
	return strcmp(a.c_str(), b.c_str()) < 0;
}

inline bool caseEqual(const StringRef a, const StringRef b) {
	DASSERT(a.isValid() && b.isValid());
	return a.size() == b.size() && strcasecmp(a.c_str(), b.c_str()) == 0;
}

inline bool caseNEqual(const StringRef a, const StringRef b) { return !caseEqual(a, b); }

inline bool caseLess(const StringRef a, const StringRef b) {
	DASSERT(a.isValid() && b.isValid());
	return strcasecmp(a.c_str(), b.c_str()) < 0;
}

int fromString(const char *str, const char **strings, int count);

#define DECLARE_ENUM(type, ...)                                                                    \
	namespace type {                                                                               \
		enum Type : char { __VA_ARGS__, count };                                                   \
		const char *toString(int);                                                                 \
		Type fromString(const char *);                                                             \
		inline constexpr bool isValid(int val) { return val >= 0 && val < count; }                 \
	}

#define DEFINE_ENUM(type, ...)                                                                     \
	namespace type {                                                                               \
		static const char *s_strings[] = {__VA_ARGS__};                                            \
		static_assert(arraySize(s_strings) == count, "String count does not match enum count");    \
		const char *toString(int value) {                                                          \
			DASSERT(isValid(value));                                                               \
			return s_strings[value];                                                               \
		}                                                                                          \
		Type fromString(const char *str) { return (Type)fwk::fromString(str, s_strings, count); }  \
	}

template <class T> struct SerializeAsPod;

class Stream {
  public:
	Stream(bool is_loading);
	virtual ~Stream() noexcept {}

	virtual const char *name() const noexcept { return ""; }

	long long size() const noexcept { return m_size; }
	long long pos() const noexcept { return m_pos; }

	void saveData(const void *ptr, int bytes);
	void loadData(void *ptr, int bytes);
	void seek(long long pos);

	int loadString(char *buffer, int size);
	void saveString(const char *, int size = 0);

	void encodeInt(int);
	int decodeInt();

	bool isLoading() const noexcept { return m_is_loading; }
	bool isSaving() const noexcept { return !m_is_loading; }
	bool allOk() const noexcept { return !m_exception_thrown; }

	//! Serializes 32-bit signature; While saving, it simply writes it to stream,
	//! while loading it checks if loaded signature is equal to sig, if not
	//! it throws an exception
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

	void handleException(const Exception &) __attribute__((noinline));

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
		static void doLoad(T &obj, Stream &sr) {
			try {
				obj.load(sr);
			} catch(const Exception &ex) { sr.handleException(ex); }
		}
		static void doSave(const T &obj, Stream &sr) {
			try {
				obj.save(sr);
			} catch(const Exception &ex) { sr.handleException(ex); }
		}
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
	virtual void v_load(void *, int) { THROW("v_load unimplemented"); }
	virtual void v_save(const void *, int) { THROW("v_save unimplemented"); }
	virtual void v_seek(long long pos) { m_pos = pos; }

	long long m_size, m_pos;
	bool m_exception_thrown, m_is_loading;
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

#define SERIALIZE_AS_POD(type)                                                                     \
	namespace fwk {                                                                                \
		template <> struct SerializeAsPod<type> {                                                  \
			enum { value = 1 };                                                                    \
		};                                                                                         \
	}

void loadFromStream(string &, Stream &);
void saveToStream(const string &, Stream &);

template <class T, class A> void loadFromStream(vector<T, A> &v, Stream &sr) {
	u32 size;
	sr.loadData(&size, sizeof(size));
	try {
		v.resize(size);
	} catch(Exception &ex) { sr.handleException(ex); }

	if(SerializeAsPod<T>::value)
		sr.loadData(&v[0], sizeof(T) * size);
	else
		for(u32 n = 0; n < size; n++)
			loadFromStream(v[n], sr);
}

template <class T, class A> void saveToStream(const vector<T, A> &v, Stream &sr) {
	u32 size;
	size = u32(v.size());
	if(size_t(size) != v.size())
		sr.handleException(Exception("Vector size too big (> 2^32) for serializer to handle"));
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
	~FileStream() noexcept;

	FileStream(const FileStream &) = delete;
	void operator=(const FileStream &) = delete;

	const char *name() const noexcept override;

  protected:
	void v_load(void *, int) override;
	void v_save(const void *, int) override;
	void v_seek(long long) override;

	void *m_file;
	string m_name;
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

// TODO: use shared_ptr<>
// Use together with Ptr<>
class RefCounter {
  public:
	RefCounter() : m_ref_count(0) {}

	// use it if your class is embedded into another
	void incRefs() { m_ref_count++; }
	size_t numRefs() const { return m_ref_count; }
	virtual ~RefCounter() {}

	RefCounter(const RefCounter &) : m_ref_count(0) {}
	RefCounter(RefCounter &&) : m_ref_count(0) {}
	RefCounter &operator=(const RefCounter &) { return *this; }
	RefCounter &operator=(RefCounter &&) { return *this; }

  private:
	u32 m_ref_count;
	template <class T> friend class Ptr;
	template <class T> friend class ResourceMgr;
};

template <class T> class Ptr {
  public:
	Ptr() noexcept : m_ptr(0) {}
	~Ptr() noexcept {
		if(m_ptr)
			decRefs();
	}
	Ptr(T *obj) noexcept : m_ptr(obj) {
		if(m_ptr)
			m_ptr->m_ref_count++;
	}
	Ptr(const Ptr &p) noexcept : m_ptr(p.m_ptr) {
		if(m_ptr)
			m_ptr->m_ref_count++;
	}
	Ptr &operator=(const Ptr &p) noexcept {
		if(p.m_ptr != m_ptr) {
			if(m_ptr)
				decRefs();
			if((m_ptr = p.m_ptr))
				m_ptr->m_ref_count++;
		}
		return *this;
	}

	bool operator==(const Ptr &t) const noexcept { return m_ptr == t.m_ptr; }
	bool operator!=(const Ptr &t) const noexcept { return m_ptr != t.m_ptr; }

	bool operator==(const T *t) const noexcept { return m_ptr == t; }
	bool operator!=(const T *t) const noexcept { return m_ptr != t; }

	template <class TT> bool operator==(const TT *rhs) const noexcept {
		static_assert(std::is_base_of<T, TT>::value || std::is_base_of<TT, T>::value, "");
		return ((void *)m_ptr) == ((void *)rhs);
	}
	template <class TT> bool operator!=(const TT *rhs) const noexcept {
		static_assert(std::is_base_of<T, TT>::value || std::is_base_of<TT, T>::value, "");
		return ((void *)m_ptr) != ((void *)rhs);
	}
	template <class TT> bool operator==(const Ptr<TT> &rhs) const noexcept {
		static_assert(std::is_base_of<T, TT>::value || std::is_base_of<TT, T>::value, "");
		return ((void *)m_ptr) == ((void *)rhs.m_ptr);
	}
	template <class TT> bool operator!=(const Ptr<TT> &rhs) const noexcept {
		static_assert(std::is_base_of<T, TT>::value || std::is_base_of<TT, T>::value, "");
		return ((void *)m_ptr) != ((void *)rhs.m_ptr);
	}

	void reset() noexcept {
		if(m_ptr) {
			decRefs();
			m_ptr = 0;
		}
	}

	explicit operator bool() const noexcept { return m_ptr ? 1 : 0; }

	T *operator->() const noexcept { return m_ptr; }
	T &operator*() const noexcept { return *m_ptr; }
	T *get() const noexcept { return m_ptr; }

	Ptr copy() { return new T(*m_ptr); }

	void serializePtr(Stream &sr) {
		DASSERT(m_ptr && "Attempt to serialize null pointer");
		serialize(*m_ptr, sr);
	}

  protected:
	void decRefs() noexcept {
		m_ptr->m_ref_count--;
		if(m_ptr->m_ref_count == 0)
			delete m_ptr;
	}

	template <class TT> friend class WeakPtr;
	template <class TT> friend class Ptr;
	T *m_ptr;
};

template <class TT, class T> bool operator==(const TT *lhs, const Ptr<T> &rhs) {
	static_assert(std::is_base_of<T, TT>::value || std::is_base_of<TT, T>::value, "");
	return lhs == rhs.get();
}

template <class TT, class T> bool operator!=(const TT *lhs, const Ptr<T> &rhs) {
	static_assert(std::is_base_of<T, TT>::value || std::is_base_of<TT, T>::value, "");
	return lhs != rhs.get();
}

/* Simple pointer, that implicitly initializes to 0 */
template <class T> class WeakPtr {
  public:
	WeakPtr() : ptr(0) {}
	WeakPtr(T *ptr) : ptr(ptr) {}
	WeakPtr(const Ptr<T> &ptr) : ptr(ptr.ptr) {}

	template <class TT> bool operator==(const WeakPtr<TT> &t) { return ptr == t.ptr; }
	template <class TT> bool operator!=(const WeakPtr<TT> &t) { return ptr != t.ptr; }

	operator T *() { return ptr; }
	operator const T *() const { return ptr; }
	T *operator->() { return ptr; }
	const T *operator->() const { return ptr; }

  protected:
	T *ptr;
};

template <class T> class ResourceMgr;

class Resource : public RefCounter {
  public:
	virtual void load(Stream &sr) = 0;

	virtual ~Resource() {}

	const char *resourceName() const { return m_resource_name.c_str(); }
	void setResourceName(const char *name) { m_resource_name = name; }

  private:
	string m_resource_name;
};

template <class T> class ResourceMgr {
  public:
	ResourceMgr(const string prefix, const string suffix) : m_prefix(prefix), m_suffix(suffix) {}
	~ResourceMgr() {}

	//! Query for a resource,
	//! throws an exception if the resource could not be found
	Ptr<T> getResource(const string &name) {
		auto it = m_dict.find(name);
		if(it == m_dict.end()) {
			Ptr<T> tmp = new T;
			if(tmp) {
				Loader ldr(m_prefix + name + m_suffix);
				if(ldr.allOk()) {
					tmp->setResourceName(name.c_str());
					tmp->load(ldr);
					if(ldr.allOk()) {
						m_dict[name] = tmp;
						return tmp;
					}
				}
			}
			THROW("Cannot find resource %s (file name: %s%s%s).", name.c_str(), m_prefix.c_str(),
				  name.c_str(), m_suffix.c_str());
		}
		return it->second;
	}

	Ptr<T> findResource(const string &name) const {
		auto it = m_dict.find(name);
		if(it == m_dict.end())
			return Ptr<T>(nullptr);
		return it->second;
	}

	Ptr<T> operator[](const string &name) { return getResource(name); }

	const string &prefix() const { return m_prefix; }
	const string &suffix() const { return m_suffix; }

	// Functor takes two parameters: name and object
	template <class Functor> void iterateOver(Functor func) {
		for(auto it = m_dict.begin(); it != m_dict.end(); ++it)
			func(it->first, *it->second);
	}

	// TODO: provide unload, make it secure, like add additonal flag to refcounter
	Ptr<T> load(const string &name) {
		Ptr<T> res = getResource(name);
		res->m_ref_count++;
		return res;
	}

	void freeResource(const string &name) {
		auto iter = m_dict.find(name);
		DASSERT(iter != m_dict.end());
		m_dict.erase(iter);
	}

	void saveResource(const string &name) {
		auto it = m_dict.find(name);
		DASSERT(it != m_dict.end() && it->second);
		Saver svr(m_prefix + name + m_suffix);
		it->second->load(svr);
	}

	void renameResource(const string &old_name, const string &new_name) {
		auto old_it = m_dict.find(old_name);
		auto new_it = m_dict.find(new_name);

		DASSERT(old_it != m_dict.end());
		DASSERT(new_it == m_dict.end());

		m_dict[new_name] = old_it->second;
		old_it->second->setResourceName(new_name.c_str());
		m_dict.erase(old_it);
	}

	void clear() { m_dict.clear(); }

  private:
	typename std::map<string, Ptr<T>> m_dict;
	string m_prefix, m_suffix;
};

template <class T> class ClonablePtr : public unique_ptr<T> {
  public:
	ClonablePtr(const ClonablePtr &rhs) : unique_ptr<T>(rhs ? rhs->clone() : nullptr) {}
	ClonablePtr(ClonablePtr &&rhs) : unique_ptr<T>(std::move(rhs)) {}
	ClonablePtr(T *ptr) : unique_ptr<T>(ptr) {}
	ClonablePtr() {}

	explicit operator bool() const { return unique_ptr<T>::operator bool(); }
	bool isValid() const { return unique_ptr<T>::operator bool(); }

	void operator=(ClonablePtr &&rhs) { unique_ptr<T>::operator=(std::move(rhs)); }
	void operator=(const ClonablePtr &rhs) {
		if(&rhs == this)
			return;
		T *clone = rhs ? rhs->clone() : nullptr;
		unique_ptr<T>::reset(clone);
	}
};

// Very simple and efficent vector for POD Types; Use with care:
// - user is responsible for initializing the data
// - when resizing, data is destroyed
// TODO: derive from PodArrayBase, resize, and serialize can be shared (modulo sizeof(T)
// multiplier)
template <class T> class PodArray {
  public:
	PodArray() : m_data(nullptr), m_size(0) {}
	explicit PodArray(int size) : m_data(nullptr), m_size(0) { resize(size); }
	PodArray(const PodArray &rhs) : m_size(rhs.m_size) {
		m_data = (T *)malloc(m_size * sizeof(T));
		memcpy(m_data, rhs.m_data, sizeof(T) * m_size);
	}
	PodArray(const T *data, int data_size) : m_size(data_size) {
		m_data = (T *)malloc(m_size * sizeof(T));
		memcpy(m_data, data, data_size * sizeof(T));
	}
	PodArray(PodArray &&rhs) : m_size(rhs.m_size), m_data(rhs.m_data) {
		rhs.m_data = nullptr;
		rhs.m_size = 0;
	}
	~PodArray() { resize(0); }

	void operator=(const PodArray &rhs) {
		if(&rhs == this)
			return;
		resize(rhs.m_size);
		memcpy(m_data, rhs.m_data, rhs.m_size);
	}
	void operator=(PodArray &&rhs) {
		if(&rhs == this)
			return;
		clear();
		m_data = rhs.m_data;
		m_size = rhs.m_size;
		rhs.m_data = nullptr;
		rhs.m_size = 0;
	}
	void load(Stream &sr) __attribute__((noinline));
	void save(Stream &sr) const __attribute__((noinline));
	void resize(int new_size) __attribute__((noinline));

	void swap(PodArray &rhs) {
		fwk::swap(m_data, rhs.m_data);
		fwk::swap(m_size, rhs.m_size);
	}

	void clear() {
		m_size = 0;
		free(m_data);
		m_data = nullptr;
	}
	bool isEmpty() const { return m_size == 0; }

	T *data() { return m_data; }
	const T *data() const { return m_data; }

	T *end() { return m_data + m_size; }
	const T *end() const { return m_data + m_size; }

	T &operator[](int idx) { return m_data[idx]; }
	const T &operator[](int idx) const { return m_data[idx]; }

	int size() const { return m_size; }
	int dataSize() const { return m_size * (int)sizeof(T); }

  private:
	T *m_data;
	int m_size;
};

template <class T> void PodArray<T>::load(Stream &sr) {
	i32 size;
	sr >> size;
	ASSERT(size >= 0);

	resize(size);
	if(m_data)
		sr.loadData(m_data, sizeof(T) * m_size);
}

template <class T> void PodArray<T>::save(Stream &sr) const {
	sr << m_size;
	if(m_data)
		sr.saveData(m_data, sizeof(T) * m_size);
}

template <class T> void PodArray<T>::resize(int new_size) {
	DASSERT(new_size >= 0);
	if(m_size == new_size)
		return;

	clear();
	m_size = new_size;
	if(new_size)
		m_data = (T *)malloc(new_size * sizeof(T));
}

class BitVector {
  public:
	typedef u32 base_type;
	enum {
		base_shift = 5,
		base_size = 32,
	};

	struct Bit {
		Bit(base_type &base, int bit_index) : base(base), bit_index(bit_index) {}
		operator bool() const { return base & (base_type(1) << bit_index); }

		void operator=(bool value) {
			base = (base & ~(base_type(1) << bit_index)) | ((base_type)value << bit_index);
		}

	  protected:
		base_type &base;
		int bit_index;
	};

	explicit BitVector(int size = 0);
	void resize(int new_size, bool clear_value = false);

	int size() const { return m_size; }
	int baseSize() const { return m_data.size(); }

	void clear(bool value);

	const PodArray<base_type> &data() const { return m_data; }
	PodArray<base_type> &data() { return m_data; }

	bool operator[](int idx) const {
		return m_data[idx >> base_shift] & (1 << (idx & (base_size - 1)));
	}

	Bit operator[](int idx) { return Bit(m_data[idx >> base_shift], idx & (base_size - 1)); }

	bool any(int base_idx) const { return m_data[base_idx] != base_type(0); }

	bool all(int base_idx) const { return m_data[base_idx] == ~base_type(0); }

  protected:
	PodArray<base_type> m_data;
	int m_size;
};

class TextFormatter {
  public:
	explicit TextFormatter(int size = 1024);

	void operator()(const char *format, ...);
	const char *text() const { return m_data.data(); }
	int length() const { return m_offset; }

  private:
	int m_offset;
	PodArray<char> m_data;
};

string format(const char *format, ...);

struct ListNode {
	ListNode() : next(-1), prev(-1) {}

	int next, prev;
};

struct List {
	List() : head(-1), tail(-1) {}
	bool isEmpty() const { return head == -1; }

	int head, tail;
};

// TODO: add functions to remove head / tail

template <class Object, ListNode Object::*member, class Container>
void listInsert(Container &container, List &list, int idx) __attribute__((noinline));

template <class Object, ListNode Object::*member, class Container>
void listRemove(Container &container, List &list, int idx) __attribute__((noinline));

template <class Object, ListNode Object::*member, class Container>
int freeListAlloc(Container &container, List &free_list) __attribute__((noinline));

// Assumes that node is disconnected
template <class Object, ListNode Object::*member, class Container>
void listInsert(Container &container, List &list, int idx) {
	DASSERT(idx >= 0 && idx < (int)container.size());
	ListNode &node = container[idx].*member;
	DASSERT(node.prev == -1 && node.next == -1);

	node.next = list.head;
	if(list.head == -1)
		list.tail = idx;
	else
		(container[list.head].*member).prev = idx;
	list.head = idx;
}

// Assumes that node is on this list
template <class Object, ListNode Object::*member, class Container>
void listRemove(Container &container, List &list, int idx) {
	DASSERT(idx >= 0 && idx < (int)container.size());
	ListNode &node = container[idx].*member;
	int prev = node.prev, next = node.next;

	if(prev == -1) {
		list.head = next;
	} else {
		(container[node.prev].*member).next = next;
		node.prev = -1;
	}

	if(next == -1) {
		list.tail = prev;
	} else {
		(container[next].*member).prev = prev;
		node.next = -1;
	}
}

// Assumes that node is disconnected
template <class Object, ListNode Object::*member, class Container>
int freeListAlloc(Container &container, List &free_list) {
	int idx;

	if(free_list.isEmpty()) {
		container.emplace_back();
		idx = (int)container.size() - 1;
	} else {
		idx = free_list.head;
		listRemove<Object, member>(container, free_list, idx);
	}

	return idx;
}

template <class Object> class LinkedVector {
  public:
	typedef pair<ListNode, Object> Elem;
	LinkedVector() : m_list_size(0) {}

	Object &operator[](int idx) { return m_objects[idx].second; }
	const Object &operator[](int idx) const { return m_objects[idx].second; }
	int size() const { return m_objects.size(); }
	int listSize() const { return m_list_size; }

	int alloc() {
		int idx = freeListAlloc<Elem, &Elem::first>(m_objects, m_free);
		listInsert<Elem, &Elem::first>(m_objects, m_active, idx);
		m_list_size++;
		return idx;
	}

	void free(int idx) {
		DASSERT(idx >= 0 && idx < (int)m_objects.size());
		listRemove<Elem, &Elem::first>(m_objects, m_active, idx);
		listInsert<Elem, &Elem::first>(m_objects, m_free, idx);
		m_list_size--;
	}

	int next(int idx) const { return m_objects[idx].first.next; }
	int prev(int idx) const { return m_objects[idx].first.prev; }

	int head() const { return m_active.head; }
	int tail() const { return m_active.tail; }

  protected:
	vector<Elem> m_objects;
	List m_active, m_free;
	int m_list_size;
};

// TODO: verify that it works on windows when working on different drives
class FilePath {
  public:
	FilePath(const char *);
	FilePath(const string &);
	FilePath(FilePath &&);
	FilePath(const FilePath &);
	FilePath();

	void operator=(const FilePath &rhs) { m_path = rhs.m_path; }
	void operator=(FilePath &&rhs) { m_path = std::move(rhs.m_path); }

	bool isRoot() const;
	bool isAbsolute() const;
	bool isEmpty() const { return m_path.empty(); }

	string fileName() const;
	bool isDirectory() const;
	bool isRegularFile() const;

	FilePath relative(const FilePath &relative_to = current()) const;
	bool isRelative(const FilePath &ancestor) const;

	FilePath absolute() const;
	FilePath parent() const;

	FilePath operator/(const FilePath &other) const;
	FilePath &operator/=(const FilePath &other);

	static FilePath current();

	operator const string &() const { return m_path; }
	const char *c_str() const { return m_path.c_str(); }
	int size() const { return (int)m_path.size(); }

	bool operator<(const FilePath &rhs) const { return m_path < rhs.m_path; }

  private:
	struct Element {
		bool isDot() const;
		bool isDots() const;
		bool isRoot() const;
		bool operator==(const Element &rhs) const;

		const char *ptr;
		int size;
	};

	static Element extractRoot(const char *);
	static void divide(const char *, vector<Element> &);
	static void simplify(const vector<Element> &src, vector<Element> &dst);
	void construct(const vector<Element> &);

	string m_path; // its always non-empty
};

inline bool operator==(const FilePath &lhs, const FilePath &rhs) {
	return (const string &)lhs == (const string &)rhs;
}

struct FileEntry {
	FilePath path;
	bool is_dir;

	bool operator<(const FileEntry &rhs) const {
		return is_dir == rhs.is_dir ? path < rhs.path : is_dir > rhs.is_dir;
	}
};

namespace FindFiles {
	enum Flags {
		regular_file = 1,
		directory = 2,

		recursive = 4,

		relative = 8,		 // all paths relative to given path
		absolute = 16,		 // all paths absolute
		include_parent = 32, // include '..'
	};
};

void findFiles(vector<FileEntry> &out, const FilePath &path, int flags = FindFiles::regular_file);
bool removeSuffix(string &str, const string &suffix);
bool removePrefix(string &str, const string &prefix);
string toLower(const string &str);
void mkdirRecursive(const FilePath &path);
bool access(const FilePath &path);
}

#endif
