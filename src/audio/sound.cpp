// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk/audio/sound.h"

#include "fwk/io/file_stream.h"
#include "fwk/sys/expected.h"

namespace fwk {

Sound::Sound(vector<char> data, const SoundInfo &info) : m_data(data), m_info(info) {
	// TODO: verification
}

FWK_COPYABLE_CLASS_IMPL(Sound)

Ex<Sound> Sound::load(FileStream &sr) {
	u32 chunk_size[2], size, frequency, byteRate;
	u16 format, channels, block_align, bits;

	// Chunk 0
	EXPECT(sr.loadSignature("RIFF"));
	sr >> chunk_size[0];
	EXPECT(sr.loadSignature("WAVE"));

	// Chunk 1
	EXPECT(sr.loadSignature("fmt "));
	sr.unpack(chunk_size[1], format, channels, frequency, byteRate, block_align, bits);

	EXPECT(block_align == channels * (bits / 8));
	EXPECT(format == 1 && "Only PCM format is supported");

	sr.seek(sr.pos() + chunk_size[1] - 16);

	// Chunk 2
	EXPECT(sr.loadSignature("data"));
	sr >> size;

	if(channels > 2 || (bits != 8 && bits != 16))
		return FWK_ERROR("Unsupported format (bits: %d channels: %d)", bits, channels);

	vector<char> data(size);
	sr.loadData(data);
	return Sound(std::move(data), {(int)frequency, bits, channels > 1});
}

Ex<> Sound::save(FileStream &sr) const {
	u32 chunk_size[2] = {(u32)m_data.size() + 36, 16};

	// Chunk 0
	sr.saveSignature("RIFF");
	sr << chunk_size[0];
	sr.saveSignature("WAVE");

	// Chunk 1
	sr.saveSignature("fmt ");
	sr << chunk_size[1];

	int num_channels = m_info.is_stereo ? 2 : 1;
	int byte_rate = m_info.sampling_freq * num_channels * (m_info.bits / 8),
		block_align = num_channels * (m_info.bits / 8);
	sr.pack(u16(1), u16(num_channels), u32(m_info.sampling_freq), u32(byte_rate), u16(block_align),
			u16(m_info.bits));

	// Chunk 2
	sr.saveSignature("data");
	sr << u32(m_data.size());
	sr.saveData(m_data);

	return {};
}

double Sound::lengthInSeconds() const {
	return double(m_data.size()) /
		   double(m_info.sampling_freq * (m_info.is_stereo ? 2 : 1) * (m_info.bits / 8));
}
}
