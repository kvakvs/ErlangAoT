# Project TOML support uses a tested, compiler-private header-only dependency.
set(ERLANG_AOT_TOML_ROOT "" CACHE PATH "toml++ 3.4.0 installation or source root")
# Re-evaluate the explicit root even when a prior configuration found another copy.
unset(ERLANG_AOT_TOML_INCLUDE CACHE)
if(ERLANG_AOT_TOML_ROOT)
    find_path(ERLANG_AOT_TOML_INCLUDE toml++/toml.hpp
        PATHS "${ERLANG_AOT_TOML_ROOT}/include" "${ERLANG_AOT_TOML_ROOT}" NO_DEFAULT_PATH)
else()
    find_path(ERLANG_AOT_TOML_INCLUDE toml++/toml.hpp
        PATHS "${PROJECT_SOURCE_DIR}/build/deps/tomlplusplus-3.4.0/include")
endif()
if(NOT ERLANG_AOT_TOML_INCLUDE)
    message(FATAL_ERROR "toml++ 3.4.0 is required. Set ERLANG_AOT_TOML_ROOT to its source/install root; see docs/projects.md. Configuration never downloads dependencies.")
endif()
file(READ "${ERLANG_AOT_TOML_INCLUDE}/toml++/impl/version.hpp" toml_version)
foreach(part IN ITEMS MAJOR MINOR PATCH)
    string(REGEX MATCH "#define TOML_LIB_${part} ([0-9]+)" matched "${toml_version}")
    set(toml_${part} "${CMAKE_MATCH_1}")
endforeach()
if(NOT "${toml_MAJOR}.${toml_MINOR}.${toml_PATCH}" STREQUAL "3.4.0")
    message(FATAL_ERROR "Expected tested toml++ 3.4.0 under ${ERLANG_AOT_TOML_INCLUDE}")
endif()
add_library(erlang_project_toml INTERFACE)
target_include_directories(erlang_project_toml SYSTEM INTERFACE "${ERLANG_AOT_TOML_INCLUDE}")
target_compile_definitions(erlang_project_toml INTERFACE
    TOML_HEADER_ONLY=1 TOML_EXCEPTIONS=1 TOML_ENABLE_UNRELEASED_FEATURES=0)
