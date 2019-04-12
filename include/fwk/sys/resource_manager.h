// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

#pragma once

#include "fwk/sys/expected.h"
#include "fwk/sys/file_stream.h"
#include "fwk/sys/immutable_ptr.h"

#include <map>

namespace fwk {

template <class T> class ResourceLoader {
  public:
	ResourceLoader(const string &file_prefix, const string &file_suffix)
		: m_file_prefix(file_prefix), m_file_suffix(file_suffix) {}

	immutable_ptr<T> operator()(const string &name) const {
		// TODO: fix it
		auto file = fileLoader(name);
		if(!file)
			FWK_FATAL("Cannot load resource '%s':\n%s", name.c_str(),
					  format("%", file.error()).c_str());
		return make_immutable<T>(name, *file);
	}

	string fileName(const string &name) const { return m_file_prefix + name + m_file_suffix; }
	const string &filePrefix() const { return m_file_prefix; }
	const string &fileSuffix() const { return m_file_suffix; }

  protected:
	string m_file_prefix, m_file_suffix;
};

template <class T, class Constructor> class ResourceManager {
  public:
	using PResource = immutable_ptr<T>;

	template <class... ConstructorArgs>
	ResourceManager(ConstructorArgs &&... args)
		: m_constructor(std::forward<ConstructorArgs>(args)...) {}
	ResourceManager() {}
	~ResourceManager() {}

	const Constructor &constructor() const { return m_constructor; }

	PResource accessResource(const string &name) {
		auto it = m_dict.find(name);
		if(it == m_dict.end()) {
			PResource res = m_constructor(name);
			DASSERT(res);
			m_dict[name] = res;
			return res;
		}
		return it->second;
	}

	PResource findResource(const string &name) const {
		auto it = m_dict.find(name);
		if(it == m_dict.end())
			return PResource();
		return it->second;
	}

	PResource operator[](const string &name) { return accessResource(name); }

	const auto &dict() const { return m_dict; }

	PResource removeResource(const string &name) {
		auto iter = m_dict.find(name);
		if(iter != m_dict.end()) {
			m_dict.erase(iter);
			return iter.second;
		}
		return PResource();
	}

	void insertResource(const string &name, PResource res) { m_dict[name] = res; }

	void renameResource(const string &old_name, const string &new_name) {
		insertResource(new_name, move(removeResource(old_name)));
	}

	using Iterator = typename std::map<string, PResource>::const_iterator;

	auto begin() const { return std::begin(m_dict); }
	auto end() const { return std::end(m_dict); }

	void clear() { m_dict.clear(); }

  private:
	typename std::map<string, PResource> m_dict;
	Constructor m_constructor;
	string m_prefix, m_suffix;
};
}
