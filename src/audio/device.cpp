// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_audio.h"

#ifdef _WIN32
#define AL_LIBTYPE_STATIC
#endif
#include <AL/al.h>
#include <AL/alc.h>
#include <string.h>

namespace fwk {
const char *errorToString(int id) {
	switch(id) {
#define CASE(e)                                                                                    \
	case e:                                                                                        \
		return #e;
		CASE(AL_NO_ERROR)
		CASE(AL_INVALID_NAME)
		CASE(AL_INVALID_ENUM)
		CASE(AL_INVALID_VALUE)
		CASE(AL_INVALID_OPERATION)
		CASE(AL_OUT_OF_MEMORY)
#undef CASE
	}
	return "UNKNOWN_ERROR";
}

void testError(const char *message) {
	int last_error = alGetError();
	if(last_error != AL_NO_ERROR)
		THROW("%s. %s", message, errorToString(last_error));
}

void uploadToBuffer(const Sound &sound, unsigned buffer_id) {
	const auto &info = sound.info();
	DASSERT(info.bits == 8 || info.bits == 16);

	u32 format = info.bits == 8 ? info.is_stereo ? AL_FORMAT_STEREO8 : AL_FORMAT_MONO8
								: info.is_stereo ? AL_FORMAT_STEREO16 : AL_FORMAT_MONO16;

	alGetError();
	alBufferData(buffer_id, format, sound.data().data(), sound.data().size(), info.sampling_freq);
	testError("Error while loading data to audio buffer.");
}

DSound::DSound(const Sound &sound) {
	alGetError();
	alGenBuffers(1, &m_id);
	testError("Error while creating audio buffer.");

	try {
		uploadToBuffer(sound, m_id);
	} catch(...) {
		alDeleteBuffers(1, &m_id);
		throw;
	}
}

DSound::DSound(DSound &&rhs) : m_id(rhs.m_id) { rhs.m_id = 0; }

DSound::~DSound() {
	if(m_id)
		alDeleteBuffers(1, &m_id);
}

struct AudioDevice::Impl {
	Impl(int max_sources)
		: device(nullptr), context(nullptr), sources(max_sources, 0), free_sources(max_sources, 0),
		  num_free_sources(0), last_time(0.0) {
		if(!(device = alcOpenDevice(0)))
			THROW("Error in alcOpenDevice");
		try {
			if(!(context = alcCreateContext(device, 0)))
				THROW("Error in alcCreateContext");
			alcMakeContextCurrent(context);

			alGetError();
			alGenSources(sources.size(), sources.data());
			testError("Error while creating audio sources.");

			alDistanceModel(AL_LINEAR_DISTANCE_CLAMPED);
			alSpeedOfSound(16.666666f * 343.3);
		} catch(...) {
			if(context)
				alcDestroyContext(context);
			alcCloseDevice(device);
			throw;
		}
	}

	ALCdevice *device;
	ALCcontext *context;

	vector<PSound> source_sounds;
	vector<uint> sources;
	vector<int> free_sources;
	int num_free_sources;
	double last_time;
};

namespace {

	static AudioDevice *s_device = nullptr;
}

AudioDevice &AudioDevice::instance() {
	ASSERT(s_device && "Audio device not initialized");
	return *s_device;
}

AudioDevice::AudioDevice(int max_sources)
	: m_impl(make_unique<Impl>(max_sources)), m_max_distance(500.0f) {
	ASSERT(!s_device);
	s_device = this;
	m_impl->last_time = getTime() - 1.0 / 60.0;
	tick();
}

AudioDevice::~AudioDevice() {
	s_device = nullptr;
	alDeleteSources(m_impl->sources.size(), m_impl->sources.data());
	alcDestroyContext(m_impl->context);
	alcCloseDevice(m_impl->device);
}

void AudioDevice::tick() {
	m_impl->num_free_sources = 0;
	for(int n = 0; n < m_impl->sources.size(); n++) {
		ALint state;
		alGetSourcei(m_impl->sources[n], AL_SOURCE_STATE, &state);
		if(state != AL_PLAYING)
			m_impl->free_sources[m_impl->num_free_sources++] = n;
	}

	double time = getTime();
	double time_delta = time - m_impl->last_time;
	m_impl->last_time = time;
}

void AudioDevice::printInfo() {
	printf("OpenAL vendor: %s\nOpenAL extensions:\n", alGetString(AL_VENDOR));
	const char *text = alcGetString(m_impl->device, ALC_EXTENSIONS);
	while(*text) {
		putc(*text == ' ' ? '\n' : *text, stdout);
		text++;
	}
	printf("\n");
}

void AudioDevice::setListener(const float3 &pos, const float3 &vel, const float3 &dir) {
	m_listener_pos = pos;

	alListener3f(AL_POSITION, pos.x, pos.y, pos.z);
	alListener3f(AL_VELOCITY, vel.x, vel.y, vel.z);
	float v[6] = {dir.x, dir.y, dir.z, 0.0f, 1.0f, 0.0f};
	alListenerfv(AL_ORIENTATION, v);
}

void AudioDevice::setUnits(float meter) {}

uint AudioDevice::prepSource(uint buffer_id) {
	if(!m_impl->num_free_sources || !buffer_id)
		return 0;

	uint source_id = m_impl->free_sources[--m_impl->num_free_sources];
	uint source = m_impl->sources[source_id];
	alSourcei(source, AL_BUFFER, buffer_id);
	alSourcef(source, AL_ROLLOFF_FACTOR, 1.0f);
	alSourcef(source, AL_MAX_DISTANCE, m_max_distance);
	alSourcef(source, AL_REFERENCE_DISTANCE, 10.0f);
	alSource3f(source, AL_DIRECTION, 0.0f, 0.0f, 0.0f);
	return source_id;
}

void AudioDevice::updateSource(uint source_id, const SoundPos &pos) {
	DASSERT((int)source_id < m_impl->sources.size());
	uint source = m_impl->sources[source_id];
	alSource3f(source, AL_POSITION, pos.pos.x, pos.pos.y, pos.pos.z);
	alSource3f(source, AL_VELOCITY, pos.velocity.x, pos.velocity.y, pos.velocity.z);
	alSourcei(source, AL_SOURCE_RELATIVE, pos.is_relative ? AL_TRUE : AL_FALSE);
}

void AudioDevice::updateSource(uint source_id, const SoundConfig &config) {
	DASSERT((int)source_id < m_impl->sources.size());
	uint source = m_impl->sources[source_id];
	alSourcef(source, AL_ROLLOFF_FACTOR, config.rolloff);
	alSourcef(source, AL_GAIN, config.gain);
	alSourcei(source, AL_LOOPING, config.is_looped ? AL_TRUE : AL_FALSE);
}

uint AudioDevice::playSound(PSound sound, const SoundPos &pos, const SoundConfig &config) {
	if(distance(pos.pos, m_listener_pos) > m_max_distance / config.rolloff)
		return 0;

	uint source_id = prepSource(sound->id());
	if(!source_id)
		return 0;

	updateSource(source_id, pos);
	updateSource(source_id, config);

	alSourcePlay(m_impl->sources[source_id]);
	return source_id;
}
}
