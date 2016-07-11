#include "fwk_base.h"

#include <cwchar>

// Kompilacja frontier:
// std::vector + std::string: 131 sec
// fwk::vector + fwk::string:  96 sec
// fwk::vector + std::string: 103 sec
// std::vector + fwk::string: 127 sec

#ifndef FWK_USE_STD_STRING

namespace fwk {
template <class T> base_string<T>::base_string(std::string) {}
template <class T> base_string<T>::base_string(std::wstring) {}
template <class T> base_string<T>::base_string() {}
template <class T> base_string<T>::base_string(const T *) {}
template <class T> base_string<T>::base_string(const T *, const T *) {}
template <class T> base_string<T>::base_string(const T *src, int size) {
	DASSERT(src);
	if(size < base_size) {
		m_small_size = size;
		memcpy(m_small_buffer, src, size);
		m_small_buffer[size] = 0;
	} else {
		m_small_size = 255;
		m_big_size = size;
	}
}
template <class T> base_string<T>::base_string(const base_string &) {}
template <class T> base_string<T>::base_string(base_string &&) {}
template <class T> base_string<T>::base_string(int, T) {}

template <class T> base_string<T>::operator std::string() const { return {}; }

template <class T> void base_string<T>::operator=(const base_string &) {}
template <class T> void base_string<T>::operator=(base_string &&) {}

template <class T> void base_string<T>::resize(int new_size) {}
template <class T> void base_string<T>::resize(int, T) {}

template <class T> void base_string<T>::reserve(int new_size) {}
template <class T> void base_string<T>::clear() {}
template <class T> void base_string<T>::shrink_too_fit() {}

template <class T> void base_string<T>::operator+=(const base_string &) {}
template <class T> base_string<T> base_string<T>::operator+(const base_string &) const {
	return {};
}
template <class T> void base_string<T>::append(const base_string &) {}

template <class T> void base_string<T>::push_back(T) {}

template <class T> void base_string<T>::assign(const base_string &) {}
template <class T> base_string<T> base_string<T>::substr(int pos, int len) const { return {}; }

template <class T> int base_string<T>::compare(const base_string &rhs) const { return 0; }

template <class T> int base_string<T>::find(T) const { return npos; }
template <class T> int base_string<T>::find(const char *) const { return npos; }

template <class T> int base_string<T>::rfind(T) const { return npos; }
template <class T> int base_string<T>::rfind(const base_string &) const { return npos; }

template <class T> void base_string<T>::swap(base_string &) {}

template <class T> T base_string<T>::pop_back() { return T(); }
template <class T> void base_string<T>::insert(int pos, T) {}
template <class T> void base_string<T>::insert(int pos, int count, T) {}
template <class T> void base_string<T>::erase(int pos, int count) {}
template class base_string<char>;
template class base_string<wchar_t>;
}

#endif
