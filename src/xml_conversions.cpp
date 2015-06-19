#include "fwk_xml.h"
#include <cfloat>

namespace fwk {
namespace xml_conversions {

	namespace detail {

		template <> string fromString(TextParser &parser) { return parser.parseString(); }
		template <> bool fromString(TextParser &parser) { return parser.parseBool(); }
		template <> uint fromString(TextParser &parser) { return parser.parseUint(); }
		template <> int fromString(TextParser &parser) { return parser.parseInt(); }
		template <> float fromString(TextParser &parser) { return parser.parseFloat(); }

		template <> int2 fromString(TextParser &parser) {
			int2 out;
			parser.parseInts(out.v);
			return out;
		}

		template <> int3 fromString(TextParser &parser) {
			int3 out;
			parser.parseInts(out.v);
			return out;
		}

		template <> int4 fromString(TextParser &parser) {
			int4 out;
			parser.parseInts(out.v);
			return out;
		}

		template <> float2 fromString(TextParser &parser) {
			float2 out;
			parser.parseFloats(out.v);
			return out;
		}

		template <> float3 fromString(TextParser &parser) {
			float3 out;
			parser.parseFloats(out.v);
			return out;
		}

		template <> float4 fromString(TextParser &parser) {
			float4 out;
			parser.parseFloats(out.v);
			return out;
		}

		template <> FRect fromString(TextParser &parser) {
			float out[4];
			parser.parseFloats(out);
			return FRect(out[0], out[1], out[2], out[3]);
		}

		template <> IRect fromString(TextParser &parser) {
			int out[4];
			parser.parseInts(out);
			return IRect(out[0], out[1], out[2], out[3]);
		}

		template <> FBox fromString(TextParser &parser) {
			float out[6];
			parser.parseFloats(out);
			return FBox(out[0], out[1], out[2], out[3], out[4], out[5]);
		}

		template <> IBox fromString(TextParser &parser) {
			int out[6];
			parser.parseInts(out);
			return IBox(out[0], out[1], out[2], out[3], out[4], out[5]);
		}

		template <> Matrix4 fromString(TextParser &parser) {
			float out[16];
			parser.parseFloats(out);
			return Matrix4(float4(out[0], out[1], out[2], out[3]),
						   float4(out[4], out[5], out[6], out[7]),
						   float4(out[8], out[9], out[10], out[11]),
						   float4(out[12], out[13], out[14], out[15]));
		}

		template <> Quat fromString(TextParser &parser) {
			float out[4];
			parser.parseFloats(out);
			return Quat(out[0], out[1], out[2], out[3]);
		}

		template <> vector<string> vectorFromString<string>(TextParser &parser) {
			vector<string> out(parser.countElements());
			parser.parseStrings(out);
			return out;
		}

		template <> vector<float> vectorFromString<float>(TextParser &parser) {
			vector<float> out(parser.countElements());
			parser.parseFloats(out);
			return out;
		}

		template <> vector<int> vectorFromString<int>(TextParser &parser) {
			vector<int> out(parser.countElements());
			parser.parseInts(out);
			return out;
		}

		template <> void toString(const string &value, TextFormatter &out) {
			out("%s", value.c_str());
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

		template <> void toString(const uint &value, TextFormatter &out) { out("%u", value); }

		static void toStringFloat(CRange<float> in, TextFormatter &out) {
			for(int n = 0; n < in.size(); n++) {
				char buffer[DBL_MAX_10_EXP + 2];
				// TODO: save floats accurately?
				int len = snprintf(buffer, sizeof(buffer), "%f", in[n]);
				while(len > 0 && buffer[len - 1] == '0')
					len--;
				if(len > 0 && buffer[len - 1] == '.')
					len--;
				buffer[len] = 0;
				out("%s", buffer);
				if(n + 1 < in.size())
					out(" ");
			}
		}

		template <> void toString(const float &value, TextFormatter &out) {
			float values[1] = {value};
			toStringFloat(values, out);
		}
		template <> void toString(const float2 &value, TextFormatter &out) {
			float values[2] = {value.x, value.y};
			toStringFloat(values, out);
		}
		template <> void toString(const float3 &value, TextFormatter &out) {
			float values[3] = {value.x, value.y, value.z};
			toStringFloat(values, out);
		}
		template <> void toString(const float4 &value, TextFormatter &out) {
			float values[4] = {value.x, value.y, value.z, value.w};
			toStringFloat(values, out);
		}

		template <> void toString(const FRect &rect, TextFormatter &out) {
			float values[4] = {rect.min.x, rect.min.y, rect.max.x, rect.max.y};
			toStringFloat(values, out);
		}

		template <> void toString(const IRect &rect, TextFormatter &out) {
			out("%d %d %d %d", rect.min.x, rect.min.y, rect.max.x, rect.max.y);
		}

		template <> void toString(const IBox &box, TextFormatter &out) {
			out("%d %d %d %d %d %d", box.min.x, box.min.y, box.min.z, box.max.x, box.max.y,
				box.max.z);
		}

		template <> void toString(const FBox &box, TextFormatter &out) {
			float values[6] = {box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z};
			toStringFloat(values, out);
		}

		template <> void toString(const Matrix4 &value, TextFormatter &out) {
			toStringFloat(CRange<float>(&value[0].x, 16), out);
		}

		template <> void toString(const Quat &value, TextFormatter &out) {
			toStringFloat(value.v, out);
		}
	}
}
}
