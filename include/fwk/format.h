// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef FWK_FORMAT_H
#define FWK_FORMAT_H

#include "fwk/pod_vector.h"
#include "fwk_base.h"
#include "fwk_math.h"

namespace fwk {

DEFINE_ENUM(FormatMode, plain, structured);
DEFINE_ENUM(FormatPrecision, adaptive, maximum);

struct FormatOptions {
	FormatMode mode = FormatMode::plain;
	FormatPrecision precision = FormatPrecision::adaptive;

	void set(FormatOptions options) { *this = options; }
	void set(FormatMode tmode) { mode = tmode; }
	void set(FormatPrecision tprecision) { precision = tprecision; }
};

template <class T> constexpr static bool isFormatType() {
	return isSame<T, FormatOptions>() || isSame<T, FormatMode>() || isSame<T, FormatPrecision>();
}

struct NotFormattible;

namespace detail {
	using TFValue = unsigned long long;
	static_assert(sizeof(TFValue) >= sizeof(void *));

	using TFFunc = void (*)(TextFormatter &, TFValue);

	template <class T> struct IsFormattible {
		template <class U>
		static auto test(const U &)
			-> decltype(std::declval<TextFormatter &>() << std::declval<const U &>());
		static char test(...);
		enum {
			value = std::is_same<decltype(test(std::declval<const T &>())), TextFormatter &>::value
		};
	};

	template <class T> void append(TextFormatter &fmt, TFValue val) {
		const T &ref = *reinterpret_cast<const T *>(val);
		fmt << ref;
	}

	template <class T> struct AppendAccess {
		static constexpr TFFunc func() { return append<T>; }
	};

#define PASS_VALUE(type)                                                                           \
	template <> void append<type>(TextFormatter & fmt, TFValue);                                   \
	constexpr TFValue getTFValue(const type &ref) { return TFValue(ref); }                         \
	constexpr auto getTFFunc(const type &ref) { return AppendAccess<type>(); }
#define PASS_POINTER(type)                                                                         \
	template <> void append<type>(TextFormatter & fmt, TFValue);                                   \
	inline TFValue getTFValue(const type &ref) { return TFValue(&ref); }                           \
	constexpr auto getTFFunc(const type &ref) { return AppendAccess<type>(); }

	PASS_VALUE(char)
	PASS_VALUE(unsigned char)
	PASS_VALUE(short)
	PASS_VALUE(unsigned short)
	PASS_VALUE(int)
	PASS_VALUE(unsigned int)
	PASS_VALUE(long)
	PASS_VALUE(unsigned long)
	PASS_VALUE(long long)
	PASS_VALUE(unsigned long long)
	PASS_VALUE(bool)

	PASS_POINTER(double)
	PASS_POINTER(float)

	PASS_POINTER(FormatOptions)
	PASS_POINTER(FormatMode)
	PASS_POINTER(FormatPrecision)

#undef PASS_VALUE
#undef PASS_POINTER

	template <class T> constexpr TFValue getTFValue(const T &ref) { return TFValue(&ref); }
	template <class T> constexpr auto getTFFunc(const T &ref) { return AppendAccess<T>(); }

	template <> void append<const char *>(TextFormatter &, TFValue);
	template <int N> inline TFValue getTFValue(char (&str)[N]) { return TFValue(str); }

	inline TFValue getTFValue(const char *str) { return TFValue(str); }
	inline TFValue getTFValue(const string &str) { return TFValue(str.c_str()); }
	inline TFValue getTFValue(StringRef str) { return TFValue(str.c_str()); }

	template <int N> constexpr auto getTFFunc(char (&str)[N]) {
		return AppendAccess<const char *>();
	}
	constexpr auto getTFFunc(const char *) { return AppendAccess<const char *>(); }
	constexpr auto getTFFunc(const string &) { return AppendAccess<const char *>(); }
	constexpr auto getTFFunc(const StringRef &) { return AppendAccess<const char *>(); }

	template <class T> constexpr TFFunc getTFFunc() {
		return decltype(getTFFunc(std::declval<const T &>()))::func();
	}

