// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#include "fwk_math_ext.h"
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
		template <> double fromString(TextParser &parser) { return parser.parseDouble(); }

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

		template <> double2 fromString(TextParser &parser) {
			double2 out;
			parser.parseDoubles(out.v);
			return out;
		}

		template <> double3 fromString(TextParser &parser) {
			double3 out;
			parser.parseDoubles(out.v);
			return out;
		}

		template <> double4 fromString(TextParser &parser) {
			double4 out;
			parser.parseDoubles(out.v);
			return out;
		}

		template <> FRect fromString(TextParser &parser) {
			float out[4];
			parser.parseFloats(out);
			return {{out[0], out[1]}, {out[2], out[3]}};
		}

		template <> IRect fromString(TextParser &parser) {
			int out[4];
			parser.parseInts(out);
			return {{out[0], out[1]}, {out[2], out[3]}};
		}

		template <> DRect fromString(TextParser &parser) {
			double out[4];
			parser.parseDoubles(out);
			return {{out[0], out[1]}, {out[2], out[3]}};
		}

		template <> FBox fromString(TextParser &parser) {
			float out[6];
			parser.parseFloats(out);
			return {{out[0], out[1], out[2]}, {out[3], out[4], out[5]}};
		}

		template <> IBox fromString(TextParser &parser) {
			int out[6];
			parser.parseInts(out);
			return {{out[0], out[1], out[2]}, {out[3], out[4], out[5]}};
		}

		template <> DBox fromString(TextParser &parser) {
			double out[6];
			parser.parseDoubles(out);
			return {{out[0], out[1], out[2]}, {out[3], out[4], out[5]}};
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

		template <> vector<double> vectorFromString<double>(TextParser &parser) {
			vector<double> out(parser.countElements());
			parser.parseDoubles(out);
			return out;
		}

		template <> vector<int> vectorFromString<int>(TextParser &parser) {
			vector<int> out(parser.countElements());
			parser.parseInts(out);
			return out;
		}

		template <> vector<uint> vectorFromString<uint>(TextParser &parser) {
			vector<uint> out(parser.countElements());
			parser.parseUints(out);
			return out;
		}

		void toString(const char *value, TextFormatter &out) { out("%s", value); }
		void toString(const string &value, TextFormatter &out) { out("%s", value.c_str()); }

		void toString(bool value, TextFormatter &out) { out(value ? "true" : "false"); }
		void toString(int value, TextFormatter &out) { out("%d", value); }
		void toString(uint value, TextFormatter &out) { out("%u", value); }
		void toString(unsigned long value, TextFormatter &out) { out("%lu", value); }
		void toString(long long value, TextFormatter &out) { out("%lld", value); }
		void toString(unsigned long long value, TextFormatter &out) { out("%llu", value); }
		void toString(qint value, TextFormatter &out) {
			// Max digits: about 36
			char buffer[64];
			int pos = 0;

			bool sign = value < 0;
			if(value < 0)
				value = -value;

			while(value > 0) {
				int digit(value % 10);
				value /= 10;
				buffer[pos++] = '0' + digit;
			}

			if(pos == 0)
				buffer[pos++] = '0';
			if(sign)
				buffer[pos++] = '-';
			buffer[pos] = 0;

			std::reverse(buffer, buffer + pos);
			out("%s", buffer);
		}

		void toString(qint2 value, TextFormatter &out) {
			toString(value[0], out);
			out(" ");
			toString(value[1], out);
		}

		void toString(const int2 &value, TextFormatter &out) { out("%d %d", value.x, value.y); }
		void toString(const int3 &value, TextFormatter &out) {
			out("%d %d %d", value.x, value.y, value.z);
		}
		void toString(const int4 &value, TextFormatter &out) {
			out("%d %d %d %d", value.x, value.y, value.z, value.w);
		}

		template <class Base, int min_size>
		static void doublesToString(CSpan<Base, min_size> in, TextFormatter &out) {
			for(int n = 0; n < in.size(); n++) {
				char buffer[DBL_MAX_10_EXP + 2];
				// TODO: save floats accurately?
				// printf("%1.8e", f)

				int len = snprintf(buffer, sizeof(buffer), "%f", (double)in[n]);
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

		template <class TSpan, EnableIfSpan<TSpan>...>
		static void doublesToString(const TSpan &span, TextFormatter &out) {
			doublesToString(makeSpan(span), out);
		}

		void toString(double value, TextFormatter &out) {
			double values[1] = {value};
			doublesToString(values, out);
		}

		void toString(float value, TextFormatter &out) {
			float values[1] = {value};
			doublesToString(values, out);
		}

		void toString(const float2 &value, TextFormatter &out) {
			float values[2] = {value.x, value.y};
			doublesToString(values, out);
		}

		void toString(const float3 &value, TextFormatter &out) {
			float values[3] = {value.x, value.y, value.z};
			doublesToString(values, out);
		}

		void toString(const float4 &value, TextFormatter &out) {
			float values[4] = {value.x, value.y, value.z, value.w};
			doublesToString(values, out);
		}

		void toString(const double2 &value, TextFormatter &out) {
			double values[2] = {value.x, value.y};
			doublesToString(values, out);
		}

		void toString(const double3 &value, TextFormatter &out) {
			double values[3] = {value.x, value.y, value.z};
			doublesToString(values, out);
		}

		void toString(const double4 &value, TextFormatter &out) {
			double values[4] = {value.x, value.y, value.z, value.w};
			doublesToString(values, out);
		}

		void toString(const FRect &rect, TextFormatter &out) {
			float values[4] = {rect.x(), rect.y(), rect.ex(), rect.ey()};
			doublesToString(values, out);
		}

		void toString(const DRect &rect, TextFormatter &out) {
			double values[4] = {rect.x(), rect.y(), rect.ex(), rect.ey()};
			doublesToString(values, out);
		}

		void toString(const IRect &rect, TextFormatter &out) {
			out("%d %d %d %d", rect.x(), rect.y(), rect.ex(), rect.ey());
		}

		void toString(const IBox &box, TextFormatter &out) {
			out("%d %d %d %d %d %d", box.x(), box.y(), box.z(), box.ex(), box.ey(), box.ez());
		}

		void toString(const FBox &box, TextFormatter &out) {
			float values[6] = {box.x(), box.y(), box.z(), box.ex(), box.ey(), box.ez()};
			doublesToString(values, out);
		}

		void toString(const DBox &box, TextFormatter &out) {
			double values[6] = {box.x(), box.y(), box.z(), box.ex(), box.ey(), box.ez()};
			doublesToString(values, out);
		}

		void toString(const Matrix4 &value, TextFormatter &out) {
			doublesToString(CSpan<float>(&value[0].x, 16), out);
		}

		void toString(const Quat &value, TextFormatter &out) {
			doublesToString(value.values(), out);
		}
	}
}
}
