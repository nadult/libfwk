// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/io/url_fetch.h"

#ifdef FWK_PLATFORM_HTML
#include <emscripten/fetch.h>
#endif

namespace fwk {

struct UrlFetch::Impl {
	Progress progress;
	Status status = Status::downloading;
	vector<char> data;

#ifdef FWK_PLATFORM_HTML
	void updateProgress(const emscripten_fetch_t &fetch) {
		progress.bytes_downloaded = fetch.dataOffset + fetch.numBytes;
		progress.bytes_total = fetch.totalBytes;
	}

	static void downloadSucceeded(emscripten_fetch_t *fetch) {
		auto &impl = *(Impl *)fetch->userData;
		impl.status = Status::completed;
		impl.data = cspan((const char *)fetch->data, fetch->numBytes);
		impl.progress.bytes_downloaded = impl.progress.bytes_total = fetch->numBytes;
		emscripten_fetch_close(fetch);
	}

	static void downloadFailed(emscripten_fetch_t *fetch) {
		auto &impl = *(Impl *)fetch->userData;
		impl.status = Status::failed;
		impl.updateProgress(*fetch);
		emscripten_fetch_close(fetch);
	}

	static void downloadProgress(emscripten_fetch_t *fetch) {
		auto &impl = *(Impl *)fetch->userData;
		impl.updateProgress(*fetch);
	}
#endif
};

Ex<UrlFetch> UrlFetch::make(ZStr relative_url) {
#ifdef FWK_PLATFORM_HTML
	emscripten_fetch_attr_t attr;
	emscripten_fetch_attr_init(&attr);
	strcpy(attr.requestMethod, "GET");

	UrlFetch out;
	attr.userData = out.m_impl.get();
	attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
	attr.onsuccess = Impl::downloadSucceeded;
	attr.onerror = Impl::downloadFailed;
	attr.onprogress = Impl::downloadProgress;
	auto *fetch = emscripten_fetch(&attr, relative_url.c_str());
	if(!fetch)
		return ERROR("Error while initiating URLFetch");
	out.m_impl->updateProgress(*fetch);
	return out;
#else
	return ERROR("UrlFetch supported only on HTML platform (for now)");
#endif
}

Ex<vector<char>> UrlFetch::finish(UrlFetch fetch) {
#ifdef FWK_PLATFORM_HTML
	auto data = move(fetch.m_impl->data);
	if(fetch.m_impl->status != Status::completed)
		return ERROR("Cannot finish UrlFetch: %",
					 fetch.status() == Status::failed ? "fetch failed" : "still downloading");
	return data;
#else
	return ERROR("UrlFetch supported only on HTML platform (for now)");
#endif
}

UrlFetch::UrlFetch() { m_impl.emplace(); }
FWK_MOVABLE_CLASS_IMPL(UrlFetch)

auto UrlFetch::status() const -> Status { return m_impl->status; }
auto UrlFetch::progress() const -> Progress { return m_impl->progress; }
}
