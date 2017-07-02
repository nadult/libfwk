// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_audio.h"

namespace fwk {

Sound::Sound(vector<char> data, const SoundInfo &info) : m_data(data), m_info(info) {
	// TODO: verification
}

Sound::Sound(Stream &sr) {
	// TODO: use first constructor
	u32 chunk_size[2], size, frequency, byteRate;
	u16 format, channels, block_align, bits;

	// Chunk 0
	sr.signature("RIFF", 4);
	sr >> chunk_size[0];
	sr.signature("WAVE", 4);

	// Chunk 1
	sr.signature("fmt ", 4);
	sr.unpack(chunk_size[1], format, channels, frequency, byteRate, block_align, bits);
	ASSERT(block_align == channels * (bits / 8));

	if(format != 1)
		THROW("Unknown format (only PCM is supported)");

	sr.seek(sr.pos() + chunk_size[1] - 16);

	// Chunk 2
	sr.signature("data", 4);
	sr >> size;

	if(channels > 2 || (bits != 8 && bits != 16))
		THROW("Unsupported format (bits: %d channels: %d)", bits, channels);

	m_data.resize(size);
	sr.loadData(m_data.data(), size);
	m_info.sampling_freq = frequency;
	m_info.bits = bits;
	m_info.is_stereo = channels > 1;
}

void Sound::save(Stream &sr) const {
	u32 chunk_size[2] = {(u32)m_data.size() + 36, 16};

	// Chunk 0
	sr.signature("RIFF", 4);
	sr << chunk_size[0];
	sr.signature("WAVE", 4);

	// Chunk 1
	sr.signature("fmt ", 4);
	sr << chunk_size[1];

	int num_channels = m_info.is_stereo ? 2 : 1;
	int byte_rate = m_info.sampling_freq * num_channels * (m_info.bits / 8),
		block_align = num_channels * (m_info.bits / 8);
	sr.pack(u16(1), u16(num_channels), u32(m_info.sampling_freq), u32(byte_rate), u16(block_align),
			u16(m_info.bits));

	// Chunk 2
	sr.signature("data", 4);
	sr << u32(m_data.size());
	sr.saveData(m_data.data(), m_data.size());
}

double Sound::lengthInSeconds() const {
	return double(m_data.size()) /
		   double(m_info.sampling_freq * (m_info.is_stereo ? 2 : 1) * (m_info.bits / 8));
}
}
