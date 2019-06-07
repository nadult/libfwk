// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/audio_base.h"
#include "fwk/sys/unique_ptr.h"

namespace fwk {

class OggStream {
  public:
	OggStream(const char *file_name);
	FWK_MOVABLE_CLASS(OggStream)
	Sound makeSound() const;

  protected:
	string m_file_name;
	struct Impl;
	UniquePtr<Impl> m_impl;
};

}

