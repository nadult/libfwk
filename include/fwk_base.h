/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef FWK_BASE_H
#define FWK_BASE_H

#include <vector>
#include <array>
#include <string>
#include <exception>
#include <type_traits>
#include <memory>
#include <map>
#include <cstring>
#include <cstdlib>
#include <algorithm>

namespace fwk {

using std::array;
using std::swap;
using std::pair;
using std::string;
using std::vector;
using std::unique_ptr;
using std::shared_ptr;

using std::begin;
using std::end;
using std::make_pair;
using std::make_shared;
using std::make_unique;

// TODO: use types from cstdint
using uint = unsigned int;
using u8 = unsigned char;
using i8 = char;
using u16 = unsigned short;
using i16 = short;
using u32 = unsigned;
using i32 = int;

// TODO: write more of these
template <class Range, class Functor> bool anyOf(const Range &range, Functor functor) {
	return std::any_of(begin(range), end(range), functor);
}

template <class Range, class Functor> bool allOf(const Range &range, Functor functor) {
	return std::all_of(begin(range), end(range), functor);
}

string execCommand(const string &cmd);

// Compile your program with -rdynamic to get some interesting info
// Currently not avaliable on mingw32 platform
// TODO: use lib-lldb
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

#ifdef __clang__
__attribute__((__format__(__printf__, 3, 4)))
#endif
void throwException(const char *file, int line, const char *fmt, ...);
void doAssert(const char *file, int line, const char *str);
void handleCtrlC(void (*handler)());
void handleSegFault();
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
template <class T> inline T min(T a, T b) { return b < a ? b : a; }

template <class T1, class T2> bool operator!=(const T1 &a, const T2 &b) { return !(a == b); }

template <class T, int size> constexpr int arraySize(T(&)[size]) noexcept { return size; }

void logError(const string &error);

template <class T> class Range;

template <class T> class RangeIterator : public std::iterator<std::random_access_iterator_tag, T> {
  protected:
#ifdef NDEBUG
	constexpr RangeIterator(T *pointer, int, int) noexcept : m_pointer(pointer) {}
#else
	RangeIterator(T *pointer, int left, int right) noexcept : m_pointer(pointer),
															  m_left(left),
															  m_right(right) {
		DASSERT(m_pointer);
		DASSERT(left >= 0 && right >= 0);
	}
#endif
	friend class Range<T>;

  public:
	constexpr auto operator+(int offset) const noexcept {
#ifdef NDEBUG
		return RangeIterator(m_pointer + offset, 0, 0);
#else
		return RangeIterator(m_pointer + offset, m_left + offset, m_right + offset);
#endif
	}
	constexpr auto operator-(int offset) const noexcept { return operator+(-offset); }

	constexpr int operator-(const RangeIterator &rhs) const noexcept {
#ifndef NDEBUG
		DASSERT(m_pointer >= rhs.m_pointer ? m_pointer - rhs.m_pointer <= m_left
										   : rhs.m_pointer - m_pointer <= m_right);
#endif
		return m_pointer - rhs.m_pointer;
	}
	constexpr T &operator*() const noexcept {
		DASSERT(m_right >= 1);
		return *m_pointer;
	}
	constexpr bool operator==(const RangeIterator &rhs) const noexcept {
		return m_pointer == rhs.m_pointer;
	}
	constexpr bool operator<(const RangeIterator &rhs) const noexcept {
		return m_pointer < rhs.m_pointer;
	}
	RangeIterator &operator++() {
		m_pointer++;
#ifndef NDEBUG
		DASSERT(m_right > 0);
		m_left++;
		m_right--;
#endif
		return *this;
	}

  private:
	T *m_pointer;
#ifndef NDEBUG
	int m_left, m_right;
#endif
};

template <class T> class PodArray;

// Simple wrapper class for ranges of values
// The user is responsible for making sure, that the values
// exist while Range pointing to them is in use
// TODO: add possiblity to reinterpret Range of uchars into range of chars, etc.
template <class T> class Range {
  public:
	Range(std::vector<T> &data) : Range(data.data(), data.size()) {}
	template <class TConvertible>
	constexpr Range(const std::vector<TConvertible> &vec) noexcept : Range(vec.data(), vec.size()) {
	}
	template <class TConvertible>
	constexpr Range(std::vector<TConvertible> &vec) noexcept : Range(vec.data(), vec.size()) {}
	template <class TConvertible>
	constexpr Range(const PodArray<TConvertible> &array) noexcept
		: Range(array.data(), array.size()) {}
	template <class TConvertible>
	constexpr Range(PodArray<TConvertible> &array) noexcept : Range(array.data(), array.size()) {}
	template <class TConvertible, int N>
	constexpr Range(TConvertible(&array)[N]) noexcept : Range(array, N) {}
	template <class TConvertible>
	constexpr Range(TConvertible *begin, TConvertible *end) noexcept
		: Range(begin, (int)(end - begin)) {}
	template <class TConvertible>
	Range(TConvertible *data, int size) noexcept : m_data(data), m_size(size) {
		DASSERT(m_data || m_size == 0);
		DASSERT(m_size >= 0);
	}
	Range(T *data, int size) noexcept : m_data(data), m_size(size) {
		DASSERT(m_data || m_size == 0);
		DASSERT(m_size >= 0);
	}
	operator Range<const T>() const { return Range<const T>(m_data, m_size); }

	constexpr auto cbegin() const noexcept { return RangeIterator<const T>(m_data, 0, m_size); }
	constexpr auto cend() const noexcept {
		return RangeIterator<const T>(m_data + m_size, m_size, 0);
	}
	auto begin() const noexcept { return cbegin(); }
	auto end() const noexcept { return cend(); }
	auto begin() noexcept { return RangeIterator<T>(m_data, 0, m_size); }
	auto end() noexcept { return RangeIterator<T>(m_data + m_size, m_size, 0); }

	const T *data() const noexcept { return m_data; }
	T *data() noexcept { return m_data; }
	constexpr int size() const noexcept { return m_size; }
	constexpr bool empty() const noexcept { return m_size == 0; }

	const T &operator[](int idx) const noexcept {
		DASSERT(idx >= 0 && idx < m_size);
		return m_data[idx];
	}
	T &operator[](int idx) noexcept {
		DASSERT(idx >= 0 && idx < m_size);
		return m_data[idx];
	}

  private:
	T *m_data;
	int m_size;
};

// TODO: better name
// TODO: Minimum size is a bit misleading, user could expect exact size
template <class T, int MinSize> class TRange : public Range<T> {
  public:
	template <class... Args> TRange(Args &&... args) : Range<T>(std::forward<Args>(args)...) {
		DASSERT(Range<T>::size() >= MinSize);
	}
	template <class TConvertible, int N>
	constexpr TRange(TConvertible(&array)[N]) noexcept : Range<T>(array, N) {
		static_assert(N >= MinSize, "Array not big enough");
	}
};

template <class T> using CRangeIterator = RangeIterator<const T>;
template <class T> using CRange = Range<const T>;

template <class T> auto makeRange(const std::vector<T> &vec) noexcept {
	return Range<const T>(vec.data(), vec.size());
}

template <class T> auto makeRange(std::vector<T> &vec) noexcept {
	return Range<T>(vec.data(), vec.size());
}

template <class T> auto makeRange(const PodArray<T> &vec) noexcept {
	return Range<const T>(vec.data(), vec.size());
}

template <class T> auto makeRange(PodArray<T> &vec) noexcept {
	return Range<T>(vec.data(), vec.size());
}

template <class T, int N> auto makeRange(T(&array)[N]) noexcept { return TRange<T, N>(array); }

template <class T> Range<typename T::value_type> makeRange(T *begin, T *end) noexcept {
	return Range<typename T::value_type>(begin, (int)(end - begin));
}

template <class T> auto makeRange(T *data, int size) noexcept { return Range<T>(data, size); }

template <class A, class B> class Indexer;

template <class Target, class Indices>
class IndexIterator
	: public std::iterator<std::random_access_iterator_tag, typename Target::value_type> {
	using Index = typename Indices::const_iterator;

  protected:
	constexpr IndexIterator(Index idx, Target &target) noexcept : m_idx(idx), m_target(target) {}
	friend class Indexer<Target, Indices>;

  public:
	constexpr auto operator+(int offset) const noexcept {
		return IndexIterator(m_idx + offset, m_target);
	}
	constexpr auto operator-(int offset) const noexcept { return operator+(-offset); }

	constexpr int operator-(const IndexIterator &rhs) const noexcept { return m_idx - rhs.m_idx; }
	constexpr auto &operator*() const noexcept { return m_target[*m_idx]; }
	constexpr bool operator==(const IndexIterator &rhs) const noexcept {
		return m_idx == rhs.m_idx;
	}
	constexpr bool operator<(const IndexIterator &rhs) const noexcept { return m_idx < rhs.m_idx; }
	IndexIterator &operator++() {
		m_idx++;
		return *this;
	}

  private:
	Index m_idx;
	Target &m_target;
};
template <class Target, class Indices> class Indexer {
  public:
	Indexer(Target &target, const Indices &indices) : m_target(target), m_indices(indices) {}

	int size() const { return m_indices.size(); }
	auto begin() const { return IndexIterator<Target, Indices>(std::begin(m_indices), m_target); }
	auto end() const { return IndexIterator<Target, Indices>(std::end(m_indices), m_target); }

  private:
	Target &m_target;
	const Indices &m_indices;
};

template <class Target, class Indices>
auto indexWith(const Target &target, const Indices &indices) {
	return Indexer<const Target, Indices>(target, indices);
}

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

int enumFromString(const char *str, const char **enum_strings, int enum_strings_count,
				   bool handle_invalid);

// See test/enums.cpp for example usage
#define DECLARE_ENUM(type, ...)                                                                    \
	namespace type {                                                                               \
		enum Type : char { invalid = -1, __VA_ARGS__, count };                                     \
		const char *toString(int, const char *on_invalid = nullptr);                               \
		Type fromString(const char *, bool handle_invalid = false);                                \
		inline constexpr bool isValid(int val) { return val >= 0 && val < count; }                 \
	}

#define DEFINE_ENUM(type, ...)                                                                     \
	namespace type {                                                                               \
		static const char *s_strings[] = {__VA_ARGS__};                                            \
		static_assert(arraySize(s_strings) == count, "String count does not match enum count");    \
		const char *toString(int value, const char *on_invalid) {                                  \
			return !isValid(value) ? on_invalid : s_strings[value];                                \
		}                                                                                          \
		Type fromString(const char *str, bool handle_invalid) {                                    \
			return (Type)fwk::enumFromString(str, s_strings, count, handle_invalid);               \
		}                                                                                          \
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
		static void doLoad(T(&obj)[tsize], Stream &sr) {
			for(size_t n = 0; n < tsize; n++)
				obj[n].load(sr);
		};
		static void doSave(const T(&obj)[tsize], Stream &sr) {
			for(size_t n = 0; n < tsize; n++)
				obj[n].save(sr);
		};
	};

