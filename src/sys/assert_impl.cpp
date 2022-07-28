// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/sys/assert_impl.h"

#include "fwk/sys/error.h"
#include "fwk/sys/exception.h"
#include "fwk/sys/on_fail.h"
#include <stdarg.h>

namespace fwk {

namespace detail {

	string AssertInfo::preFormat(TextFormatter &out, const char *prefix) const {
		string temp;

		if(strlen(arg_names) > 0) {
			out("%%\n", prefix, message);
			return detail::autoPrintFormat(arg_names);
		} else {
			out << prefix;
			return message;
		}
	}

	void assertFailed(const AssertInfo *info, ...) {
		TextFormatter out({FormatMode::structured});
		auto fmt = info->preFormat(out, "Assert failed: ");

		va_list ap;
		va_start(ap, info);
		out.append_(fmt.c_str(), info->arg_count, info->funcs, ap);
		va_end(ap);

		Backtrace::t_is_enabled = true;
		onFailMakeError(info->file, info->line, out.text()).print();
		asm("int $3");
		exit(1);
	}

	Error makeError(const AssertInfo *info, ...) {
		TextFormatter out({FormatMode::structured});

		va_list ap;
		va_start(ap, info);
		out.append_(info->message, info->arg_count, info->funcs, ap);
		va_end(ap);

		return onFailMakeError(info->file, info->line, out.text());
	}

	void checkFailed(const AssertInfo *info, ...) {
		TextFormatter out({FormatMode::structured});
		auto fmt = info->preFormat(out, "Check failed: ");

		va_list ap;
		va_start(ap, info);
		out.append_(fmt.c_str(), info->arg_count, info->funcs, ap);
		va_end(ap);

		fwk::raiseException(onFailMakeError(info->file, info->line, out.text()));
	}

	void raiseException(const AssertInfo *info, ...) {
		TextFormatter out({FormatMode::structured});
		auto fmt = info->preFormat(out, "Exception raised: ");

		va_list ap;
		va_start(ap, info);
		out.append_(fmt.c_str(), info->arg_count, info->funcs, ap);
		va_end(ap);

		fwk::raiseException(onFailMakeError(info->file, info->line, out.text()));
	}
}
}
