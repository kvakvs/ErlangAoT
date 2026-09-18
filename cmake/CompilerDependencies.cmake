# Use the standalone headers from the pinned Boost.Parser release.
set(ERLANG_AOT_BOOST_PARSER_ROOT "${PROJECT_SOURCE_DIR}/build/deps/boost-parser"
    CACHE PATH "Boost.Parser 1.90.0 source or installed Boost prefix")
find_path(ERLANG_AOT_BOOST_PARSER_INCLUDE boost/parser/parser.hpp
    HINTS "${ERLANG_AOT_BOOST_PARSER_ROOT}/include")
if(NOT ERLANG_AOT_BOOST_PARSER_INCLUDE)
    message(FATAL_ERROR "Boost.Parser 1.90.0 is required. See README.md dependency setup.")
endif()
file(SHA256 "${ERLANG_AOT_BOOST_PARSER_INCLUDE}/boost/parser/parser.hpp" parser_hash)
if(NOT parser_hash STREQUAL "f043c986bec69fe029c53921076b71698c8e745730ff4e1d54cc10199a880e0f")
    message(FATAL_ERROR "Boost.Parser headers do not match the pinned 1.90.0 release.")
endif()
add_library(erlang_aot_parser_dependency INTERFACE)
target_include_directories(erlang_aot_parser_dependency SYSTEM INTERFACE
    "${ERLANG_AOT_BOOST_PARSER_INCLUDE}")

# Arbitrary precision condition values belong to the compiler, never the runtime.
set(ERLANG_AOT_BOOST_ROOT "${PROJECT_SOURCE_DIR}/build/deps/boost_1_90_0"
    CACHE PATH "Boost 1.90.0 headers for compiler arithmetic")
find_path(ERLANG_AOT_BOOST_INCLUDE boost/multiprecision/cpp_int.hpp
    HINTS "${ERLANG_AOT_BOOST_ROOT}" "${ERLANG_AOT_BOOST_ROOT}/include" REQUIRED)
file(STRINGS "${ERLANG_AOT_BOOST_INCLUDE}/boost/version.hpp" boost_version REGEX "^#define BOOST_VERSION ")
if(NOT boost_version STREQUAL "#define BOOST_VERSION 109000")
    message(FATAL_ERROR "Compiler arithmetic requires Boost 1.90.0 headers")
endif()
target_include_directories(erlang_aot_parser_dependency SYSTEM INTERFACE "${ERLANG_AOT_BOOST_INCLUDE}")
