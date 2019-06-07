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
	for(auto &elem : m_elements)
		if(elem.second.xmlEnabled()) {
			auto sub_node = node.addChild(node.own(elem.first));
			elem.second.save(sub_node);
		}
}

const Any *AnyConfig::get(Str name) const {
	auto it = m_elements.find(name);
	return it == m_elements.end() ? nullptr : &it->second;
}

const AnyConfig *AnyConfig::subConfig(Str name) const { return get<AnyConfig>(name); }

void AnyConfig::set(string name, Any value) { m_elements[name] = move(value); }

vector<string> AnyConfig::keys() const {
	vector<string> out;
	out.reserve(m_elements.size());
	for(auto &elem : m_elements)
		out.emplace_back(elem.first);
	return out;
}

void AnyConfig::printErrors() const {
	for(auto &[name, err] : m_loading_errors) {
		print("Error while loading AnyConfig element: %\n", name);
		err.print();
		print("\n");
	}
}

}
