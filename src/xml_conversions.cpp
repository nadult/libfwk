#include "fwk_xml.h"

namespace fwk {
namespace xml_conversions {

	template <> bool fromString(const char *input) {
		if(strcasecmp(input, "true") == 0)
			return true;
		if(strcasecmp(input, "false") == 0)
			return false;
		return fromString<int>(input) != 0;
	}

	static void toInt(const char *input, int count, int *out) {
		DASSERT(input && count <= 4);
		if(!input[0]) {
			for(int i = 0; i < count; i++)
				out[i] = 0;
			return;
		}

		const char *format = "%d %d %d %d";
		if(sscanf(input, format + (4 - count) * 3, out + 0, out + 1, out + 2, out + 3) != count)
			THROW("Error while parsing %d ints from string \"%s\"", count, input);
	}

	static void toFloat(const char *input, int count, float *out) {
		DASSERT(input && count <= 4);
		if(!input[0]) {
			for(int i = 0; i < count; i++)
				out[i] = 0.0f;
			return;
		}

		const char *format = "%f %f %f %f";
		if(sscanf(input, format + (4 - count) * 3, out + 0, out + 1, out + 2, out + 3) != count)
			THROW("Error while parsing %d floats from string \"%s\"", count, input);
	}

	template <> int fromString(const char *input) {
		int out[1];
		toInt(input, 1, out);
		return out[0];
	}

	template <> int2 fromString(const char *input) {
		int out[2];
		toInt(input, 2, out);
		return int2(out[0], out[1]);
	}

	template <> int3 fromString(const char *input) {
		int out[3];
		toInt(input, 3, out);
		return int3(out[0], out[1], out[2]);
	}

	template <> int4 fromString(const char *input) {
		int out[4];
		toInt(input, 4, out);
		return int4(out[0], out[1], out[2], out[3]);
	}

	template <> float fromString(const char *input) {
		float out[1];
		toFloat(input, 1, out);
		return out[0];
	}

	template <> float2 fromString(const char *input) {
		float out[2];
		toFloat(input, 2, out);
		return float2(out[0], out[1]);
	}

	template <> float3 fromString(const char *input) {
		float out[3];
		toFloat(input, 3, out);
		return float3(out[0], out[1], out[2]);
	}

	template <> float4 fromString(const char *input) {
		float out[4];
		toFloat(input, 4, out);
		return float4(out[0], out[1], out[2], out[3]);
	}

	template <> FRect fromString(const char *input) {
		float out[4];
		toFloat(input, 4, out);
		return FRect(out[0], out[1], out[2], out[3]);
	}

	template <> IRect fromString(const char *input) {
		int out[4];
		toInt(input, 4, out);
		return IRect(out[0], out[1], out[2], out[3]);
	}

	template <> vector<string> fromString<vector<string>>(const char *input) {
		vector<string> out;
		const char *iptr = input;

		while(*iptr) {
			const char *next_space = strchr(iptr, ' ');
			int len = next_space ? next_space - iptr : strlen(iptr);

			out.emplace_back(string(iptr, iptr + len));

			if(!next_space)
				break;
			iptr = next_space + 1;
		}

		return out;
	}

	template <> void toString(const bool &value, TextFormatter &out) {
		out(value ? "true" : "false");
	}
	template <> void toString(const int &value, TextFormatter &out) { out("%d", value); }
	template <> void toString(const int2 &value, TextFormatter &out) {
		out("%d %d", value.x, value.y);
	}
	template <> void toString(const int3 &value, TextFormatter &out) {
		out("%d %d %d", value.x, value.y, value.z);
	}
	template <> void toString(const int4 &value, TextFormatter &out) {
		out("%d %d %d %d", value.x, value.y, value.z, value.w);
	}

	template <int count> static void toStringFloat(float *values, TextFormatter &out) {
		for(int n = 0; n < count; n++) {
			int ivalue = (int)values[n];
			if(values[n] == (float)ivalue)
				out("%d", ivalue);
			else
				out("%f", values[n]);
			if(n + 1 < count)
				out(" ");
		}
	}

	template <> void toString(const float &value, TextFormatter &out) {
		float values[1] = {value};
		toStringFloat<1>(values, out);
	}
	template <> void toString(const float2 &value, TextFormatter &out) {
		float values[2] = {value.x, value.y};
		toStringFloat<2>(values, out);
	}
	template <> void toString(const float3 &value, TextFormatter &out) {
		float values[3] = {value.x, value.y, value.z};
		toStringFloat<3>(values, out);
	}
	template <> void toString(const float4 &value, TextFormatter &out) {
		float values[4] = {value.x, value.y, value.z, value.w};
		toStringFloat<4>(values, out);
	}

	template <> void toString(const FRect &rect, TextFormatter &out) {
		float values[4] = {rect.min.x, rect.min.y, rect.max.x, rect.max.y};
		toStringFloat<4>(values, out);
	}
	template <> void toString(const IRect &rect, TextFormatter &out) {
		out("%d %d %d %d", rect.min.x, rect.min.y, rect.max.x, rect.max.y);
	}
}
}
