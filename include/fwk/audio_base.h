// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/math_base.h"
#include "fwk/sys_base.h"

namespace fwk {

struct SoundInfo {
	SoundInfo(int sampling_freq = 44100, int bits = 16, bool is_stereo = false)
		: sampling_freq(sampling_freq), bits(bits), is_stereo(is_stereo) {}

	int sampling_freq;
	int bits;
	bool is_stereo;
};

struct SoundPos {
	SoundPos(const float3 &pos, const float3 &velocity = float3(), bool is_relative = false)
		: pos(pos), velocity(velocity), is_relative(is_relative) {}
	SoundPos() : SoundPos(float3(), float3(), true) {}

	float3 pos;
	float3 velocity;
	bool is_relative;
};

struct SoundConfig {
	SoundConfig(float gain = 1.0f, float rolloff = 1.0f, bool is_looped = false)
		: gain(gain), rolloff(rolloff), is_looped(is_looped) {}

	float gain;
	float rolloff;
	bool is_looped;
};

class Sound;
class OggStream;
class AlSound;
class AlDevice;

// TODO: remove it
using PSound = shared_ptr<AlSound>;
}
