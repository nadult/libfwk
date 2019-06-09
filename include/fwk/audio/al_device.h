// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/audio_base.h"
#include "fwk/dynamic.h"

namespace fwk {

// This is just a handle and can be invalidated by device
// it shouldn't be an immutable object
// TODO: similar case with gfx device objects
class AlSound {
  public:
	AlSound(const Sound &);
	AlSound(AlSound &&rhs);
	~AlSound();

	AlSound(const AlSound &) = delete;
	void operator=(const AlSound &) = delete;

	bool isValid() const { return m_id != 0; }
	uint id() const { return m_id; }

  private:
	unsigned m_id;
};

class AlDevice {
  public:
	AlDevice(int max_sources = 8);
	FWK_MOVABLE_CLASS(AlDevice)

	static AlDevice &instance();

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
	Dynamic<Impl> m_impl;

	float m_max_distance;
	float3 m_listener_pos;
};
}
