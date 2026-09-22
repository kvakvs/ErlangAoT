# Share header-only Boost dependencies across independently enabled project components.
include_guard(GLOBAL)

# Explicit roots remain optional; installed Boost and local dependency checkouts are also searched.
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

# Require the same supported Boost release for compiler and runtime headers.
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

# Multiprecision is header-only; propagate its include path without a binary Boost library.
find_path(ERLANG_AOT_BOOST_INCLUDE boost/multiprecision/cpp_int.hpp
    HINTS "${ERLANG_AOT_BOOST_ROOT}" "${ERLANG_AOT_BOOST_ROOT}/include"
        "${ERLANG_AOT_BOOST_PARSER_ROOT}" "${ERLANG_AOT_BOOST_PARSER_ROOT}/include"
        "${ERLANG_AOT_BOOST_PARSER_INCLUDE}" ${boost_hints}
    PATHS "${PROJECT_SOURCE_DIR}/build/deps/boost_1_90_0")
if(NOT ERLANG_AOT_BOOST_INCLUDE)
    message(FATAL_ERROR "Boost.Multiprecision is required. Install Boost 1.90 or newer (brew install boost on macOS), or set ERLANG_AOT_BOOST_ROOT.")
endif()
erlang_aot_check_boost_version("${ERLANG_AOT_BOOST_INCLUDE}")
message(STATUS "Boost.Multiprecision headers: ${ERLANG_AOT_BOOST_INCLUDE}")
set(ERLANG_AOT_BOOST_SYSTEM_INCLUDES "${ERLANG_AOT_BOOST_INCLUDE}")
# Homebrew's linked headers can shadow the formula path through inherited -I flags.
# Mark that alias as system only when it resolves to the selected Boost installation.
if(APPLE AND NOT CMAKE_CROSSCOMPILING AND ERLANG_AOT_BREW_EXECUTABLE)
    execute_process(COMMAND "${ERLANG_AOT_BREW_EXECUTABLE}" --prefix
        RESULT_VARIABLE brew_result OUTPUT_VARIABLE brew_install_prefix
        OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET TIMEOUT 10)
    if(brew_result STREQUAL "0" AND IS_DIRECTORY "${brew_install_prefix}/include/boost")
        file(REAL_PATH "${ERLANG_AOT_BOOST_INCLUDE}/boost" selected_boost)
        file(REAL_PATH "${brew_install_prefix}/include/boost" linked_boost)
        if(selected_boost STREQUAL linked_boost)
            list(APPEND ERLANG_AOT_BOOST_SYSTEM_INCLUDES "${brew_install_prefix}/include")
            # CMake can infer the inherited -I as implicit and omit the required -isystem.
            list(REMOVE_ITEM CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES "${brew_install_prefix}/include")
        endif()
    endif()
endif()
add_library(erlang_aot_multiprecision_dependency INTERFACE)
target_include_directories(erlang_aot_multiprecision_dependency SYSTEM INTERFACE
    ${ERLANG_AOT_BOOST_SYSTEM_INCLUDES})
