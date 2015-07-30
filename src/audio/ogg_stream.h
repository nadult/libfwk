#ifndef AUDIO_OGG_STREAM_H
#define AUDIO_OGG_STREAM_H

#include "base.h"
#include "audio/buffer.h"

struct OggVorbis_File;

namespace audio {

class OggStream {
  public:
	OggStream();
	OggStream(const OggStream &);
	const OggStream &operator=(const OggStream &);
	~OggStream();

	void Serialize(Serializer &sr);

	vector<char> getData();
	int frequency() const { return frequency; }
	int channels() const { return channels; }
	int bits() const { return 16; }

	void rewind(float sec);
	void free();
	inline bool EndOfStream() const { return endOfStream; }

	struct Data {
		vector<char> data;
		u32 pos;
	};

  private:
	bool _RefreshData();

	Data data;

	int section;
	OggVorbis_File *vFile;
	u32 frequency, channels;
	bool endOfStream;
};
}

#endif
