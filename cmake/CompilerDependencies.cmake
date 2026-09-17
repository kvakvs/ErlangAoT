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
