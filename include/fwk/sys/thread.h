// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#ifdef FWK_PLATFORM_HTML
#define FWK_THREADS_DISABLED
#endif

#ifndef FWK_THREADS_ENABLED
#include <pthread.h>
#endif

namespace fwk {

struct Mutex {
#ifdef FWK_THREADS_DISABLED
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
	void lock() { pthread_mutex_lock(&mutex); }
	void unlock() { pthread_mutex_unlock(&mutex); }
#else
	void lock() {}
	void unlock() {}
	int mutex = 0;
#endif
};

struct MutexLocker {
	MutexLocker(Mutex &ref) : ref(ref) { ref.lock(); }
	~MutexLocker() { ref.unlock(); }

	MutexLocker(const MutexLocker &) = delete;
	void operator=(const MutexLocker &) = delete;

	Mutex &ref;
};

inline int threadId() {
#ifdef FWK_THREADS_DISABLED
	return 0;
#else
	return (int)pthread_self();
#endif
}
}
