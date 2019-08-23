// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/vector.h"

namespace fwk {

template <class T> constexpr int smallVectorSize(int byte_size) {
	int spare_size = sizeof(vector<T>) - sizeof(uint);
	int target_size = spare_size > byte_size ? spare_size : byte_size;
	int header_size = alignof(T) > 4 ? alignof(T) : 4;
	return (target_size - header_size) / sizeof(T);
};

// template <class T, int base_size> using SmallVector = vector<T>;

// DEPRECATED: SSBO for vectors is a bad idea, use pooling, local buffers and other methods
//
// TODO: decrease code bloat by writing some generic templated functions
// But it wouln't be possible to move their implementation to cpp file...
template <class T, int base_size_ = smallVectorSize<T>(64)> class SmallVector {
  public:
	static constexpr int spare_size = sizeof(vector<T>) - sizeof(uint);
	static constexpr int base_size =
		base_size_ * sizeof(T) < spare_size ? spare_size / sizeof(T) : base_size_;
	static constexpr unsigned small_bit = 0x80000000;
	using value_type = T;

#define SMALL ((T *)(m_small))
#define RHS_SMALL ((T *)(rhs.m_small))

	SmallVector() : m_size(small_bit) {}
	~SmallVector() {
		if(isSmall()) {
			for(auto &elem : *this)
				elem.~T();
		} else {
			destruct(m_big);
		}
	}
	SmallVector(const SmallVector &rhs) {
		if(rhs.isSmall()) {
			m_size = rhs.m_size;
			for(int n = 0, size = this->size(); n < size; n++)
				new(&SMALL[n]) T(RHS_SMALL[n]);
		} else {
			new(&m_big) vector<T>(rhs.m_big);
		}
	}
	SmallVector(CSpan<T> span) {
		uint size = span.size();
		if(size <= base_size) {
			m_size = size | small_bit;
			for(uint n = 0; n < size; n++)
				new(&SMALL[n]) T(span[n]);
		} else {
			new(&m_big) vector<T>(span);
		}
	}

	SmallVector(SmallVector &&rhs) {
		if(rhs.isSmall()) {
			m_size = rhs.m_size;
			for(int n = 0, size = this->size(); n < size; n++) {
				new(&SMALL[n]) T(move(RHS_SMALL[n]));
				RHS_SMALL[n].~T();
			}
		} else {
			new(&m_big) vector<T>(move(rhs.m_big));
			destruct(rhs.m_big);
		}
		rhs.m_size = small_bit;
	}
	operator CSpan<T>() const { return {data(), size()}; }
	template <class URange, class U = RangeBase<URange>, EnableIf<is_convertible<U, T>>...>
	SmallVector(const URange &range) : SmallVector() {
		for(auto &elem : range)
			emplace_back(elem);
	}

	void swap(SmallVector &rhs) {
		SmallVector temp = move(*this);
		*this = move(rhs);
		rhs = move(temp);
	}

	void operator=(SmallVector &&rhs) {
		if(&rhs != this) {
			this->~SmallVector();
			new(this) SmallVector(move(rhs));
		}
	}

	void operator=(const SmallVector &rhs) {
		SmallVector copy(rhs);
		swap(copy);
	}

	template <class... Args> void emplace_back(Args &&... args) {
		if(isSmall()) {
			int size = this->size();
			if(size < base_size) {
				new(&SMALL[size]) T{std::forward<Args>(args)...};
				m_size++;
				return;
			}
			makeBig();
		}

		m_big.emplace_back(std::forward<Args>(args)...);
	}

	void pop_back() {
		PASSERT(!empty());
		if(isSmall()) {
			m_size--;
			SMALL[size()].~T();
		} else {
			m_big.pop_back();
		}
	}

	bool inRange(int index) const { return index >= 0 && index < size(); }
	bool empty() const { return size() == 0; }
	explicit operator bool() const { return !empty(); }

	void clear() {
		if(isSmall())
			for(auto &elem : *this)
				elem.~T();
		else
			destruct(m_big);
		m_size = small_bit;
	}

	T &operator[](int index) {
		IF_PARANOID(checkInRange(index, 0, size()));
		return isSmall() ? SMALL[index] : m_big[index];
	}
	const T &operator[](int index) const {
		IF_PARANOID(checkInRange(index, 0, size()));
		return isSmall() ? SMALL[index] : m_big[index];
	}

	const T &back() const { return (*this)[size() - 1]; }
	T &back() { return (*this)[size() - 1]; }
	const T &front() const { return (*this)[0]; }
	T &front() { return (*this)[0]; }

	T *data() { return begin(); }
	T *begin() { return isSmall() ? SMALL : m_big.begin(); }
	T *end() { return isSmall() ? SMALL + size() : m_big.end(); }

	const T *data() const { return begin(); }
	const T *begin() const { return isSmall() ? SMALL : m_big.begin(); }
	const T *end() const { return isSmall() ? SMALL + size() : m_big.end(); }

	int size() const { return (int)(m_size & ~small_bit); }
	bool isSmall() const { return m_size & small_bit; }
	i64 usedMemory() const { return isSmall() ? 0 : m_big.capacity() * sizeof(T); }

	int capacity() const { return isSmall() ? base_size : m_big.capacity(); }
	void reserve(int capacity) {
		if(isSmall()) {
			if(capacity <= base_size)
				return;
			return makeBig(capacity);
		}

		m_big.reserve(capacity);
	}

	void shrink(int new_size) {
		PASSERT(new_size >= 0 && new_size <= size());
		if(isSmall()) {
			while(size() > new_size) {
				m_size--;
				SMALL[size()].~T();
			}
		} else {
			while(m_big.size() > new_size)
				m_big.pop_back();
		}
	}

	void resize(int new_size) {
		if(isSmall()) {
			int index = size();
			if(new_size > base_size) {
				makeBig(new_size);

				// TODO: this is required on GCC9; Proper solution would require reimplementing
				// vector & base vector here, or paying the cost of additional size...
				m_size = new_size;
			} else {
				while(index > new_size)
					SMALL[--index].~T();
				m_size = new_size | small_bit;
			}
			while(index < new_size)
				new(data() + index++) T();
		} else {
			m_big.resize(new_size);
		}
	}

	bool operator==(const SmallVector &rhs) const {
		return size() == rhs.size() && std::equal(begin(), end(), rhs.begin(), rhs.end());
	}

	bool operator<(const SmallVector &rhs) const {
		return std::lexicographical_compare(begin(), end(), rhs.begin(), rhs.end());
	}

  private:
	template <class TT> static void destruct(TT &ref) { ref.~TT(); }

	// TODO: makeBig funcs can be more efficient (moving + resizing to proper size)
	void makeBig(int capacity) {
		vector<T> big;
		big.reserve(capacity);
		big.insert(big.begin(), begin(), end());
		for(auto &elem : *this)
			elem.~T();
		new(&m_big) vector<T>(move(big));
		PASSERT(!isSmall());
	}

	void makeBig() {
		vector<T> big(begin(), end());
		for(auto &elem : *this)
			elem.~T();
		new(&m_big) vector<T>(move(big));
		PASSERT(!isSmall());
	}

#undef SMALL
#undef RHS_SMALL

#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#error "TODO: add big endian support"
#endif

	union {
		struct {
			uint m_size;
			std::aligned_storage_t<sizeof(T), alignof(T)> m_small[base_size];
		};
		struct {
			vector<T> m_big;
		};
	};
};

}
