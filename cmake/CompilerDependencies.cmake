# Explicit roots remain optional; installed Boost and local dependency checkouts are also searched.
set(ERLANG_AOT_BOOST_PARSER_ROOT "" CACHE PATH "Standalone Boost.Parser source or installed Boost prefix")
set(ERLANG_AOT_BOOST_ROOT "" CACHE PATH "Boost 1.90 or newer source or installed prefix")

# Homebrew's formula prefix also covers unlinked/keg-only installations and custom brew locations.
set(boost_hints)
if(APPLE AND NOT CMAKE_CROSSCOMPILING)
    find_program(ERLANG_AOT_BREW_EXECUTABLE brew HINTS /opt/homebrew/bin /usr/local/bin)
    if(ERLANG_AOT_BREW_EXECUTABLE)
        execute_process(COMMAND "${ERLANG_AOT_BREW_EXECUTABLE}" --prefix boost
            RESULT_VARIABLE brew_result OUTPUT_VARIABLE brew_prefix
            OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET TIMEOUT 10)
        if(brew_result STREQUAL "0" AND IS_DIRECTORY "${brew_prefix}")
            list(APPEND boost_hints "${brew_prefix}/include")
        endif()
    endif()
endif()

# Full Boost installations advertise a release; standalone Parser retains the pinned checksum below.
function(erlang_aot_check_boost_version include_dir)
    if(NOT EXISTS "${include_dir}/boost/version.hpp")
        message(FATAL_ERROR "Boost version.hpp is missing under ${include_dir}")
    endif()
    file(STRINGS "${include_dir}/boost/version.hpp" version_line REGEX "^#define BOOST_VERSION ")
    if(NOT version_line MATCHES "^#define BOOST_VERSION +([0-9]+)$")
        message(FATAL_ERROR "Cannot read Boost version from ${include_dir}/boost/version.hpp")
    endif()
    if(CMAKE_MATCH_1 LESS 109000)
        message(FATAL_ERROR "Boost 1.90 or newer is required; found ${version_line} in ${include_dir}")
    endif()
endfunction()

find_path(ERLANG_AOT_BOOST_PARSER_INCLUDE boost/parser/parser.hpp
    HINTS "${ERLANG_AOT_BOOST_PARSER_ROOT}" "${ERLANG_AOT_BOOST_PARSER_ROOT}/include"
        "${ERLANG_AOT_BOOST_ROOT}" "${ERLANG_AOT_BOOST_ROOT}/include" ${boost_hints}
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

# Arithmetic and parser headers stay compiler-private; runtime-only builds never include this file.
find_path(ERLANG_AOT_BOOST_INCLUDE boost/multiprecision/cpp_int.hpp
    HINTS "${ERLANG_AOT_BOOST_ROOT}" "${ERLANG_AOT_BOOST_ROOT}/include"
        "${ERLANG_AOT_BOOST_PARSER_INCLUDE}" ${boost_hints}
    PATHS "${PROJECT_SOURCE_DIR}/build/deps/boost_1_90_0" REQUIRED)
erlang_aot_check_boost_version("${ERLANG_AOT_BOOST_INCLUDE}")
message(STATUS "Boost.Parser headers: ${ERLANG_AOT_BOOST_PARSER_INCLUDE}")
message(STATUS "Boost.Multiprecision headers: ${ERLANG_AOT_BOOST_INCLUDE}")
add_library(erlang_aot_parser_dependency INTERFACE)
target_include_directories(erlang_aot_parser_dependency SYSTEM INTERFACE
    "${ERLANG_AOT_BOOST_PARSER_INCLUDE}" "${ERLANG_AOT_BOOST_INCLUDE}")
