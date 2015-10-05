/* Copyright (C) 2015 Krzysztof Jakubowski <nadult@fastmail.fm>

   This file is part of libfwk. */

#ifndef FWK_XML_H
#define FWK_XML_H

#include "fwk_base.h"
#include "fwk_math.h"

#ifndef RAPIDXML_HPP_INCLUDED

namespace rapidxml {
template <class Ch> class xml_node;
template <class Ch> class xml_document;
};

#endif

namespace fwk {

namespace xml_conversions {

	template <class T> TextFormatter toString(const T &value);

	namespace detail {

		template <class T> T fromString(TextParser &) {
			static_assert(sizeof(T) < 0,
						  "xml_conversions::fromString unimplemented for given type");
		}

		template <> string fromString<string>(TextParser &);
		template <> bool fromString<bool>(TextParser &);
		template <> int fromString<int>(TextParser &);
		template <> int2 fromString<int2>(TextParser &);
		template <> int3 fromString<int3>(TextParser &);
		template <> int4 fromString<int4>(TextParser &);
		template <> uint fromString<uint>(TextParser &);
		template <> float fromString<float>(TextParser &);
		template <> float2 fromString<float2>(TextParser &);
		template <> float3 fromString<float3>(TextParser &);
		template <> float4 fromString<float4>(TextParser &);
		template <> FRect fromString<FRect>(TextParser &);
		template <> IRect fromString<IRect>(TextParser &);
		template <> FBox fromString<FBox>(TextParser &);
		template <> IBox fromString<IBox>(TextParser &);
		template <> Matrix4 fromString<Matrix4>(TextParser &);
		template <> Quat fromString<Quat>(TextParser &);

		template <class T> vector<T> vectorFromString(TextParser &parser) {
			vector<T> out;
			while(parser.hasAnythingLeft())
				out.emplace_back(fromString<T>(parser));
			return out;
		}

		template <> vector<float> vectorFromString<float>(TextParser &);
		template <> vector<int> vectorFromString<int>(TextParser &);
		template <> vector<uint> vectorFromString<uint>(TextParser &);
		template <> vector<string> vectorFromString<string>(TextParser &);

		template <class T> struct SelectParser {
			static T parse(TextParser &parser) { return fromString<T>(parser); }
		};

		template <class T> struct SelectParser<vector<T>> {
			static vector<T> parse(TextParser &parser) { return vectorFromString<T>(parser); }
		};

		template <class T> void toString(const T &value, TextFormatter &out) {
			static_assert(sizeof(T) < 0, "xml_conversions::toString unimplemented for given type");
		}

		template <> void toString(const string &value, TextFormatter &out);
		template <> void toString(const bool &value, TextFormatter &out);
		template <> void toString(const int &value, TextFormatter &out);
		template <> void toString(const int2 &value, TextFormatter &out);
		template <> void toString(const int3 &value, TextFormatter &out);
		template <> void toString(const int4 &value, TextFormatter &out);
		template <> void toString(const uint &value, TextFormatter &out);
		template <> void toString(const long long &value, TextFormatter &out);
		template <> void toString(const float &value, TextFormatter &out);
		template <> void toString(const float2 &value, TextFormatter &out);
		template <> void toString(const float3 &value, TextFormatter &out);
		template <> void toString(const float4 &value, TextFormatter &out);
		template <> void toString(const FRect &value, TextFormatter &out);
		template <> void toString(const IRect &value, TextFormatter &out);
		template <> void toString(const FBox &value, TextFormatter &out);
		template <> void toString(const IBox &value, TextFormatter &out);
		template <> void toString(const Matrix4 &value, TextFormatter &out);
		template <> void toString(const Quat &value, TextFormatter &out);

		template <class T> void vectorToString(const vector<T> &array, TextFormatter &out) {
			for(int n = 0; n < (int)array.size(); n++) {
				toString(array[n], out);
				if(n + 1 < (int)array.size())
					out(" ");
			}
		}

		template <class T> struct SelectPrinter {
			static void print(const T &value, TextFormatter &out) {
				return toString<T>(value, out);
			}
		};

		template <class T> struct SelectPrinter<vector<T>> {
			static void print(const vector<T> &value, TextFormatter &out) {
				return vectorToString<T>(value, out);
			}
		};
	}

	template <class T> T fromString(TextParser &parser) {
		return detail::SelectParser<T>::parse(parser);
	}

	template <class T> T fromString(const char *input) {
		TextParser parser(input);
		return detail::SelectParser<T>::parse(parser);
	}

	template <class T> T fromString(const string &input) {
		TextParser parser(input.c_str());
		return detail::SelectParser<T>::parse(parser);
	}

