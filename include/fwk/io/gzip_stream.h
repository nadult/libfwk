// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/io/stream.h"
#include "fwk/pod_vector.h"

namespace fwk {

class GzipStream {
  public:
	GzipStream(GzipStream &&);
	GzipStream &operator=(GzipStream &&);
	~GzipStream();

	GzipStream(const GzipStream &) = delete;
	void operator=(const GzipStream &) = delete;

	// Referenced stream has to exist as long as GzipStream
	static Ex<GzipStream> loader(Stream &, Maybe<i64> load_limit = none);
	//static Ex<GzipStream> saver(Stream&);

	Ex<int> loadData(Span<char>);
	Ex<int> saveData(CSpan<char>);
	Ex<vector<char>> loadData();

	bool isFinished() const { return m_is_finished; }

  protected:
	GzipStream(void *, Stream &, bool);

	vector<char> m_buffer;
	Stream *m_pipe = nullptr;
	void *m_ctx = nullptr;
	i64 m_load_limit = -1;
	bool m_is_loading = false, m_is_valid = true, m_is_finished = false;
};
}
