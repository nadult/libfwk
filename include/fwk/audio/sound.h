// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/audio_base.h"
#include "fwk/vector.h"

namespace fwk {

class Sound {
  public:
	Sound(vector<char> data = {}, const SoundInfo & = SoundInfo());
	FWK_COPYABLE_CLASS(Sound);

	static Ex<Sound> load(FileStream &);
	Ex<> save(FileStream &) const;

	const SoundInfo &info() const { return m_info; }
	const auto &data() const { return m_data; }

	double lengthInSeconds() const;

  protected:
	vector<char> m_data;
	SoundInfo m_info;
};

}
