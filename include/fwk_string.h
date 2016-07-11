#pragma once

#include <functional>
#include <string>

// baselib kompiluje sie ok 10% szybciej z tym stringiem

#ifdef FWK_USE_STD_STRING

namespace fwk {
using std::string;
using std::wstring;
}

#else

namespace fwk {

// TODO: better name
template <class Type, int bytes> class FlatStorage { public: };

template <class T> class base_string {
  public:
	template <typename _InIter>
	using RequireInputIter = typename std::enable_if<
		std::is_convertible<typename std::iterator_traits<_InIter>::iterator_category,
							std::input_iterator_tag>::value>::type;
	base_string(std::string);
	base_string(std::wstring);
	base_string();
	base_string(const T *);
	base_string(const T *, const T *);

	template <class Iter, typename = RequireInputIter<Iter>> base_string(Iter first, Iter last) {}

	base_string(const T *, int);
	base_string(const base_string &);
	base_string(base_string &&);
	base_string(int, T);

	operator std::string() const;

	void operator=(const base_string &);
	void operator=(base_string &&);

	T *begin() { return const_cast<T *>(const_cast<const base_string *>(this)->begin()); }
	T *end() { return const_cast<T *>(const_cast<const base_string *>(this)->end()); }

	const T *begin() const { return isSmall() ? m_small_buffer : m_big_buffer; }
	const T *end() const {
		return isSmall() ? m_small_buffer + m_small_size : m_big_buffer + m_big_size;
	}

	enum { npos = 2147483647 };

	int size() const { return isSmall() ? m_small_size : m_big_size; }
	int length() const { return size(); }
	int max_size() const { return npos; }

	void resize(int new_size);
	void resize(int, T);

	int capacity() const { return size(); }

	void reserve(int new_size);
	void clear();
	bool empty() const { return m_small_size == 0; }
	void shrink_too_fit();

	T &operator[](int idx) { return at(idx); }
	const T &operator[](int idx) const { return at(idx); }

	const T *c_str() const { return begin(); }
	const T *data() const { return begin(); }
	T *data() { return begin(); }

	T &at(int idx) { return begin()[idx]; }
	const T &at(int idx) const { return begin()[idx]; }

	T &back() { return end()[-1]; }
	const T &back() const { return end()[-1]; }

	T &front() { return begin()[0]; }
	const T &front() const { return begin()[0]; }

	void operator+=(const base_string &);
	base_string operator+(const base_string &) const;
	void append(const base_string &);

	void push_back(T);
	void emplace_back(T t) { return push_back(t); }

	void assign(const base_string &);
	base_string substr(int pos, int len = npos) const;

	int compare(const base_string &rhs) const;
	bool operator<(const base_string &rhs) const { return compare(rhs) < 0; }

	int find(T) const;
	int find(const char *) const;

	int rfind(T) const;
	int rfind(const base_string &) const;

	void insert(int pos, T);
	void insert(int pos, int count, T);
	void erase(int pos, int count);

	void swap(base_string &);
	T pop_back();

  private:
	bool isSmall() const { return m_small_size < base_size; }

	enum { base_size = sizeof(T) == 1 ? 15 : 7 };

	union {
		T m_small_buffer[base_size];
		struct {
			T *m_big_buffer;
			int m_big_size;
		};
	};
	char m_small_size;
};

template <class T> auto operator+(const T *text, const base_string<T> &str) {
	return base_string<T>(text) + str;
}

using string = base_string<char>;
using wstring = base_string<wchar_t>;
}

namespace std {

template <class T> struct hash<fwk::base_string<T>> {
	size_t operator()(const fwk::base_string<T> &str) const { return 0; }
};
}

#endif
