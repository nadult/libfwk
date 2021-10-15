// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/io/stream.h"
#include "fwk/pod_vector.h"

namespace fwk {

// Stream class for loading/saving plain data from/to file.
class BaseFileStream : public Stream {
  public:
	BaseFileStream(BaseFileStream &&);
	BaseFileStream(const BaseFileStream &) = delete;
	~BaseFileStream();

	BaseFileStream &operator=(BaseFileStream &&);
	void operator=(const BaseFileStream &) = delete;

	ZStr name() const { return m_name; }

	void saveData(CSpan<char>) final;
	void loadData(Span<char>) final;
	void seek(i64) final;

  protected:
	string errorMessage(Str) const final;
	BaseFileStream();
	friend Ex<FileStream> fileStream(ZStr, bool);

	string m_name;
	void *m_file;
};

Ex<FileStream> fileLoader(ZStr file_name);
Ex<FileStream> fileSaver(ZStr file_name);
}
