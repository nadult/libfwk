// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef FWK_FORMAT_H
#define FWK_FORMAT_H

#include "fwk/math/box.h"
#include "fwk/pod_vector.h"
#include "fwk/str.h"
#include "fwk/sys_base.h"

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

struct NotFormattible;

namespace detail {
	using TFValue = unsigned long long;
	static_assert(sizeof(TFValue) >= sizeof(void *));

	using TFFunc = void (*)(TextFormatter &, TFValue);

	// This is tricky because of << definitions at the end of this file
	template <class T> struct IsLeftFormattible {
		FWK_SFINAE_TYPE(Result, T, DECLVAL(TextFormatter &) << DECLVAL(const U &));
		static constexpr bool value = is_same<Result, TextFormatter &>;
	};

	template <class T> struct IsRightFormattible {
		FWK_SFINAE_TYPE(Result, T, DECLVAL(const U &) >> DECLVAL(TextFormatter &));
		static constexpr bool value = is_same<Result, void>;
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
	PASS_POINTER(Str)

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

	template <int N> constexpr auto getTFFunc(char (&str)[N]) {
		return AppendAccess<const char *>();
	}
	constexpr auto getTFFunc(const char *) { return AppendAccess<const char *>(); }
	constexpr auto getTFFunc(const string &) { return AppendAccess<const char *>(); }

	template <class T> constexpr TFFunc getTFFunc() {
		return decltype(getTFFunc(DECLVAL(const T &)))::func();
	}

	void formatSpan(TextFormatter &out, const char *data, int size, int obj_size, TFFunc);
	string autoPrintFormat(const char *args);

	template <class... T> struct TFFuncs {
		static constexpr TFFunc funcs[] = {getTFFunc<T>()...};
		static constexpr int count = sizeof...(T);
	};
	template <class... T> constexpr auto formatFuncs(const T &...) -> TFFuncs<T...>;
}

template <class T> constexpr bool is_right_formattible = detail::IsRightFormattible<T>::value;
template <class T>
constexpr bool is_formattible = detail::IsLeftFormattible<T>::value || is_right_formattible<T>;

template <class T> concept c_formattible = is_formattible<T>;

// TODO: escapable % ?
// TODO: better name
//
// To make new type formattible provide one of the following operators:
// void MyNewType::operator>>(TextFormatter&) const;
// TextFormatter &operator<<(TextFormatter&, const MyNewType &rhs);
class TextFormatter {
  public:
	explicit TextFormatter(int capacity = 256, FormatOptions = {});
	explicit TextFormatter(FormatOptions options) : TextFormatter(256, options) {}
	TextFormatter(const TextFormatter &);
	~TextFormatter();

	TextFormatter &operator<<(Str);
	TextFormatter &operator<<(const char *);
	TextFormatter &operator<<(const string &);
	TextFormatter &operator<<(ZStr str) { return *this << Str(str); }
	template <int N> TextFormatter &operator<<(const char (&str)[N]) { return *this << Str(str); }

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

	FWK_NO_INLINE void reserve(int);
	void trim(int count);

	const char *c_str() const & { return m_data.data(); }
	ZStr text() const & { return {m_data.data(), m_offset}; }
	int size() const { return m_offset; }
	bool empty() const { return m_offset == 0; }
	void clear();

	const FormatOptions &options() const { return m_options; }
	FormatOptions &options() { return m_options; }

	bool isStructured() const { return m_options.mode == FormatMode::structured; }
	bool isPlain() const { return m_options.mode == FormatMode::plain; }

	template <c_formattible... T> void operator()(const char *format_str, const T &...args) {
		append_(format_str, sizeof...(T), detail::TFFuncs<T...>::funcs,
				detail::getTFValue(args)...);
	}

	void stdFormat(const char *format, ...) ATTRIB_PRINTF(2, 3);

	template <class T>
		requires(is_right_formattible<T>)
	TextFormatter &operator<<(const T &rhs) {
		rhs >> *this;
		return *this;
	}

	template <c_span TSpan, class T = SpanBase<TSpan>>
		requires(is_formattible<T>)
	TextFormatter &operator<<(const TSpan &span) {
		detail::TFFunc func = [](TextFormatter &fmt, unsigned long long ptr) {
			const T &ref = *(const T *)ptr;
			auto func = detail::getTFFunc<T>();
			return func(fmt, detail::getTFValue(ref));
		};
		detail::formatSpan(*this, (const char *)fwk::data(span), fwk::size(span), sizeof(T), func);
		return *this;
	}

	template <c_range TRange, class T = RangeBase<TRange>>
		requires(is_formattible<T> && !is_span<TRange> && !is_right_formattible<TRange>)
	TextFormatter &operator<<(const TRange &range) {
		const char *separator = isStructured() ? ", " : " ";

		if(isStructured())
			(*this) << '[';
		auto it = begin(range), it_end = end(range);
		while(it != it_end) {
			(*this) << *it;
			++it;
			if(it != it_end)
				(*this) << separator;
		}
		if(isStructured())
			(*this) << ']';
		return *this;
	}

