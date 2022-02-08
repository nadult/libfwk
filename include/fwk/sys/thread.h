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
#include <condition_variable>
#include <mutex>
#include <thread>
#endif

namespace fwk {

struct Mutex {
	Mutex() = default;
	Mutex(const Mutex &) = delete;
	void operator=(const Mutex &) = delete;

#ifndef FWK_THREADS_DISABLED
	void lock() { mutex.lock(); }
	void unlock() { mutex.unlock(); }

	std::mutex mutex;
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
	void signalOne() { cond.notify_one(); }
	void signalAll() { cond.notify_all(); }
	void wait(Mutex &mutex) {
		std::unique_lock<std::mutex> lock(mutex.mutex);
		cond.wait(lock);
	}

	std::condition_variable cond;
#else
	void signalOne() {}
	void signalAll() {}
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
	Thread(void *(*func)(void *), void *arg) : thread(func, arg) {}
	void join() { thread.join(); }

	std::thread thread;
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
	return (int)std::hash<std::thread::id>{}(std::this_thread::get_id());
#endif
}
}