	void formatSpan(TextFormatter &out, const char *data, int size, int obj_size, TFFunc);
}

template <class T> constexpr bool isFormattible() { return detail::IsFormattible<T>::value; }

template <class... Args> constexpr bool areFormattible() {
	return detail::Conjunction<detail::IsFormattible<Args>...>::value;
}

template <class T>
using EnableIfFormattible = EnableIf<detail::IsFormattible<T>::value, NotFormattible>;
template <class... Args>
using EnableIfFormattibleN = EnableIf<areFormattible<Args...>(), NotFormattible>;

// TODO: escapable % ?
// TODO: better name
class TextFormatter {
  public:
	explicit TextFormatter(int initial_size = 256, FormatOptions = {});
	TextFormatter(const TextFormatter &);
	~TextFormatter();

	TextFormatter &operator<<(StringRef);
	TextFormatter &operator<<(const char *);
	TextFormatter &operator<<(const string &);
	template <int N> TextFormatter &operator<<(const char (&str)[N]) {
		return *this << (const char *)str;
	}

	// char is treated as a single character, not like a number!
	TextFormatter &operator<<(char);

	TextFormatter &operator<<(bool);
	TextFormatter &operator<<(double);
	TextFormatter &operator<<(float);
	TextFormatter &operator<<(int);
	TextFormatter &operator<<(unsigned int);
	TextFormatter &operator<<(long);
	TextFormatter &operator<<(unsigned long);
	TextFormatter &operator<<(long long);
	TextFormatter &operator<<(unsigned long long);

	void reserve(int) NOINLINE;
	void trim(int count);

	const char *c_str() const { return m_data.data(); }
	StringRef text() const { return {m_data.data(), m_offset}; }
	int length() const { return m_offset; }

	const FormatOptions &options() const { return m_options; }
	FormatOptions &options() { return m_options; }

	bool isStructured() const { return m_options.mode == FormatMode::structured; }
	bool isPlain() const { return m_options.mode == FormatMode::plain; }

	template <class... Args, EnableIfFormattibleN<Args...>...>
	void operator()(const char *format_str, const Args &... args) {
		append_(format_str, sizeof...(Args), getFuncs<Args...>(), detail::getTFValue(args)...);
	}

	void stdFormat(const char *format, ...) ATTRIB_PRINTF(2, 3);

  private:
	using Value = detail::TFValue;
	using Func = detail::TFFunc;

	template <class... Args> static inline const Func *getFuncs() {
		static constexpr const Func funcs[] = {detail::getTFFunc<Args>()...};
		return funcs;
	}

	// Basically TextFormatter is trying to pass arguments to internal functions efficiently:
	// all arguments are passed as unsigned long long; all types which naturally fit in this number
	// are passed by value, all other types are passed by pointer
	void append_(const char *format, int arg_count, const Func *, va_list);
	void append_(const char *format, int arg_count, const Func *, ...);
	static string strFormat_(const char *format, int arg_count, const Func *, ...);
	static void print_(FormatMode, const char *format, int arg_count, const Func *, ...);
	static string toString_(Func, Value);

	const char *nextElement(const char *format_str);

	template <class... T, EnableIfFormattibleN<T...>...>
	friend string format(const char *str, T &&... args) {
		return strFormat_(str, sizeof...(T), getFuncs<T...>(), detail::getTFValue(args)...);
	}

	template <class... T, EnableIfFormattibleN<T...>...>
	friend void print(const char *str, T &&... args) {
		print_(FormatMode::structured, str, sizeof...(T), getFuncs<T...>(),
			   detail::getTFValue(args)...);
	}

	template <class... T, EnableIfFormattibleN<T...>...>
	friend void printPlain(const char *str, T &&... args) {
		print_(FormatMode::plain, str, sizeof...(T), getFuncs<T...>(), detail::getTFValue(args)...);
	}

	template <class T, EnableIf<!isEnum<RemoveReference<T>>()>..., EnableIfFormattible<T>...>
	friend string toString(T &&value) {
		return toString_(detail::getTFFunc<T>(), detail::getTFValue(value));
	}

