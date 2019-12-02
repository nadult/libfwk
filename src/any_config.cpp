#include "fwk/hash_map.h"

#include "fwk/any_config.h"

#include "fwk/sys/expected.h"
#include "fwk/sys/xml.h"

namespace fwk {

AnyConfig::AnyConfig() = default;
FWK_COPYABLE_CLASS_IMPL(AnyConfig);

Ex<AnyConfig> AnyConfig::load(CXmlNode node, bool ignore_errors) {
	AnyConfig out;

	if(node) {
		auto sub_node = node.child();
		while(sub_node) {
			ZStr name = sub_node.name();
			auto value = Any::load(sub_node);
			if(value)
				out.m_elements.emplace(name, move(*value));
			else
				out.m_loading_errors.emplace_back(name, value.error());
			sub_node = sub_node.sibling();
		}
	}

	if(!ignore_errors && out.m_loading_errors) {
		vector<Error> errors = transform(out.m_loading_errors, [](auto p) { return p.second; });
		return Error::merge(move(errors));
	}

	return out;
}

void AnyConfig::save(XmlNode node) const {
	for(auto &[key, value] : m_elements)
		if(value.xmlEnabled())
			value.save(node.addChild(node.own(key)));
}

const Any *AnyConfig::get(Str name) const {
	auto it = m_elements.find(name);
	return it ? &it->value : nullptr;
}

const AnyConfig *AnyConfig::subConfig(Str name) const { return get<AnyConfig>(name); }

void AnyConfig::set(string name, Any value) { m_elements[name] = move(value); }

vector<string> AnyConfig::keys() const { return m_elements.keys(); }

void AnyConfig::printErrors() const {
	for(auto &[name, err] : m_loading_errors) {
		print("Error while loading AnyConfig element: %\n", name);
		err.print();
		print("\n");
	}
}

}
