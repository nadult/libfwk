#pragma once

#include "fwk/any.h"
#include "fwk/fwd_member.h"
#include "fwk/str.h"

namespace fwk {

// You can keep any kind of values here, under different names
// Same names should identify values of same type (it's checked)
// Values serializable to/from XML will be serialized with save/load methods
class AnyConfig {
  public:
	using Element = pair<string, Any>;

	AnyConfig();
	FWK_COPYABLE_CLASS(AnyConfig);

	static Ex<AnyConfig> load(CXmlNode, bool ignore_errors = false);
	void save(XmlNode) const;

	const Any *get(Str) const;
	const AnyConfig *subConfig(Str) const;

	void set(string, Any);

	template <class T, class RT = Decay<T>> void set(string name, const T &value) {
		set(name, Any(RT(value)));
	}
	template <class T, class RT = Decay<T>>
	void set(string name, const T &value, const T &default_value) {
		if(value != default_value)
			set(name, Any(RT(value)));
	}
	vector<string> keys() const;

	template <class T> const T *get(string name) const {
		if(auto *any = get(name); any && any->is<T>())
			return &any->get<T>();
		return nullptr;
	}

	template <class T> T get(string name, const T &default_value) const {
		if(auto *any = get(name); any && any->is<T>())
			return any->get<T>();
		return default_value;
	}

	CSpan<Pair<string, Error>> loadingErrors() const { return m_loading_errors; }
	void printErrors() const;

  private:
	FwdMember<HashMap<string, Any>> m_elements;
	vector<Pair<string, Error>> m_loading_errors;
};
}
