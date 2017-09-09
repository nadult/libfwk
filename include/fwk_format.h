// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#ifndef FWK_FORMAT_H
#define FWK_FORMAT_H

#include "fwk_base.h"
#include "fwk_math.h"

namespace fwk {

DEFINE_ENUM(FormatMode, plain, structured);
DEFINE_ENUM(FormatPrecision, adaptive, maximum);

DEFINE_ENUM(FormatGroup, range, vector, structure);

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

// TODO: better name
class TextFormatter {
  public:
	explicit TextFormatter(int initial_size = 256, FormatOptions = {});
	TextFormatter(const TextFormatter &);
	~TextFormatter();

#ifdef __clang__
	__attribute__((__format__(__printf__, 2, 3)))
#endif
	void
	operator()(const char *format, ...);

	void append(StringRef);
	void append(const char *str) { append(StringRef(str)); }
	void append(double);
	void append(float);
	void append(int);
	void append(uint);
	void append(long);
	void append(unsigned long);
	void append(long long);
	void append(unsigned long long);
	void append(bool);

	void reserve(int) NOINLINE;

	void trim(int count);

	const char *c_str() const { return m_data.data(); }
	StringRef text() const { return {m_data.data(), m_offset}; }
	int length() const { return m_offset; }

	const FormatOptions &options() const { return m_options; }
	FormatOptions &options() { return m_options; }

	bool isStructured() const { return m_options.mode == FormatMode::structured; }
	bool isPlain() const { return m_options.mode == FormatMode::plain; }

	void openGroup(FormatGroup);
	void closeGroup(FormatGroup);

	// TODO: better naming
	template <class... Args> void doFormat(const char *format_str, Args &&...);

	template <class... Args> void appendFmt(const char *format_str, Args &&... args) {
		while(true) {
			int ret = snprintf(&m_data[m_offset], m_data.size() - m_offset, format_str,
							   std::forward<Args>(args)...);
			if(ret < 0)
				FATAL("Error while snprintfing");

			int new_offset = ret + m_offset;
			if((new_offset + 1 <= m_data.size())) {
				m_offset = new_offset;
				return;
			}

			reserve(new_offset + 1);
		}
	}

  private:
	static void checkArgumentCount(const char *format_str, int num_arg);
	const char *nextElement(const char *format_str);

	template <class Arg> void processArgument(const char *&format_str, Arg &&arg);

	PodArray<char> m_data;
	int m_offset;
	FormatOptions m_options;
};

// Czy te funkcje są odkryte czy nie?
// Czy robie na f-ptrach ? PO CO?
// Czy martwię się wydajnością? NIE, to co mamy jest wystaczające

// TODO: te funkcje powinny sie nazywac inaczej...

void format(TextFormatter &, StringRef);
void format(TextFormatter &, const char *);
void format(TextFormatter &, const string &);

void format(TextFormatter &, bool);
void format(TextFormatter &, int);
void format(TextFormatter &, uint);
void format(TextFormatter &, unsigned long);
void format(TextFormatter &, long long);
void format(TextFormatter &, unsigned long long);

void format(TextFormatter &, const int2 &);
void format(TextFormatter &, const int3 &);
void format(TextFormatter &, const int4 &);

void format(TextFormatter &, double);
void format(TextFormatter &, const double2 &);
void format(TextFormatter &, const double3 &);
void format(TextFormatter &, const double4 &);

void format(TextFormatter &, float);
void format(TextFormatter &, const float2 &);
void format(TextFormatter &, const float3 &);
void format(TextFormatter &, const float4 &);

void format(TextFormatter &, const DRect &);
void format(TextFormatter &, const FRect &);
void format(TextFormatter &, const IRect &);

void format(TextFormatter &, const FBox &);
void format(TextFormatter &, const IBox &);
void format(TextFormatter &, const DBox &);

void format(TextFormatter &, const Matrix4 &);
void format(TextFormatter &, const Quat &);

struct NotFormattible;

template <class T> struct Formattible {
	template <class U>
	static auto test(const U &)
		-> decltype(format(std::declval<TextFormatter &>(), std::declval<const U &>()));
	template <class U> static char test(...);
	enum { value = !std::is_same<decltype(test<T>(std::declval<const T &>())), char>::value };
};

template <class T> using EnableIfFormattible = EnableIf<Formattible<T>::value, NotFormattible>;

template <class TRange, class T = RangeBase<TRange>, EnableIfFormattible<T>...>
void format(TextFormatter &out, const TRange &range) {
	bool is_structured = out.options().mode == FormatMode::structured;

	const char *separator = is_structured ? ", " : " ";

	out.openGroup(FormatGroup::range);
	auto it = begin(range), it_end = end(range);
	while(it != it_end) {
		format(out, *it);
		++it;
		if(it != it_end)
			out.append(separator);
	}
	out.closeGroup(FormatGroup::range);
}

template <class TVector, EnableIfVector<TVector>...>
void format(TextFormatter &out, const TVector &vec) {
	enum { N = TVector::vector_size };
	if
		constexpr(N == 2) out.doFormat(out.isStructured() ? "(%, %)" : "% %", vec[0], vec[1]);
	else if(N == 3)
		out.doFormat(out.isStructured() ? "(%, %, %)" : "% % %", vec[0], vec[1], vec[2]);
	else if(N == 4)
		out.doFormat(out.isStructured() ? "(%, %, %, %)" : "% % % %", vec[0], vec[1], vec[2],
					 vec[3]);
	else
		static_assert((N >= 2 && N <= 4), "Vector size not supported");
}

template <class T> void format(TextFormatter &out, const Box<T> &box) {
	out.doFormat(out.isStructured() ? "(%; %)" : "% %", box.min(), box.max());
}

// TODO: default formatting mode ???
template <class T, EnableIf<!isEnum<T>()>...> string toString(T value) {
	TextFormatter out;
	format(out, std::forward<T>(value));
	return out.text();
}

#ifdef __clang__
__attribute__((__format__(__printf__, 1, 2)))
#endif
string
stdFormat(const char *format, ...);

template <class... Args> string format(const char *str, Args &&... args) {
	TextFormatter out;
	out.doFormat(str, std::forward<Args>(args)...);
	return out.text();
}

template <class... Args> void print(const char *str, Args &&... args) {
	TextFormatter out(1024, {FormatMode::structured});
	out.doFormat(str, std::forward<Args>(args)...);
	fputs(out.c_str(), stdout);
}

template <class... Args> void printPlain(const char *str, Args &&... args) {
	TextFormatter out(1024);
	out.doFormat(str, std::forward<Args>(args)...);
	fputs(out.c_str(), stdout);
}

// TODO: escapable % ?
template <class... Args> void TextFormatter::doFormat(const char *format_str, Args &&... args) {
	constexpr int num_arguments = ((isFormatType<Args>() ? 0 : 1) + ... + 0);
#ifndef NDEBUG
	checkArgumentCount(format_str, num_arguments);
#endif

	(..., processArgument<Args>(format_str, std::forward<Args>(args)));
	append(format_str);
}

template <class Arg> void TextFormatter::processArgument(const char *&format_str, Arg &&arg) {
	if
		constexpr(isFormatType<Arg>()) { m_options.set(std::forward<Arg>(arg)); }
	else {
		format_str = nextElement(format_str);
		format(*this, std::forward<Arg>(arg));
	}
}
}

#endif