	template <class T, int tsize> struct Serialization<T[tsize], true> {
		static void doLoad(T(&obj)[tsize], Stream &sr) { sr.loadData(&obj[0], sizeof(T) * tsize); }
		static void doSave(const T(&obj)[tsize], Stream &sr) {
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

template <class T> class ResourceLoader {
  public:
	ResourceLoader(const string &file_prefix, const string &file_suffix)
		: m_file_prefix(file_prefix), m_file_suffix(file_suffix) {}

	shared_ptr<T> operator()(const string &name) const {
		Loader stream(fileName(name));
		return make_shared<T>(name, stream);
	}

	string fileName(const string &name) const { return m_file_prefix + name + m_file_suffix; }
	const string &filePrefix() const { return m_file_prefix; }
	const string &fileSuffix() const { return m_file_suffix; }

  protected:
	string m_file_prefix, m_file_suffix;
};

template <class T, class Constructor = ResourceLoader<T>> class ResourceManager {
  public:
	using PResource = shared_ptr<T>;

	template <class... ConstructorArgs>
	ResourceManager(ConstructorArgs &&... args)
		: m_constructor(std::forward<ConstructorArgs>(args)...) {}
	ResourceManager() = default;
	~ResourceManager() {}

	const Constructor &constructor() const { return m_constructor; }

	PResource accessResource(const string &name) {
		auto it = m_dict.find(name);
		if(it == m_dict.end()) {
			PResource res = m_constructor(name);
			DASSERT(res);
			m_dict[name] = res;
			return res;
		}
		return it->second;
	}

	PResource findResource(const string &name) const {
		auto it = m_dict.find(name);
		if(it == m_dict.end())
			return PResource();
		return it->second;
	}

	PResource operator[](const string &name) { return accessResource(name); }

	const auto &dict() const { return m_dict; }

	PResource removeResource(const string &name) {
		auto iter = m_dict.find(name);
		if(iter != m_dict.end()) {
			m_dict.erase(iter);
			return iter.second;
		}
		return PResource();
	}

	void insertResource(const string &name, PResource res) { m_dict[name] = res; }

	void renameResource(const string &old_name, const string &new_name) {
		insertResource(new_name, std::move(removeResource(old_name)));
	}

	using Iterator = typename std::map<string, PResource>::const_iterator;

	auto begin() const { return std::begin(m_dict); }
	auto end() const { return std::end(m_dict); }

	void clear() { m_dict.clear(); }

  private:
	typename std::map<string, PResource> m_dict;
	Constructor m_constructor;
	string m_prefix, m_suffix;
};

template <class T> class ClonablePtr : public unique_ptr<T> {
  public:
	static_assert(std::is_same<decltype(&T::clone), T *(T::*)() const>::value, "");

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
	// You can specify initial buffer size. Don't worry though, buffer
	// will be resized if needed
	explicit TextFormatter(int size = 256);

#ifdef __clang__
	__attribute__((__format__(__printf__, 2, 3)))
#endif
	void
	operator()(const char *format, ...);

	const char *text() const { return m_data.data(); }
	int length() const { return m_offset; }

  private:
	int m_offset;
	PodArray<char> m_data;
};

// Parsing white-space separated elements
class TextParser {
  public:
	TextParser(const char *input) : m_current(input) { DASSERT(m_current); }

	bool parseBool();
	int parseInt();
	float parseFloat();
	uint parseUint();
	string parseString();

	void parseInts(Range<int> out);
	void parseFloats(Range<float> out);
	void parseUints(Range<uint> out);
	void parseStrings(Range<string> out);

	bool hasAnythingLeft();
	int countElements() const;
	bool isFinished() const { return !*m_current; }

  private:
	const char *m_current;
};

#ifdef __clang__
__attribute__((__format__(__printf__, 1, 2)))
#endif
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
	string fileExtension() const;
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

vector<FileEntry> findFiles(const FilePath &path, int flags = FindFiles::regular_file);
bool removeSuffix(string &str, const string &suffix);
bool removePrefix(string &str, const string &prefix);
string toLower(const string &str);
void mkdirRecursive(const FilePath &path);
bool access(const FilePath &path);
}

#endif