	// TODO: It would be better to have string type internally
	PodVector<char> m_data;
	int m_offset;
	FormatOptions m_options;
};

// To make new type formattable: simply overload operator<<:
// TextFormatter &format(TextFormatter&, const MyNewType &rhs);

TextFormatter &operator<<(TextFormatter &, const int2 &);
TextFormatter &operator<<(TextFormatter &, const int3 &);
TextFormatter &operator<<(TextFormatter &, const int4 &);

TextFormatter &operator<<(TextFormatter &, const double2 &);
TextFormatter &operator<<(TextFormatter &, const double3 &);
TextFormatter &operator<<(TextFormatter &, const double4 &);

TextFormatter &operator<<(TextFormatter &, const float2 &);
TextFormatter &operator<<(TextFormatter &, const float3 &);
TextFormatter &operator<<(TextFormatter &, const float4 &);

TextFormatter &operator<<(TextFormatter &, const DRect &);
TextFormatter &operator<<(TextFormatter &, const FRect &);
TextFormatter &operator<<(TextFormatter &, const IRect &);

TextFormatter &operator<<(TextFormatter &, const FBox &);
TextFormatter &operator<<(TextFormatter &, const IBox &);
TextFormatter &operator<<(TextFormatter &, const DBox &);

TextFormatter &operator<<(TextFormatter &, const Matrix4 &);
TextFormatter &operator<<(TextFormatter &, const Quat &);

template <class TSpan, class T = SpanBase<TSpan>, EnableIfFormattible<T>...>
TextFormatter &operator<<(TextFormatter &out, const TSpan &span) {
	detail::TFFunc func = [](TextFormatter &fmt, unsigned long long ptr) {
		const T &ref = *(const T *)ptr;
		auto func = detail::getTFFunc<T>();
		return func(fmt, detail::getTFValue(ref));
	};
	detail::formatSpan(out, (const char *)fwk::data(span), fwk::size(span), sizeof(T), func);
	return out;
}

template <class TRange, class T = RangeBase<TRange>, EnableIfFormattible<T>...,
		  EnableIf<!isSpan<TRange>()>...>
TextFormatter &operator<<(TextFormatter &out, const TRange &range) {
	const char *separator = out.isStructured() ? ", " : " ";

	if(out.isStructured())
		out << '[';
	auto it = begin(range), it_end = end(range);
	while(it != it_end) {
		out << *it;
		++it;
		if(it != it_end)
			out << separator;
	}
	if(out.isStructured())
		out << ']';
	return out;
}

template <class TVector, EnableIfVector<TVector>...>
TextFormatter &operator<<(TextFormatter &out, const TVector &vec) {
	enum { N = TVector::vector_size };
	if constexpr(N == 2)
		out(out.isStructured() ? "(%, %)" : "% %", vec[0], vec[1]);
	else if(N == 3)
		out(out.isStructured() ? "(%, %, %)" : "% % %", vec[0], vec[1], vec[2]);
	else if(N == 4)
		out(out.isStructured() ? "(%, %, %, %)" : "% % % %", vec[0], vec[1], vec[2], vec[3]);
	else
		static_assert((N >= 2 && N <= 4), "Vector size not supported");
	return out;
}

template <class T> TextFormatter &operator<<(TextFormatter &out, const Box<T> &box) {
	out(out.isStructured() ? "(%; %)" : "% %", box.min(), box.max());
	return out;
}

template <class T1, class T2>
TextFormatter &operator<<(TextFormatter &out, const pair<T1, T2> &pair) {
	out(out.isStructured() ? "(%; %)" : "% %", pair.first, pair.second);
	return out;
}

template <class T, EnableIfEnum<T>...> TextFormatter &operator<<(TextFormatter &out, T value) {
	return out << toString(value);
}

string stdFormat(const char *format, ...) ATTRIB_PRINTF(1, 2);

template <class T, EnableIf<!isEnum<RemoveReference<T>>()>..., EnableIfFormattible<T>...>
string toString(T &&value);

template <class... T, EnableIfFormattibleN<T...>...> string format(const char *str, T &&...);
template <class... T, EnableIfFormattibleN<T...>...> void print(const char *str, T &&...);
template <class... T, EnableIfFormattibleN<T...>...> void printPlain(const char *str, T &&...);
}

#endif