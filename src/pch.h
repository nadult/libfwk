#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <ctime>
#include <cwchar>
#include <exception>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

#ifndef BOOST_PP_VARIADICS
#define BOOST_PP_VARIADICS 1
#endif

#if !BOOST_PP_VARIADICS
#error FWK requires BOOST_PP_VARIADICS == 1
#endif

#include <boost/preprocessor/list/to_tuple.hpp>
#include <boost/preprocessor/list/transform.hpp>
#include <boost/preprocessor/variadic/to_list.hpp>
