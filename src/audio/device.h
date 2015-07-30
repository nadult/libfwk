/* Copyright (C) 2013-2014 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of FreeFT.
 */

#ifndef AUDIO_DEVICE_H
#define AUDIO_DEVICE_H

#include "base.h"
#include "game/base.h"

namespace audio {

// All of the functions can be called without initializing the audio device.
// They won't do anything (or simply return -1 / empty string) in such case.
//
// Sound names are case-insensitive.
//
// Sounds with the same name modulo numerical suffix are grouped together.
// For example: empburst1.wav and empburst2.wav.
//
// TODO: handle non-existent sounds silently in release build (or log them to a file)
// TODO: add upper limit for sound buffers memory
// TODO: add possibility to access specific sound variation (by it's name)

enum {
	max_sources = 16,
};

bool isInitialized();

void initSoundMap();
void initDevice();
void freeDevice();

void printInfo();

void setListener(const float3 &pos, const float3 &vel, const float3 &dir);

void setUnits(float unitsPerMeter);

void tick();

struct SoundIndex {
	SoundIndex() : first_idx(-1), variation_count(0) {}
	SoundIndex(int first_idx, int var_count) : first_idx(first_idx), variation_count(var_count) {}

	void save(Stream &) const;
	void load(Stream &);

	operator int() const { return first_idx; }

	int specificId(int index) const {
		DASSERT(index >= 0 && index < variation_count);
		return first_idx + index + 1;
	}

	int first_idx;
	int variation_count;
};

const SoundIndex findSound(const char *locase_name);

void loadSound(int id);
int playSound(int id, game::SoundType::Type sound_type, const float3 &pos,
			  const float3 &vel = float3(0.0f, 0.0f, 0.0f));
void updateSource(int id, float loudness);

void playSound(int id, float volume = 1.0f);
void playSound(const char *locase_name, float volume = 1.0f);
}

#endif
