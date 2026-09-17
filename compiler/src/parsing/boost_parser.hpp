#pragma once

// Boost 1.90's flags are a bitmask: combinations and zero are valid enum
// values. Tell Clang this semantic fact so its enum checker can analyze library
// calls.
#if defined(__clang__)
namespace boost::parser::detail {
enum class [[clang::flag_enum]] flags : unsigned int;
}
#endif
#include <boost/parser/parser.hpp>
