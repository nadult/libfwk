// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/logger.h"
#include "fwk/format.h"
#include "fwk/hash_map.h"
#include "fwk/sys/thread.h"

namespace fwk {

Logger::Logger() = default;
FWK_COPYABLE_CLASS_IMPL(Logger);

bool Logger::keyPresent(Str key) const {
	if(key) {
		auto it = m_keys.find(key);
		return it != m_keys.end();
	}
	return false;
}

void Logger::addMessage(Str text, Str unique_key) {
	if(unique_key) {
		auto it = m_keys.find(unique_key);
		if(it != m_keys.end())
			return;
		m_keys[unique_key] = 1;
	}

	print("%\n", text);
}

static Logger s_logger;
static Mutex s_mutex;

void log(Str message, Str unique_key) {
	MutexLocker lock(s_mutex);
	s_logger.addMessage(message, unique_key);
}

void log(Str message) { log(message, ""); }

bool logKeyPresent(Str key) {
	MutexLocker lock(s_mutex);
	return s_logger.keyPresent(key);
}
}
