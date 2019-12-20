// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/expected.h"

#include "fwk/sys/error.h"
#include "fwk/sys/exception.h"
#include "fwk/sys/on_fail.h"

namespace fwk {
namespace detail {
	Error expectMakeError(const char *expr, const char *file, int line) {
		auto chunks = onFailChunks();
		chunks.emplace_back(ErrorLoc{file, line}, format("Failed: %", expr));
		return chunks;
	}

	void expectFromExceptions(Dynamic<Error> *out) {
		new(out) Dynamic<Error>(getMergedExceptions());
	}

	void expectMergeExceptions(Error &out) {
		auto errors = getExceptions();
		errors.emplace_back(move(out));
		out = Error::merge(errors);
	}

}
}
