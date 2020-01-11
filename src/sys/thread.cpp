// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/thread.h"

#ifndef FWK_THREADS_DISABLED
#include <thread>
#endif

namespace fwk {
int Thread::hardwareConcurrency() {
#ifdef FWK_THREADS_DISABLED
	return 1;
#else
	return std::thread::hardware_concurrency();
#endif
}
}
