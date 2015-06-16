enum {
	// TODO: tune-able parameters?
	min0 = 16,
	min1 = 1024 * 4,
	min2 = 1024 * 1024,
	min3 = 1024 * 1024 * 128,

	max0 = 64,
	max1 = 16 * 1024,
	max2 = 1024 * 1024 * 4,
	max3 = 1024 * 1024 * 512,
};

void Stream::encodeInt(int value) {
	if(value >= -min0 && value < max0 - min0) {
		value += min0;
		pack(u8(0x00 + value));
	} else if(value >= -min1 && value < max1 - min1) {
		value += min1;
		pack(u8(0x40 + (value & 0x3f)), u8(value >> 6));
	} else if(value >= -min2 && value < max2 - min2) {
		value += min2;
		pack(u8(0x80 + (value & 0x3f)), u8((value >> 6) & 0xff), u8((value >> 14)));
	} else if(value >= -min3 && value < max3 - min3) {
		value += min3;
		pack(u8(0xc0 + (value & 0x1f)), u8((value >> 5) & 0xff), u8((value >> 13) & 0xff),
			 u8(value >> 21));
	} else {
		pack(u8(0xff), u8(value & 0xff), u8((value >> 8) & 0xff), u8((value >> 16) & 0xff),
			 u8(value >> 24));
	}
}

int Stream::decodeInt() {
	u8 first_byte, bytes[4];
	loadData(&first_byte, 1);

	u8 header = first_byte & 0xc0;
	if(header == 0x00) {
		return (first_byte & 0x3f) - min0;
	} else if(header == 0x40) {
		loadData(bytes, 1);
		return (i32(first_byte & 0x3f) | (i32(bytes[0]) << 6)) - min1;
	} else if(header == 0x80) {
		loadData(bytes, 2);
		return (i32(first_byte & 0x3f) | (i32(bytes[0]) << 6) | (i32(bytes[1]) << 14)) - min2;
	} else {
		if(first_byte == 0xff) {
			loadData(bytes, 4);
			return (i32(bytes[0]) | (i32(bytes[1]) << 8) | (i32(bytes[2]) << 16) |
					(i32(bytes[3]) << 24));
		} else {
			loadData(bytes, 3);
			return (i32(first_byte & 0x1f) | i32(bytes[0] << 5) | (i32(bytes[1]) << 13) |
					(i32(bytes[2]) << 21)) -
				   min3;
		}
	}
}

void testIntEncoding() {
	// int min = -2ll * 1024 * 1024 * 1024, max = 2ll * 1024 * 1024 * 1024 - 1, step = 1024 * 1024 *
	// 32;
	int min = -2ll * 1024 * 1024, max = 2ll * 1024 * 1024 - 1, step = 1024 * 1024;

	printf("Testing int encoding: \n");

#pragma omp parallel for
	for(int i = min / step; i < max / step; i++) {
		vector<char> buffer(1024 * 1024 * 200);
		MemorySaver sstream(buffer.data(), buffer.size());

		int end = (long long)max - i * step < step ? max : i * step + step;
		for(int j = i * step; j < end; j++)
			sstream.encodeInt(j);

		MemoryLoader lstream(buffer.data(), sstream.pos());
		for(int j = i * step; j < end; j++)
			ASSERT(lstream.decodeInt() == j);

		printf("%11d - %11d: OK\n", i * step, end);
	}
}
