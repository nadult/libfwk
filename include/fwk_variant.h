/*
Copyright (c) MapBox
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.
- Neither the name "MapBox" nor the names of its contributors may be
  used to endorse or promote products derived from this software without
  specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Some modifications by Krzysztof 'nadult' Jakubowski

#pragma once
// clang-format off

#include <new> // operator new
#include <string>
#include <tuple>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include "fwk_base.h"

#ifdef _MSC_VER
 // https://msdn.microsoft.com/en-us/library/bw1hbe6y.aspx
 #ifdef NDEBUG
  #define VARIANT_INLINE __forceinline
 #else
  #define VARIANT_INLINE inline
 #endif
#else
 #ifdef NDEBUG
  #define VARIANT_INLINE inline __attribute__((always_inline))
 #else
  #define VARIANT_INLINE inline
 #endif
#endif

#define VARIANT_MAJOR_VERSION 0
#define VARIANT_MINOR_VERSION 1
#define VARIANT_PATCH_VERSION 0

#define VARIANT_VERSION (VARIANT_MAJOR_VERSION*100000) + (VARIANT_MINOR_VERSION*100) + (VARIANT_PATCH_VERSION)


namespace fwk {

namespace detail {

template <typename T, typename... Types>
struct direct_type;

template <typename T, typename First, typename... Types>
struct direct_type<T, First, Types...>
{
    static constexpr int index = std::is_same<T, First>::value
        ? (int)sizeof...(Types) : direct_type<T, Types...>::index;
};

template <typename T>
struct direct_type<T>
{
    static constexpr int index = -1;
};

template <typename T, typename... Types>
struct convertible_type;

template <typename T, typename First, typename... Types>
struct convertible_type<T, First, Types...>
{
    static constexpr int index = std::is_convertible<T, First>::value
        ? (int)sizeof...(Types) : convertible_type<T, Types...>::index;
};

template <typename T>
struct convertible_type<T>
{
    static constexpr int index = -1;
};

template <typename T, typename... Types>
struct value_traits
{
    using value_type = typename std::remove_reference<T>::type;
    static constexpr int direct_index = direct_type<value_type, Types...>::index;
    static constexpr bool is_direct = direct_index != -1;
    static constexpr int index = is_direct ? direct_index : convertible_type<value_type, Types...>::index;
    static constexpr bool is_valid = index != -1;
    static constexpr int tindex = is_valid ? (int)sizeof...(Types) - index : 0;
    using target_type = typename std::tuple_element<tindex, std::tuple<void, Types...>>::type;
};

// check if T is in Types...
template <typename T, typename... Types>
struct has_type;

template <typename T, typename First, typename... Types>
struct has_type<T, First, Types...>
{
    static constexpr bool value = std::is_same<T, First>::value
        || has_type<T, Types...>::value;
};

template <typename T>
struct has_type<T> : std::false_type {};

template <typename T, typename... Types>
struct is_valid_type;

template <typename T, typename First, typename... Types>
struct is_valid_type<T, First, Types...>
{
    static constexpr bool value = std::is_convertible<T, First>::value
        || is_valid_type<T, Types...>::value;
};

template <typename T>
struct is_valid_type<T> : std::false_type {};


template <typename T, typename R = void>
struct enable_if_type { using type = R; };

template <typename F, typename V, typename Enable = void>
struct result_of_unary_visit
{
    using type = typename std::result_of<F(V &)>::type;
};

template <typename F, typename V>
struct result_of_unary_visit<F, V, typename enable_if_type<typename F::result_type>::type >
{
    using type = typename F::result_type;
};

template <typename F, typename V, typename Enable = void>
struct result_of_binary_visit
{
    using type = typename std::result_of<F(V &, V &)>::type;
};


template <typename F, typename V>
struct result_of_binary_visit<F, V, typename enable_if_type<typename F::result_type>::type >
{
    using type = typename F::result_type;
};




template <int arg1, int... others>
struct static_max;

template <int arg>
struct static_max<arg>
{
    static const int value = arg;
};

template <int arg1, int arg2, int... others>
struct static_max<arg1, arg2, others...>
{
    static const int value = arg1 >= arg2 ? static_max<arg1, others...>::value :
        static_max<arg2, others...>::value;
};

template <typename... Types>
struct variant_helper;

template <typename T, typename... Types>
struct variant_helper<T, Types...>
{
    VARIANT_INLINE static void destroy(int type_index, void * data)
    {
        if (type_index == (int)sizeof...(Types))
        {
            reinterpret_cast<T*>(data)->~T();
        }
        else
        {
            variant_helper<Types...>::destroy(type_index, data);
        }
    }

    VARIANT_INLINE static void move(int old_type_index, void * old_value, void * new_value)
    {
        if (old_type_index == (int)sizeof...(Types))
        {
            new (new_value) T(std::move(*reinterpret_cast<T*>(old_value)));
        }
        else
        {
            variant_helper<Types...>::move(old_type_index, old_value, new_value);
        }
    }

    VARIANT_INLINE static void copy(int old_type_index, const void * old_value, void * new_value)
    {
        if (old_type_index == (int)sizeof...(Types))
        {
            new (new_value) T(*reinterpret_cast<const T*>(old_value));
        }
        else
        {
            variant_helper<Types...>::copy(old_type_index, old_value, new_value);
        }
    }

};

template <>
struct variant_helper<>
{
    VARIANT_INLINE static void destroy(int, void *) {}
    VARIANT_INLINE static void move(int, void *, void *) {}
    VARIANT_INLINE static void copy(int, const void *, void *) {}
};

template <typename T>
struct unwrapper
{
    static T const& apply_const(T const& obj) {return obj;}
    static T& apply(T & obj) {return obj;}
};

template <typename T>
struct unwrapper<std::reference_wrapper<T>>
{
    static auto apply_const(std::reference_wrapper<T> const& obj)
        -> typename std::reference_wrapper<T>::type const&
    {
        return obj.get();
    }
    static auto apply(std::reference_wrapper<T> & obj)
        -> typename std::reference_wrapper<T>::type&
    {
        return obj.get();
    }
};

template <typename F, typename V, typename R, typename... Types>
struct dispatcher;

template <typename F, typename V, typename R, typename T, typename... Types>
struct dispatcher<F, V, R, T, Types...>
{
    VARIANT_INLINE static R apply_const(V const& v, F && f)
    {
        if (v. template is<T>())
        {
            return f(unwrapper<T>::apply_const(v. template get<T>()));
        }
        else
        {
            return dispatcher<F, V, R, Types...>::apply_const(v, std::forward<F>(f));
        }
    }

    VARIANT_INLINE static R apply(V & v, F && f)
    {
        if (v. template is<T>())
        {
            return f(unwrapper<T>::apply(v. template get<T>()));
        }
        else
        {
            return dispatcher<F, V, R, Types...>::apply(v, std::forward<F>(f));
        }
    }
};

template <typename F, typename V, typename R, typename T>
struct dispatcher<F, V, R, T>
{
    VARIANT_INLINE static R apply_const(V const& v, F && f)
    {
        return f(unwrapper<T>::apply_const(v. template get<T>()));
    }

    VARIANT_INLINE static R apply(V & v, F && f)
    {
        return f(unwrapper<T>::apply(v. template get<T>()));
    }
};


template <typename F, typename V, typename R, typename T, typename... Types>
struct binary_dispatcher_rhs;

template <typename F, typename V, typename R, typename T0, typename T1, typename... Types>
struct binary_dispatcher_rhs<F, V, R, T0, T1, Types...>
{
    VARIANT_INLINE static R apply_const(V const& lhs, V const& rhs, F && f)
    {
        if (rhs. template is<T1>()) // call binary functor
        {
            return f(unwrapper<T0>::apply_const(lhs. template get<T0>()),
                     unwrapper<T1>::apply_const(rhs. template get<T1>()));
        }
        else
        {
            return binary_dispatcher_rhs<F, V, R, T0, Types...>::apply_const(lhs, rhs, std::forward<F>(f));
        }
    }

    VARIANT_INLINE static R apply(V & lhs, V & rhs, F && f)
    {
        if (rhs. template is<T1>()) // call binary functor
        {
            return f(unwrapper<T0>::apply(lhs. template get<T0>()),
                     unwrapper<T1>::apply(rhs. template get<T1>()));
        }
        else
        {
            return binary_dispatcher_rhs<F, V, R, T0, Types...>::apply(lhs, rhs, std::forward<F>(f));
        }
    }

};

template <typename F, typename V, typename R, typename T0, typename T1>
struct binary_dispatcher_rhs<F, V, R, T0, T1>
{
    VARIANT_INLINE static R apply_const(V const& lhs, V const& rhs, F && f)
    {
        return f(unwrapper<T0>::apply_const(lhs. template get<T0>()),
                 unwrapper<T1>::apply_const(rhs. template get<T1>()));
    }

    VARIANT_INLINE static R apply(V & lhs, V & rhs, F && f)
    {
        return f(unwrapper<T0>::apply(lhs. template get<T0>()),
                 unwrapper<T1>::apply(rhs. template get<T1>()));
    }

};


template <typename F, typename V, typename R,  typename T, typename... Types>
struct binary_dispatcher_lhs;

template <typename F, typename V, typename R, typename T0, typename T1, typename... Types>
struct binary_dispatcher_lhs<F, V, R, T0, T1, Types...>
{
    VARIANT_INLINE static R apply_const(V const& lhs, V const& rhs, F && f)
    {
        if (lhs. template is<T1>()) // call binary functor
        {
            return f(unwrapper<T1>::apply_const(lhs. template get<T1>()),
                     unwrapper<T0>::apply_const(rhs. template get<T0>()));
        }
        else
        {
            return binary_dispatcher_lhs<F, V, R, T0, Types...>::apply_const(lhs, rhs, std::forward<F>(f));
        }
    }

    VARIANT_INLINE static R apply(V & lhs, V & rhs, F && f)
    {
        if (lhs. template is<T1>()) // call binary functor
        {
            return f(unwrapper<T1>::apply(lhs. template get<T1>()),
                     unwrapper<T0>::apply(rhs. template get<T0>()));
        }
        else
        {
            return binary_dispatcher_lhs<F, V, R, T0, Types...>::apply(lhs, rhs, std::forward<F>(f));
        }
    }

};

template <typename F, typename V, typename R, typename T0, typename T1>
struct binary_dispatcher_lhs<F, V, R, T0, T1>
{
    VARIANT_INLINE static R apply_const(V const& lhs, V const& rhs, F && f)
    {
        return f(unwrapper<T1>::apply_const(lhs. template get<T1>()),
                 unwrapper<T0>::apply_const(rhs. template get<T0>()));
    }

    VARIANT_INLINE static R apply(V & lhs, V & rhs, F && f)
    {
        return f(unwrapper<T1>::apply(lhs. template get<T1>()),
                 unwrapper<T0>::apply(rhs. template get<T0>()));
    }

};


template <typename F, typename V, typename R, typename... Types>
struct binary_dispatcher;

template <typename F, typename V, typename R, typename T, typename... Types>
struct binary_dispatcher<F, V, R, T, Types...>
{
    VARIANT_INLINE static R apply_const(V const& v0, V const& v1, F && f)
    {
        if (v0. template is<T>())
        {
            if (v1. template is<T>())
            {
                return f(unwrapper<T>::apply_const(v0. template get<T>()),
                         unwrapper<T>::apply_const(v1. template get<T>())); // call binary functor
            }
            else
            {
                return binary_dispatcher_rhs<F, V, R, T, Types...>::apply_const(v0, v1, std::forward<F>(f));
            }
        }
        else if (v1. template is<T>())
        {
            return binary_dispatcher_lhs<F, V, R, T, Types...>::apply_const(v0, v1, std::forward<F>(f));
        }
        return binary_dispatcher<F, V, R, Types...>::apply_const(v0, v1, std::forward<F>(f));
    }

    VARIANT_INLINE static R apply(V & v0, V & v1, F && f)
    {
        if (v0. template is<T>())
        {
            if (v1. template is<T>())
            {
                return f(unwrapper<T>::apply(v0. template get<T>()),
                         unwrapper<T>::apply(v1. template get<T>())); // call binary functor
            }
            else
            {
                return binary_dispatcher_rhs<F, V, R, T, Types...>::apply(v0, v1, std::forward<F>(f));
            }
        }
        else if (v1. template is<T>())
        {
            return binary_dispatcher_lhs<F, V, R, T, Types...>::apply(v0, v1, std::forward<F>(f));
        }
        return binary_dispatcher<F, V, R, Types...>::apply(v0, v1, std::forward<F>(f));
    }
};

template <typename F, typename V, typename R, typename T>
struct binary_dispatcher<F, V, R, T>
{
    VARIANT_INLINE static R apply_const(V const& v0, V const& v1, F && f)
    {
        return f(unwrapper<T>::apply_const(v0. template get<T>()),
                 unwrapper<T>::apply_const(v1. template get<T>())); // call binary functor
    }

    VARIANT_INLINE static R apply(V & v0, V & v1, F && f)
    {
        return f(unwrapper<T>::apply(v0. template get<T>()),
                 unwrapper<T>::apply(v1. template get<T>())); // call binary functor
    }
};


// comparator functors
struct equal_comp
{
    template <typename T>
    bool operator()(T const& lhs, T const& rhs) const
    {
        return lhs == rhs;
    }
};

struct less_comp
{
    template <typename T>
    bool operator()(T const& lhs, T const& rhs) const
    {
        return lhs < rhs;
    }
};

template <typename Variant, typename Comp>
class comparer
{
public:
    explicit comparer(Variant const& lhs): lhs_(lhs) {}
    comparer & operator=(comparer const&) = delete;
    // visitor
    template <typename T>
    bool operator()(T const& rhs_content) const
    {
        T const& lhs_content = lhs_.template get<T>();
        return Comp()(lhs_content, rhs_content);
    }
private:
    Variant const& lhs_;
};

// True if Predicate matches for all of the types Ts
template <template<typename> class Predicate, typename... Ts>
struct static_all_of : std::is_same<std::tuple<std::true_type, typename Predicate<Ts>::type...>,
                                    std::tuple<typename Predicate<Ts>::type..., std::true_type>>
{};

// True if Predicate matches for none of the types Ts
template <template<typename> class Predicate, typename... Ts>
struct static_none_of : std::is_same<std::tuple<std::false_type, typename Predicate<Ts>::type...>,
                                     std::tuple<typename Predicate<Ts>::type..., std::false_type>>
{};


template <typename... Fns>
struct visitor;

template <typename Fn>
struct visitor<Fn> : Fn
{
    using type = Fn;
    using Fn::operator();

    visitor(Fn fn) : Fn(fn) {}
};

template <typename Fn, typename... Fns>
struct visitor<Fn, Fns...> : Fn, visitor<Fns...>
{
    using type = visitor;
    using Fn::operator();
    using visitor<Fns...>::operator();

    visitor(Fn fn, Fns... fns) : Fn(fn), visitor<Fns...>(fns...) {}
};

template <typename... Fns>
visitor<Fns...> make_visitor(Fns... fns)
{
    return visitor<Fns...>(fns...);
}

} // namespace detail

struct no_init {};

struct TypeNotInVariant;

template <typename... Types>
class Variant
{
    static_assert(sizeof...(Types) > 0, "Template parameter type list of variant can not be empty");
	static_assert(sizeof...(Types) < 128, "Please, keep it reasonable");
    static_assert(detail::static_none_of<std::is_reference, Types...>::value, "Variant can not hold reference types. Maybe use std::reference?");

	template <class T>
	using EnableIfValidType = EnableIf<(detail::direct_type<T, Types...>::index != -1), TypeNotInVariant>;

#ifdef NDEBUG
	enum { ndebug_access = true };
#else
	enum { ndebug_access = false };
#endif

private:
    static const int data_size = detail::static_max<(int)sizeof(Types)...>::value;
    static const int data_align = detail::static_max<(int)alignof(Types)...>::value;

    using first_type = typename std::tuple_element<0, std::tuple<Types...>>::type;
    using data_type = typename std::aligned_storage<data_size, data_align>::type;
    using helper_type = detail::variant_helper<Types...>;

    data_type data;
    char type_index;

public:
    VARIANT_INLINE Variant()
        : type_index((int)sizeof...(Types) - 1)
    {
        static_assert(std::is_default_constructible<first_type>::value, "First type in variant must be default constructible to allow default construction of variant");
        new (&data) first_type();
    }

    VARIANT_INLINE Variant(no_init) : type_index(-1) {}

    // http://isocpp.org/blog/2012/11/universal-references-in-c11-scott-meyers
    template <typename T, typename Traits = detail::value_traits<T, Types...>,
                          typename Enable = typename std::enable_if<Traits::is_valid>::type>
    VARIANT_INLINE Variant(T && val)
        : type_index(Traits::index)
    {
        new (&data) typename Traits::target_type(std::forward<T>(val));
    }

    VARIANT_INLINE Variant(Variant<Types...> const& old)
        : type_index(old.type_index)
    {
        helper_type::copy(old.type_index, &old.data, &data);
    }

    VARIANT_INLINE Variant(Variant<Types...> && old)
        : type_index(old.type_index)
    {
        helper_type::move(old.type_index, &old.data, &data);
    }

private:
    VARIANT_INLINE void copy_assign(Variant<Types...> const& rhs)
    {
        helper_type::destroy(type_index, &data);
        type_index = -1;
        helper_type::copy(rhs.type_index, &rhs.data, &data);
        type_index = rhs.type_index;
    }

    VARIANT_INLINE void move_assign(Variant<Types...> && rhs)
    {
        helper_type::destroy(type_index, &data);
        type_index = -1;
        helper_type::move(rhs.type_index, &rhs.data, &data);
        type_index = rhs.type_index;
    }

public:
    VARIANT_INLINE Variant<Types...>& operator=(Variant<Types...> &&  other)
    {
        move_assign(std::move(other));
        return *this;
    }

    VARIANT_INLINE Variant<Types...>& operator=(Variant<Types...> const& other)
    {
        copy_assign(other);
        return *this;
    }

    // conversions
    // move-assign
    template <typename T>
    VARIANT_INLINE Variant<Types...>& operator=(T && rhs)
    {
        Variant<Types...> temp(std::forward<T>(rhs));
        move_assign(std::move(temp));
        return *this;
    }

    // copy-assign
    template <typename T>
    VARIANT_INLINE Variant<Types...>& operator=(T const& rhs)
    {
        Variant<Types...> temp(rhs);
        copy_assign(temp);
        return *this;
    }

    template <typename T>
    VARIANT_INLINE bool is() const
    {
        static_assert(detail::has_type<T, Types...>::value, "invalid type in T in `is<T>()` for this variant");
        return type_index == detail::direct_type<T, Types...>::index;
    }

    VARIANT_INLINE bool valid() const
    {
        return type_index != -1;
    }

    template <typename T, typename... Args>
    VARIANT_INLINE void set(Args &&... args)
    {
        helper_type::destroy(type_index, &data);
        type_index = -1;
        new (&data) T(std::forward<Args>(args)...);
        type_index = detail::direct_type<T, Types...>::index;
    }

    // get<T>()
    template <typename T, EnableIfValidType<T>...>
    VARIANT_INLINE T & get()
    {
        if (type_index == detail::direct_type<T, Types...>::index || ndebug_access)
            return *reinterpret_cast<T*>(&data);
        else
			FATAL("Bad variant access in get()");
    }

    template <typename T, EnableIfValidType<T>...>
    VARIANT_INLINE T const& get() const
    {
        if (type_index == detail::direct_type<T, Types...>::index || ndebug_access)
            return *reinterpret_cast<T const*>(&data);
        else
			FATAL("Bad variant access in get()");
    }

    // get<T>() - T stored as std::reference_wrapper<T>
    template <typename T, EnableIfValidType<std::reference_wrapper<T>>...>
    VARIANT_INLINE T& get() {
        if (type_index == detail::direct_type<std::reference_wrapper<T>, Types...>::index || ndebug_access)
            return (*reinterpret_cast<std::reference_wrapper<T>*>(&data)).get();
        else
			FATAL("Bad variant access in get()");
    }

    template <typename T, EnableIfValidType<std::reference_wrapper<const T>>...>
    VARIANT_INLINE T const& get() const {
        if (type_index == detail::direct_type<std::reference_wrapper<T const>, Types...>::index || ndebug_access)
            return (*reinterpret_cast<std::reference_wrapper<T const> const*>(&data)).get();
        else
			FATAL("Bad variant access in get()");
    }

	template <typename T, EnableIfValidType<T>...>
    VARIANT_INLINE operator T *() {
        return type_index == detail::direct_type<T, Types...>::index? reinterpret_cast<T *>(&data) : nullptr;
    }

	template <typename T, EnableIfValidType<T>...>
    VARIANT_INLINE operator const T *() const {
        return type_index == detail::direct_type<T, Types...>::index? reinterpret_cast<T const*>(&data) : nullptr;
    }

	template <typename T, EnableIfValidType<std::reference_wrapper<T>>...>
    VARIANT_INLINE operator T*() {
        if (type_index == detail::direct_type<std::reference_wrapper<T>, Types...>::index)
            return &(*reinterpret_cast<std::reference_wrapper<T>*>(&data)).get();
		return nullptr;
    }

    template <typename T, EnableIfValidType<std::reference_wrapper<const T>>...>
    VARIANT_INLINE operator const T*() const {
        if (type_index == detail::direct_type<std::reference_wrapper<T const>, Types...>::index)
            return &(*reinterpret_cast<std::reference_wrapper<T const> const*>(&data)).get();
		return nullptr;
    }

    VARIANT_INLINE int which() const {
        return (int)sizeof...(Types) - type_index - 1;
    }

    // visitor
    // unary
    template <typename F, typename V, typename R = typename detail::result_of_unary_visit<F, first_type>::type>
    auto VARIANT_INLINE
    static visit(V const& v, F && f) {
        return detail::dispatcher<F, V, R, Types...>::apply_const(v, std::forward<F>(f));
    }
    // non-const
    template <typename F, typename V, typename R = typename detail::result_of_unary_visit<F, first_type>::type>
    auto VARIANT_INLINE
    static visit(V & v, F && f) {
        return detail::dispatcher<F, V, R, Types...>::apply(v, std::forward<F>(f));
    }

    // binary
    // const
    template <typename F, typename V, typename R = typename detail::result_of_binary_visit<F, first_type>::type>
    auto VARIANT_INLINE
    static binary_visit(V const& v0, V const& v1, F && f) {
        return detail::binary_dispatcher<F, V, R, Types...>::apply_const(v0, v1, std::forward<F>(f));
    }
    // non-const
    template <typename F, typename V, typename R = typename detail::result_of_binary_visit<F, first_type>::type>
    auto VARIANT_INLINE
    static binary_visit(V& v0, V& v1, F && f) {
        return detail::binary_dispatcher<F, V, R, Types...>::apply(v0, v1, std::forward<F>(f));
    }


    // unary
    template <typename... Fs>
    auto VARIANT_INLINE match(Fs&&... fs) const {
        return visit(*this, detail::make_visitor(std::forward<Fs>(fs)...));
    }
    // non-const
    template <typename... Fs>
    auto VARIANT_INLINE match(Fs&&... fs) {
        return visit(*this, detail::make_visitor(std::forward<Fs>(fs)...));
    }

    ~Variant() {
        helper_type::destroy(type_index, &data);
    }

    VARIANT_INLINE bool operator==(Variant const& rhs) const {
        DASSERT(valid() && rhs.valid());
        if (this->which() != rhs.which())
            return false;
        detail::comparer<Variant, detail::equal_comp> visitor(*this);
        return visit(rhs, visitor);
    }

    VARIANT_INLINE bool operator<(Variant const& rhs) const {
        DASSERT(valid() && rhs.valid());
        if (this->which() != rhs.which())
            return this->which() < rhs.which();
        detail::comparer<Variant, detail::less_comp> visitor(*this);
        return visit(rhs, visitor);
    }
};

// unary visitor interface
// const
template <typename F, typename V>
auto VARIANT_INLINE apply_visitor(F && f, V const& v) -> decltype(V::visit(v, std::forward<F>(f)))
{
    return V::visit(v, std::forward<F>(f));
}

// non-const
template <typename F, typename V>
auto VARIANT_INLINE apply_visitor(F && f, V & v) -> decltype(V::visit(v, std::forward<F>(f)))
{
    return V::visit(v, std::forward<F>(f));
}

// binary visitor interface
// const
template <typename F, typename V>
auto VARIANT_INLINE apply_visitor(F && f, V const& v0, V const& v1) {
    return V::binary_visit(v0, v1, std::forward<F>(f));
}

// non-const
template <typename F, typename V>
auto VARIANT_INLINE apply_visitor(F && f, V & v0, V & v1) {
    return V::binary_visit(v0, v1, std::forward<F>(f));
}

// getter interface
template <typename ResultType, typename T>
ResultType & get(T & var) {
    return var.template get<ResultType>();
}

template <typename ResultType, typename T>
ResultType const& get(T const& var) {
    return var.template get<ResultType>();
}

}
