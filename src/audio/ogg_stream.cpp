/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#include "fwk_audio.h"
#include <vorbis/vorbisfile.h>

namespace fwk {

struct OggStream::Impl {
	Impl(const char *file_name) {
		memset(&vorbis_file, 0, sizeof(vorbis_file));
		if(ov_fopen(file_name, &vorbis_file) != 0)
			THROW("Error while opening ogg stream: %s\n", file_name);
	}
	~Impl() { ov_clear(&vorbis_file); }

	OggVorbis_File vorbis_file;
};

OggStream::OggStream(const char *file_name)
	: m_file_name(file_name), m_impl(make_unique<Impl>(file_name)) {}
OggStream::~OggStream() {}

Sound OggStream::makeSound() const {
	SoundInfo info;
	auto *vf = &m_impl->vorbis_file;
	auto *vinfo = ov_info(vf, -1);
	info.sampling_freq = vinfo->rate;
	int num_streams = vinfo->channels;
	ASSERT(num_streams <= 2);
	info.is_stereo = num_streams > 1;
	info.bits = 16;

	ov_raw_seek(vf, 0);

	vector<char> data;

	while(true) {
		char buffer[4096];
		int bit_stream = 0;
		int ret = ov_read(vf, buffer, sizeof(buffer), 0, 2, 1, &bit_stream);
		if(ret < 0)
			THROW("Error while reading from ogg stream: %s", m_file_name.c_str());
		if(!ret)
			break;
		data.insert(end(data), buffer, buffer + ret);
	}

	printf("Sound %s: %dKB\n", m_file_name.c_str(), data.size() / 1024);
	return Sound(std::move(data), info);
}
}