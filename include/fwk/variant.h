// Copyright (C) Krzysztof Jakubowski <nadult@fastmail.fm>
// This file is part of libfwk. See license.txt for details.

// fwk::Variant evolved from MapBox::variant
// Licensed under BSD license (available in extern/mapbox_license.txt)

#pragma once

#include "fwk/io/xml.h"
#include "fwk/sys/error.h"
#include "fwk/sys_base.h"
#include "fwk/type_info_gen.h"
#include <new>
#include <type_traits>
#include <utility>

namespace fwk {
namespace detail {
	// References are stored as pointers
	template <class T> struct VariantStoredType {
		using Type = T;
	};
	template <class T> struct VariantStoredType<T &> {
		using Type = T *;
	};

	template <typename T, typename R = void> struct enable_if_type {
		using type = R;
	};
	template <typename F, typename V, typename Enable = void> struct result_of_unary_visit {
		using type = typename std::invoke_result<F, V &>::type;
	};

	template <typename F, typename V>
	struct result_of_unary_visit<F, V, typename enable_if_type<typename F::result_type>::type> {
		using type = typename F::result_type;
	};

	template <typename... Types> struct variant_helper;

	template <typename T, typename... Types> struct variant_helper<T, Types...> {
		using ST = typename VariantStoredType<T>::Type;

		static void destroy(int type_index, void *data) {
			if(type_index == 0)
				reinterpret_cast<ST *>(data)->~ST();
			else
				variant_helper<Types...>::destroy(type_index - 1, data);
		}

		static void move(int old_type_index, void *old_value, void *new_value) {
			if(old_type_index == 0)
				new(new_value) ST(std::move(*reinterpret_cast<ST *>(old_value)));
			else
				variant_helper<Types...>::move(old_type_index - 1, old_value, new_value);
		}

		static void copy(int old_type_index, const void *old_value, void *new_value) {
			if(old_type_index == 0)
				new(new_value) ST(*reinterpret_cast<const ST *>(old_value));
			else
				variant_helper<Types...>::copy(old_type_index - 1, old_value, new_value);
		}
	};

	template <> struct variant_helper<> {
		static void destroy(int, void *) {}
		static void move(int, void *, void *) {}
		static void copy(int, const void *, void *) {}
	};

	template <typename F, typename V, typename R, typename... Types> struct dispatcher;

	template <typename F, typename V, typename R, typename T, typename... Types>
	struct dispatcher<F, V, R, T, Types...> {
		static R apply_const(V const &v, F &&f) {
			if(v.template is<T>())
				return f(v.template get<T>());
			else
				return dispatcher<F, V, R, Types...>::apply_const(v, std::forward<F>(f));
		}

		static R apply(V &v, F &&f) {
			if(v.template is<T>())
				return f(v.template get<T>());
			else
				return dispatcher<F, V, R, Types...>::apply(v, std::forward<F>(f));
		}
	};

	template <typename F, typename V, typename R, typename T> struct dispatcher<F, V, R, T> {
		static R apply_const(V const &v, F &&f) { return f(v.template get<T>()); }
		static R apply(V &v, F &&f) { return f(v.template get<T>()); }
	};

	// comparator functors
	struct equal_comp {
		template <typename T> bool operator()(T const &lhs, T const &rhs) const {
			return lhs == rhs;
		}
	};

	struct less_comp {
		template <typename T> bool operator()(T const &lhs, T const &rhs) const {
			return lhs < rhs;
		}
	};

	template <typename Variant, typename Comp> class comparer {
	  public:
		explicit comparer(const Variant &lhs) : lhs_(lhs) {}
		comparer &operator=(comparer const &) = delete;
		// visitor
		template <typename T> bool operator()(T const &rhs_content) const {
			T const &lhs_content = lhs_.template get<T>();
			return Comp()(lhs_content, rhs_content);
		}

	  private:
		const Variant &lhs_;
	};

	template <typename... Fns> struct visitor;

	template <typename Fn> struct visitor<Fn> : Fn {
		using type = Fn;
		using Fn::operator();

		visitor(Fn fn) : Fn(fn) {}
	};

	template <typename Fn, typename... Fns> struct visitor<Fn, Fns...> : Fn, visitor<Fns...> {
		using type = visitor;
		using Fn::operator();
		using visitor<Fns...>::operator();

		visitor(Fn fn, Fns... fns) : Fn(fn), visitor<Fns...>(fns...) {}
	};