	template <c_vec TVec>
		requires(!is_right_formattible<TVec>)
	TextFormatter &operator<<(const TVec &vec) {
		enum { N = TVec::vec_size };
		if constexpr(N == 2)
			(*this)(isStructured() ? "(%, %)" : "% %", vec[0], vec[1]);
		else if(N == 3)
			(*this)(isStructured() ? "(%, %, %)" : "% % %", vec[0], vec[1], vec[2]);
		else if(N == 4)
			(*this)(isStructured() ? "(%, %, %, %)" : "% % % %", vec[0], vec[1], vec[2], vec[3]);
		else
			static_assert((N >= 2 && N <= 4), "Vector size not supported");
		return *this;
	}

	template <class T> TextFormatter &operator<<(const Box<T> &box) {
		(*this)(isStructured() ? "(%; %)" : "% %", box.min(), box.max());
		return *this;
	}

	template <class T1, class T2> TextFormatter &operator<<(const Pair<T1, T2> &pair) {
		(*this)(isStructured() ? "(%; %)" : "% %", pair.first, pair.second);
		return *this;
	}

	template <class... Args> TextFormatter &operator<<(const LightTuple<Args...> &tuple) {
		if(isStructured())
			*this << '(';
		formatTuple<0>(tuple);
		if(isStructured())
			*this << ')';
		return *this;
	}

	template <class T> TextFormatter &operator<<(const Maybe<T> &maybe) {
		if(maybe)
			*this << *maybe;
		else
			*this << "none";
		return *this;
	}

	template <class T, EnableIfEnum<T>...> TextFormatter &operator<<(T value) {
		return *this << toString(value);
	}

	TextFormatter &operator<<(const None &);

  public:
	using Value = detail::TFValue;
	using Func = detail::TFFunc;

	// Basically TextFormatter is trying to pass arguments to internal functions efficiently:
	// all arguments are passed as unsigned long long; all types which naturally fit in this number
	// are passed by value, all other types are passed by pointer
	void append_(const char *format, int arg_count, const Func *, va_list);
	void append_(const char *format, int arg_count, const Func *, ...);

  private:
	static string strFormat_(const char *format, int arg_count, const Func *, ...);
	static void print_(FormatMode, const char *format, int arg_count, const Func *, ...);
	static string toString_(Func, Value);

	const char *nextElement(const char *format_str);

	template <c_formattible... T> friend string format(const char *str, T &&...args) {
		return strFormat_(str, sizeof...(T), detail::TFFuncs<T...>::funcs,
						  detail::getTFValue(args)...);
	}

	template <c_formattible... T> friend void print(const char *str, T &&...args) {
		print_(FormatMode::structured, str, sizeof...(T), detail::TFFuncs<T...>::funcs,
			   detail::getTFValue(args)...);
	}

	template <c_formattible... T> friend void printPlain(const char *str, T &&...args) {
		print_(FormatMode::plain, str, sizeof...(T), detail::TFFuncs<T...>::funcs,
			   detail::getTFValue(args)...);
	}

	template <c_formattible T>
		requires(!is_enum<RemoveReference<T>>)
	friend string toString(T &&value) {
		return toString_(detail::getTFFunc<T>(), detail::getTFValue(value));
	}

	template <int N, class... Args> void formatTuple(const LightTuple<Args...> &tuple) {
		*this << get<N>(tuple);
		if constexpr(N + 1 < sizeof...(Args)) {
			*this << (isStructured() ? "; " : " ");
			formatTuple<N + 1>(tuple);
		}
	}

	// TODO: It would be better to have string type internally
	PodVector<char> m_data;
	int m_offset;
	FormatOptions m_options;
};

extern template TextFormatter &TextFormatter::operator<<(const vec2<int> &);
extern template TextFormatter &TextFormatter::operator<<(const vec3<int> &);
extern template TextFormatter &TextFormatter::operator<<(const vec4<int> &);

extern template TextFormatter &TextFormatter::operator<<(const vec2<float> &);
extern template TextFormatter &TextFormatter::operator<<(const vec3<float> &);
extern template TextFormatter &TextFormatter::operator<<(const vec4<float> &);

extern template TextFormatter &TextFormatter::operator<<(const vec2<double> &);
extern template TextFormatter &TextFormatter::operator<<(const vec3<double> &);
extern template TextFormatter &TextFormatter::operator<<(const vec4<double> &);

extern template TextFormatter &TextFormatter::operator<<(const Box<int2> &);
extern template TextFormatter &TextFormatter::operator<<(const Box<int3> &);
extern template TextFormatter &TextFormatter::operator<<(const Box<float2> &);
extern template TextFormatter &TextFormatter::operator<<(const Box<float3> &);
extern template TextFormatter &TextFormatter::operator<<(const Box<double2> &);
extern template TextFormatter &TextFormatter::operator<<(const Box<double3> &);

string stdFormat(const char *format, ...) ATTRIB_PRINTF(1, 2);

template <c_formattible T>
	requires(!is_enum<RemoveReference<T>>)
string toString(T &&value);

template <c_formattible... T> string format(const char *str, T &&...);
template <c_formattible... T> void print(const char *str, T &&...);
template <c_formattible... T> void printPlain(const char *str, T &&...);

#define FWK_DUMP(...) fwk::print(fwk::detail::autoPrintFormat(#__VA_ARGS__).c_str(), __VA_ARGS__)
}

#endif
