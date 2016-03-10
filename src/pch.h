#include <vector>
#include <array>
#include <string>
#include <exception>
#include <type_traits>
#include <memory>
#include <map>
#include <cstring>
#include <cstdlib>
#include <algorithm>
#include <atomic>

#ifndef BOOST_PP_VARIADICS
#define BOOST_PP_VARIADICS 1
#endif

#if !BOOST_PP_VARIADICS
#error FWK requires BOOST_PP_VARIADICS == 1
#endif

#include <boost/preprocessor/list/to_tuple.hpp>
#include <boost/preprocessor/variadic/to_list.hpp>

#include <boost/optional.hpp>
