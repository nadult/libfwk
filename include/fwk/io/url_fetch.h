// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/dynamic.h"
#include "fwk/enum.h"
#include "fwk/sys/expected.h"
#include "fwk/vector.h"

namespace fwk {

DEFINE_ENUM(UrlFetchStatus, downloading, completed, failed);

// Class for downloading files from URL
// For now works only on HTML platform
class UrlFetch {
  public:
	using Status = UrlFetchStatus;

	struct Progress {
		// bytes_total can be 0
		u64 bytes_downloaded = 0, bytes_total = 0;
	};

	FWK_MOVABLE_CLASS(UrlFetch);

	static Ex<UrlFetch> make(ZStr relative_url);
	static Ex<vector<char>> finish(UrlFetch);

	UrlFetchStatus status() const;
	Progress progress() const;

  private:
	UrlFetch();

	struct Impl;
	Dynamic<Impl> m_impl;
};
}