	template <class T> void toString(const T &value, TextFormatter &out) {
		detail::SelectPrinter<T>::print(value, out);
	}

	template <class T> TextFormatter toString(const T &value) {
		TextFormatter out;
		detail::SelectPrinter<T>::print(value, out);
		return out;
	}
}

template <class... Args> string xmlFormat(const char *format, const Args &... args) {
	vector<string> strings = {xml_conversions::toString(args).text()...};
	return simpleFormat(format, strings);
}

template <class... Args> void xmlPrint(const char *format, Args &&... args) {
	printf("%s", xmlFormat(format, std::forward<Args>(args)...).c_str());
}

class XMLNode {
  public:
	XMLNode(const XMLNode &rhs) : m_ptr(rhs.m_ptr), m_doc(rhs.m_doc) {}
	XMLNode() : m_ptr(nullptr), m_doc(nullptr) {}

	// TODO: change to tryAttrib?
	// Returns nullptr if not found
	const char *hasAttrib(const char *name) const;

	// If an attribute cannot be found or properly parsed then exception is thrown
	const char *attrib(const char *name) const;
	// If an attribute cannot be found then default_value will be returned
	const char *attrib(const char *name, const char *default_value) const;

	template <class T> T attrib(const char *name) const {
		try {
			return xml_conversions::fromString<T>(attrib(name));
		} catch(const Exception &ex) {
			parsingError(name, ex.what());
			return T();
		}
	}

	template <class T> T attrib(const char *name, T default_value) const {
		const char *value = hasAttrib(name);
		return value ? xml_conversions::fromString<T>(value) : default_value;
	}

	// When adding new nodes, you have to make sure that strings given as
	// arguments will exist as long as XMLNode exists; use 'own' method
	// to reallocate them in the memory pool if you're not sure
	void addAttrib(const char *name, const char *value);
	void addAttrib(const char *name, int value);

	template <class T> void addAttrib(const char *name, const T &value) {
		TextFormatter formatter;
		xml_conversions::toString(value, formatter);
		addAttrib(name, own(formatter.text()));
	}

	XMLNode addChild(const char *name, const char *value = nullptr);
	XMLNode sibling(const char *name = nullptr) const;
	XMLNode child(const char *name = nullptr) const;

	template <class T> XMLNode addChild(const char *name, const T &value) {
		TextFormatter formatter;
		xml_conversions::toString(value, formatter);
		return addChild(name, own(formatter.text()));
	}

	void next() { *this = sibling(name()); }

	const char *value() const;

	template <class T> T value() const {
		try {
			return xml_conversions::fromString<T>(value());
		} catch(const Exception &ex) {
			parsingError(nullptr, ex.what());
			return T();
		}
	}
	template <class T> T value(T default_value) const {
		const char *val = value();
		return val[0] ? value<T>() : default_value;
	}
	template <class T> T childValue(const char *child_name, T default_value) const {
		XMLNode child_node = child(child_name);
		const char *val = child_node ? child_node.value() : "";
		return val[0] ? child_node.value<T>() : default_value;
	}

	const char *name() const;

	const char *own(const char *str);
	const char *own(const string &str) { return own(str.c_str()); }
	const char *own(const TextFormatter &str) { return own(str.text()); }
	explicit operator bool() const { return m_ptr != nullptr; }

  protected:
	XMLNode(rapidxml::xml_node<char> *ptr, rapidxml::xml_document<char> *doc)
		: m_ptr(ptr), m_doc(doc) {}
	void parsingError(const char *attrib_name, const char *error_message) const;
	friend class XMLDocument;

	rapidxml::xml_node<char> *m_ptr;
	rapidxml::xml_document<char> *m_doc;
};

class XMLDocument {
  public:
	XMLDocument();
	XMLDocument(const XMLDocument &) = delete;
	XMLDocument(XMLDocument &&);
	~XMLDocument();
	void operator=(XMLDocument &&);
	void operator=(const XMLDocument &) = delete;

	void load(const char *file_name);
	void save(const char *file_name) const;

	void load(Stream &);
	void save(Stream &) const;

	XMLNode addChild(const char *name, const char *value = nullptr) const;
	XMLNode child(const char *name = nullptr) const;
	XMLNode child(const string &name) const { return child(name.c_str()); }

	const char *own(const char *str);
	const char *own(const string &str) { return own(str.c_str()); }
	const char *own(const TextFormatter &str) { return own(str.text()); }

  protected:
	unique_ptr<rapidxml::xml_document<char>> m_ptr;
};
}

#endif