	template <typename... Fns> visitor<Fns...> make_visitor(Fns... fns) {
		return visitor<Fns...>(fns...);
	}
}

struct TypeNotInVariant;

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif

template <class T> static constexpr bool is_variant = false;
template <class... Types> static constexpr bool is_variant<Variant<Types...>> = true;

template <typename... Types> class Variant {
	static_assert(sizeof...(Types) > 0, "Template parameter type list of variant can not be empty");
	static_assert(sizeof...(Types) < 128, "Please, keep it reasonable");
	// TODO: check that types are distinct (after references are dropped?)

	template <class T> using StoredType = typename detail::VariantStoredType<T>::Type;
	static constexpr int data_size = max(0, 0, (int)sizeof(StoredType<Types>)...);
	static constexpr int data_align = max(0, 0, (int)alignof(StoredType<Types>)...);
	template <class T> static constexpr int type_index_ = fwk::type_index<T, Types...>;
	template <class T> using EnableIfValidType = EnableIf<(type_index_<T> != -1), TypeNotInVariant>;

	using First = NthType<0, Types...>;
	using Helper = detail::variant_helper<Types...>;

#ifdef NDEBUG
	static constexpr bool ndebug_access = true;
#else
	static constexpr bool ndebug_access = false;
#endif

	alignas(data_align) char data[data_size];
	char type_index;

	template <class T> T &access() const {
		if constexpr(is_reference<T>)
			return **(StoredType<T> *)(&data);
		else
			return *(T *)(&data);
	}

  public:
	Variant() : type_index(0) {
		static_assert(std::is_default_constructible<First>::value,
					  "First type in variant must be default constructible to allow default "
					  "construction of variant");
		new(&data) First();
	}

	template <class T, class DT = Decay<T>, int index = type_index_<DT>, EnableIf<index != -1>...>
	Variant(T &&val) : type_index(index) {
		new(&data) DT(std::forward<T>(val));
	}

	// Initializing reference type
	template <class T,
			  int index = (type_index_<T &> == -1 ? type_index_<const T &> : type_index_<T &>),
			  EnableIf<index != -1>...>
	Variant(T &val) : type_index(index) {
		new (&data)(T *)(&val);
	}

	Variant(const Variant &old) : type_index(old.type_index) {
		Helper::copy(old.type_index, &old.data, &data);
	}

	Variant(Variant &&old) : type_index(old.type_index) {
		Helper::move(old.type_index, &old.data, &data);
	}

	~Variant() { Helper::destroy(type_index, &data); }

	Variant &operator=(const Variant &rhs) {
		Helper::destroy(type_index, &data);
		type_index = -1;
		Helper::copy(rhs.type_index, &rhs.data, &data);
		type_index = rhs.type_index;
		return *this;
	}

	Variant &operator=(Variant &&rhs) {
		Helper::destroy(type_index, &data);
		type_index = -1;
		Helper::move(rhs.type_index, &rhs.data, &data);
		type_index = rhs.type_index;
		return *this;
	}

	template <typename T, class DT = Decay<T>, int index = type_index_<DT>,
			  EnableIf<index != -1>...>
	void operator=(T &&rhs) {
		if constexpr(std::is_move_assignable<T>::value)
			if(index == type_index) {
				reinterpret_cast<DT &>(data) = std::move(rhs);
				return;
			}

		Helper::destroy(type_index, &data);
		type_index = index;
		new(&data) DT(std::move(rhs));
	}

	template <typename T, int index = type_index_<T>, EnableIf<index != -1>...>
	void operator=(const T &rhs) {
		if constexpr(std::is_move_assignable<T>::value)
			if(index == type_index) {
				reinterpret_cast<T &>(data) = rhs;
				return;
			}

		Helper::destroy(type_index, &data);
		type_index = index;
		new(&data) T(rhs);
	}

	// Assigning reference type
	template <typename T,
			  int index = (type_index_<T &> == -1 ? type_index_<const T &> : type_index_<T &>),
			  EnableIf<index != -1>...>
	void operator=(T &rhs) {
		Helper::destroy(type_index, &data);
		type_index = index;
		new (&data)(T *)(&rhs);
	}

	template <typename T> bool is() const {
		static_assert(is_one_of<T, Types...>, "invalid type in T in `is<T>()` for this variant");
		return type_index == type_index_<T>;
	}

	template <typename T, typename... Args> void set(Args &&...args) {
		Helper::destroy(type_index, &data);
		new(&data) T(std::forward<Args>(args)...);
		type_index = type_index_<T>;
	}

	template <typename T, EnableIfValidType<T>...> T &get() & {
		if(type_index == type_index_<T> || ndebug_access)
			return access<T>();
		else
			FWK_FATAL("Bad variant access in get()");
	}

	template <typename T, EnableIfValidType<T>...> T const &get() const & {
		if(type_index == type_index_<T> || ndebug_access)
			return access<const T>();
		else
			FWK_FATAL("Bad variant access in get()");
	}

	template <typename T, EnableIfValidType<T>...> operator T *() & {
		return type_index == type_index_<T> ? &access<T>() : nullptr;
	}

	template <typename T, EnableIfValidType<T>...> operator const T *() const & {
		return type_index == type_index_<T> ? &access<const T>() : nullptr;
	}

	// Accessing references
	template <typename T, EnableIfValidType<T &>...> operator T *() & {
		return type_index == type_index_<T &> ? &access<T &>() : nullptr;
	}

	template <typename T, EnableIfValidType<T &>...> operator const T *() const & {
		return type_index == type_index_<T &> ? &access<const T &>() : nullptr;
	}

	template <typename T, EnableIfValidType<T>...> T &get() && = delete;
	template <typename T, EnableIfValidType<T>...> T const &get() const && = delete;

	template <typename T, EnableIfValidType<T>...> operator T *() && = delete;
	template <typename T, EnableIfValidType<T>...> operator const T *() const && = delete;

	int which() const { return type_index; }

	template <typename F, typename R = typename detail::result_of_unary_visit<F, First>::type>
	auto visit(F &&f) const {
		return detail::dispatcher<F, Variant, R, Types...>::apply_const(*this, std::forward<F>(f));
	}
	template <typename F, typename R = typename detail::result_of_unary_visit<F, First>::type>
	auto visit(F &&f) {
		return detail::dispatcher<F, Variant, R, Types...>::apply(*this, std::forward<F>(f));
	}

	template <typename... Fs> auto match(Fs &&...fs) const {
		return visit(detail::make_visitor(std::forward<Fs>(fs)...));
	}
	template <typename... Fs> auto match(Fs &&...fs) {
		return visit(detail::make_visitor(std::forward<Fs>(fs)...));
	}

	bool operator==(const Variant &rhs) const {
		if(this->type_index != rhs.type_index)
			return false;
		detail::comparer<Variant, detail::equal_comp> visitor(*this);
		return rhs.visit(visitor);
	}

	bool operator<(const Variant &rhs) const {
		if(this->type_index != rhs.type_index)
			return this->type_index < rhs.type_index;
		detail::comparer<Variant, detail::less_comp> visitor(*this);
		return rhs.visit(visitor);
	}

	template <auto v> explicit Variant(Intrusive::Tag<v>) : type_index(-1 - int(v)) {}
	template <auto v> bool operator==(Intrusive::Tag<v>) const { return type_index == -1 - int(v); }
};

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

template <typename ResultType, typename T> ResultType &get(T &var) {
	return var.template get<ResultType>();
}

template <typename ResultType, typename T> ResultType const &get(T const &var) {
	return var.template get<ResultType>();
}

namespace detail {
	template <class T, class... Types> struct VariantC {
		static Ex<T> load_(ZStr type_name, CXmlNode node) {
			return FWK_ERROR("Invalid type_name: '%' when constructing variant", type_name);
		}
		static void save_(const T &variant, XmlNode node) {}
	};

