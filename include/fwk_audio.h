// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef FWK_AUDIO_H
#define FWK_AUDIO_H

#include "fwk/math_base.h"
#include "fwk/sys/unique_ptr.h"
#include "fwk/sys_base.h"

namespace fwk {

struct SoundInfo {
	SoundInfo(int sampling_freq = 44100, int bits = 16, bool is_stereo = false)
		: sampling_freq(sampling_freq), bits(bits), is_stereo(is_stereo) {}

	int sampling_freq;
	int bits;
	bool is_stereo;
};

class Sound {
  public:
	Sound(vector<char> data = {}, const SoundInfo & = SoundInfo());

	Sound(Stream &);
	void save(Stream &) const;

	const SoundInfo &info() const { return m_info; }
	const auto &data() const { return m_data; }

	double lengthInSeconds() const;

  protected:
	vector<char> m_data;
	SoundInfo m_info;
};

// This is just a handle and can be invalidated by device
// it shouldn't be an immutable object
// TODO: similar case with gfx device objects
class DSound {
  public:
	DSound(const Sound &);
	DSound(DSound &&rhs);
	~DSound();

	DSound(const DSound &) = delete;
	void operator=(const DSound &) = delete;

	bool isValid() const { return m_id != 0; }
	uint id() const { return m_id; }

  private:
	unsigned m_id;
};

using PSound = shared_ptr<DSound>;

class OggStream {
  public:
	OggStream(const char *file_name);
	~OggStream();
	Sound makeSound() const;

  protected:
	string m_file_name;
	struct Impl;
	UniquePtr<Impl> m_impl;
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

class AudioDevice {
  public:
	AudioDevice(int max_sources = 8);
	~AudioDevice();

	static AudioDevice &instance();

	// Swaps frames and synchronizes frame rate
	void tick();

	void printInfo();

	void setListener(const float3 &pos, const float3 &vel, const float3 &dir);
	void setUnits(float unitsPerMeter);

	void updateSource(uint source_id, const SoundPos &);
	void updateSource(uint source_id, const SoundConfig &);
	uint playSound(PSound, const SoundPos &pos, const SoundConfig &config);

  private:
	uint prepSource(uint buffer_id);

	struct Impl;
	UniquePtr<Impl> m_impl;

	float m_max_distance;
	float3 m_listener_pos;
};
}

#endif
