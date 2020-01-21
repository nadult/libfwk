// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/io/stream.h"
#include "fwk/pod_vector.h"

namespace fwk {

// Simple class for loading/saving plain data from/to file
// It treats all data as plain bytes, so you have to be very careful when using it.
// There are also special functions for loading/saving strings & PodVectors.
//
// FWK exceptions are used for error handling; besides that:
// - when error happens all following operations will not be performed
// - when loading: returned data will be zeroed, vectors & strings will be empty
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
	void raise(ZStr) final;
	BaseFileStream();
	friend Ex<FileStream> fileStream(ZStr, bool);

	string m_name;
	void *m_file;
};

Ex<FileStream> fileLoader(ZStr file_name);
Ex<FileStream> fileSaver(ZStr file_name);
}