	template <class T, class T1, class... Types> struct VariantC<T, T1, Types...> {
		static FWK_ALWAYS_INLINE Ex<T> load_(ZStr type_name, CXmlNode node) {
			static_assert(!is_one_of<Error, Types...>);
			if(type_name == typeName<T1>()) {
				auto result = load<T1>(node);
				if(result)
					return T(*result);
				else
					return result.error();
			}
			return VariantC<T, Types...>::load_(type_name, node);
		}
		static void save_(const T &variant, XmlNode node) {
			if(variant.template is<T1>()) {
				node.addAttrib("_variant_type_id", typeName<T1>());
				const T1 *value = variant;
				save(node, *value);
			} else {
				VariantC<T, Types...>::save_(variant, node);
			}
		}
	};

	template <class T> struct VariantCExp {};
	template <class... Types>
	struct VariantCExp<Variant<Types...>> : public VariantC<Variant<Types...>, Types...> {};
}

template <class... Types, class Enable = EnableIf<(... && is_xml_loadable<Types>)>>
Ex<Variant<Types...>> load(CXmlNode node, Type<Variant<Types...>>) EXCEPT {
	return detail::VariantCExp<Variant<Types...>>::load_(node.attrib("_variant_type_id"), node);
}

template <class... Types, class Enable = EnableIf<(... && is_xml_saveable<Types>)>>
void save(XmlNode node, const Variant<Types...> &value) {
	detail::VariantCExp<Variant<Types...>>::save_(value, node);
}

template <class... Types, class Enable = EnableIf<(... && is_formattible<Types>)>>
TextFormatter &operator<<(TextFormatter &out, const Variant<Types...> &variant) {
	variant.visit([&](const auto &value) { out << value; });
	return out;
}
}
