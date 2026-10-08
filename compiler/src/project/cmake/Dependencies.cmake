# Project TOML support uses a tested, compiler-private header-only dependency.
set(CLAUSE_TOML_ROOT "" CACHE PATH "toml++ 3.4.0 installation or source root")
include("${PROJECT_SOURCE_DIR}/cmake/ThirdPartyDependencies.cmake")
# Re-evaluate the explicit root even when a prior configuration found another copy.
unset(CLAUSE_TOML_INCLUDE CACHE)
if(CLAUSE_TOML_ROOT)
    find_path(CLAUSE_TOML_INCLUDE toml++/toml.hpp
        PATHS "${CLAUSE_TOML_ROOT}/include" "${CLAUSE_TOML_ROOT}" NO_DEFAULT_PATH)
elseif(CMAKE_HOST_WIN32)
    clause_download_dependency(tomlplusplus-3.4.0
        "https://github.com/marzer/tomlplusplus/archive/refs/tags/v3.4.0.tar.gz"
        "8517f65938a4faae9ccf8ebb36631a38c1cadfb5efa85d9a72e15b9e97d25155"
        "include/toml++/toml.hpp" toml_source)
    find_path(CLAUSE_TOML_INCLUDE toml++/toml.hpp
        PATHS "${toml_source}/include" NO_DEFAULT_PATH)
else()
    # Ask Homebrew for the formula prefix so unlinked/custom installations also work.
    set(toml_hints)
    if(APPLE AND NOT CMAKE_CROSSCOMPILING)
        find_program(CLAUSE_BREW_EXECUTABLE brew HINTS /opt/homebrew/bin /usr/local/bin)
        if(CLAUSE_BREW_EXECUTABLE)
            execute_process(COMMAND "${CLAUSE_BREW_EXECUTABLE}" --prefix tomlplusplus
                RESULT_VARIABLE toml_brew_result OUTPUT_VARIABLE toml_brew_prefix
                OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET TIMEOUT 10)
            if(toml_brew_result STREQUAL "0" AND IS_DIRECTORY "${toml_brew_prefix}")
                list(APPEND toml_hints "${toml_brew_prefix}/include")
            endif()
        endif()
    endif()
    find_path(CLAUSE_TOML_INCLUDE toml++/toml.hpp
        HINTS ${toml_hints}
        PATHS "${PROJECT_SOURCE_DIR}/thirdparty/tomlplusplus-3.4.0/include")
endif()
if(NOT CLAUSE_TOML_INCLUDE)
    message(FATAL_ERROR "toml++ 3.4.0 is required. Set CLAUSE_TOML_ROOT to its source/install root; see docs/projects.md.")
endif()
file(READ "${CLAUSE_TOML_INCLUDE}/toml++/impl/version.hpp" toml_version)
foreach(part IN ITEMS MAJOR MINOR PATCH)
    string(REGEX MATCH "#define TOML_LIB_${part} ([0-9]+)" matched "${toml_version}")
    set(toml_${part} "${CMAKE_MATCH_1}")
endforeach()
if(NOT "${toml_MAJOR}.${toml_MINOR}.${toml_PATCH}" STREQUAL "3.4.0")
    message(FATAL_ERROR "Expected tested toml++ 3.4.0 under ${CLAUSE_TOML_INCLUDE}")
endif()
message(STATUS "toml++ 3.4.0 headers: ${CLAUSE_TOML_INCLUDE}")
add_library(clause_project_toml INTERFACE)
target_include_directories(clause_project_toml SYSTEM INTERFACE "${CLAUSE_TOML_INCLUDE}")
target_compile_definitions(clause_project_toml INTERFACE
    TOML_HEADER_ONLY=1 TOML_EXCEPTIONS=1 TOML_ENABLE_UNRELEASED_FEATURES=0)
