/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_base.h"
#include <mutex>

namespace fwk {

template <class Value, class... Keys> class TCache;
class CacheRegistry;

class CacheBase {
  public:
	CacheBase();
	CacheBase(const CacheBase &) = delete;

	virtual ~CacheBase();
	virtual void clearInvalid() = 0;
};

class CacheRegistry {
  public:
	// TODO: memory limit for each cache?
	static void clearInvalid();

  private:
	static void registerCache(CacheBase *);
	static void unregisterCache(CacheBase *); // TODO: fix crash
	friend class CacheBase;
};

template <class Value, class... Keys> class TCache : public CacheBase {
  public:
	template <class T> struct KeyFilter { using type = T; };
	template <class T> struct KeyFilter<immutable_ptr<T>> { using type = immutable_weak_ptr<T>; };

	using Key = std::tuple<typename KeyFilter<Keys>::type...>;

	Key makeKey(Keys... keys) const {
		return std::make_tuple((typename KeyFilter<Keys>::type)(keys)...);
	}

	void add(const Key &key, immutable_ptr<Value> value) {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_map.emplace(key, std::move(value));
	}

	auto access(const Key &key) const {
		std::lock_guard<std::mutex> lock(m_mutex);
		auto it = m_map.find(key);
		if(it != m_map.end())
			return it->second;
		return immutable_ptr<Value>();
	}

	static TCache g_cache;

  private:
	template <class T> static bool isValidElem(const T &elem) { return true; }
	template <class T> static bool isValidElem(const immutable_weak_ptr<T> &elem) {
		return !elem.expired();
	}

	template <std::size_t I = 0, typename... Tp>
	typename std::enable_if<I == sizeof...(Tp), bool>::type isValid(const std::tuple<Tp...> &) {
		return true;
	}

	template <std::size_t I = 0, typename... Tp>
		typename std::enable_if < I<sizeof...(Tp), bool>::type isValid(const std::tuple<Tp...> &t) {
		return isValidElem(std::get<I>(t)) && isValid<I + 1, Tp...>(t);
	}

	void clearInvalid() {
		for(auto it = begin(m_map); it != end(m_map); ++it)
			if(!isValid(it->first))
				m_map.erase(it);
	}

	// TODO: shared_mutex
	mutable std::mutex m_mutex;
	std::map<Key, immutable_ptr<Value>> m_map;
};
}
