/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk.*/

#include "fwk_cache.h"

namespace fwk {

CacheBase::CacheBase() { CacheRegistry::registerCache(this); }
CacheBase::~CacheBase() { /*CacheRegistry::unregisterCache(this);*/ }

namespace {
	std::mutex s_mutex;
	vector<CacheBase *> s_caches;
}

void CacheRegistry::clearInvalid() {
	std::lock_guard<std::mutex> lock(s_mutex);
	for(auto *cache : s_caches)
		cache->clearInvalid();
}

void CacheRegistry::registerCache(CacheBase *cache) {
	std::lock_guard<std::mutex> lock(s_mutex);
	s_caches.emplace_back(cache);
}

void CacheRegistry::unregisterCache(CacheBase *cache) {
	std::lock_guard<std::mutex> lock(s_mutex);
	s_caches.erase(std::find(begin(s_caches), end(s_caches), cache));
}
}
