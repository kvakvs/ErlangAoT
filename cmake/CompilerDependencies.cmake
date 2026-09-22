# Parser is compiler-only; Multiprecision is shared with the independently built runtime.
set(ERLANG_AOT_BOOST_PARSER_ROOT "" CACHE PATH "Standalone Boost.Parser source or installed Boost prefix")
include("${CMAKE_CURRENT_LIST_DIR}/BoostDependencies.cmake")

find_path(ERLANG_AOT_BOOST_PARSER_INCLUDE boost/parser/parser.hpp
    HINTS "${ERLANG_AOT_BOOST_PARSER_ROOT}" "${ERLANG_AOT_BOOST_PARSER_ROOT}/include"
        "${ERLANG_AOT_BOOST_ROOT}" "${ERLANG_AOT_BOOST_ROOT}/include"
        "${ERLANG_AOT_BOOST_INCLUDE}" ${boost_hints}
    PATHS "${PROJECT_SOURCE_DIR}/build/deps/boost-parser/include"
        "${PROJECT_SOURCE_DIR}/build/deps/boost_1_90_0")
if(NOT ERLANG_AOT_BOOST_PARSER_INCLUDE)
    message(FATAL_ERROR "Boost.Parser is required. Install Boost 1.90 or newer (brew install boost on macOS), or see README.md dependency setup.")
endif()
if(EXISTS "${ERLANG_AOT_BOOST_PARSER_INCLUDE}/boost/version.hpp")
    erlang_aot_check_boost_version("${ERLANG_AOT_BOOST_PARSER_INCLUDE}")
else()
    file(SHA256 "${ERLANG_AOT_BOOST_PARSER_INCLUDE}/boost/parser/parser.hpp" parser_hash)
    if(NOT parser_hash STREQUAL "f043c986bec69fe029c53921076b71698c8e745730ff4e1d54cc10199a880e0f")
        message(FATAL_ERROR "Standalone Boost.Parser must match the pinned 1.90.0 release; alternatively use an installed Boost 1.90 or newer.")
    endif()
endif()

message(STATUS "Boost.Parser headers: ${ERLANG_AOT_BOOST_PARSER_INCLUDE}")
add_library(erlang_aot_parser_dependency INTERFACE)
target_include_directories(erlang_aot_parser_dependency SYSTEM INTERFACE
    "${ERLANG_AOT_BOOST_PARSER_INCLUDE}")
target_link_libraries(erlang_aot_parser_dependency INTERFACE erlang_aot_multiprecision_dependency)
