// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

// Light abstraction over pthreads. Allows compiling code on
// platforms with no thread support (like html). This still requires
// handling such a case in most multi-threaded programs, but greatly
// decreases amount of #ifdefs.
//
// Mutex, Condition, Thread simply do nothing when threads are disabled!

#ifdef FWK_PLATFORM_HTML
#define FWK_THREADS_DISABLED
#endif

#ifndef FWK_THREADS_DISABLED
#include <pthread.h>
#endif

namespace fwk {

struct Mutex {
	Mutex() = default;
	Mutex(const Mutex &) = delete;
	void operator=(const Mutex &) = delete;

#ifndef FWK_THREADS_DISABLED
	~Mutex() { pthread_mutex_destroy(&mutex); }
	void init() { pthread_mutex_init(&mutex, nullptr); }
	void lock() { pthread_mutex_lock(&mutex); }
	void unlock() { pthread_mutex_unlock(&mutex); }

	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
#else
	void lock() {}
	void unlock() {}
	void init() {}

	int mutex = 0;
#endif
};

struct Condition {
	Condition() = default;
	Condition(const Condition &) = delete;
	void operator=(const Condition &) = delete;

#ifndef FWK_THREADS_DISABLED
	~Condition() { pthread_cond_destroy(&cond); }
	void signal() { pthread_cond_signal(&cond); }
	void wait(Mutex &mutex) { pthread_cond_wait(&cond, &mutex.mutex); }

	pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
#else
	void signal() {}
	void wait(Mutex &) {}

	int cond = 0;
#endif
};

struct Thread {
	Thread(const Thread &) = delete;
	void operator=(const Thread &) = delete;
	~Thread() = default; // Don't forget to join!

	static int hardwareConcurrency();

#ifndef FWK_THREADS_DISABLED
	Thread(void *(*func)(void *), void *arg) { pthread_create(&thread, nullptr, func, arg); }
	void join() { pthread_join(thread, 0); }

	pthread_t thread;
#else
	Thread(void *(*func)(void *), void *) {}
	void join() {}

	int thread = 0;
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
