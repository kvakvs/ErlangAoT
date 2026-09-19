cmake_minimum_required(VERSION 3.28)
project(ErlangAoT VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ERLANG_AOT_BUILD_COMPILER "Build the erlangaot compiler tool" ON)
option(ERLANG_AOT_BUILD_RUNTIME "Build the runtime library" ON)

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(WARNING "Only macOS is currently validated. See .agents/plan-windows.md for Windows work.")
endif()

include(cmake/ProjectOptions.cmake)
include(CTest)
configure_file(cmake/QualityToolchain.cmake.in QualityToolchain.cmake @ONLY)
# Keep source quality checks explicit and independent of normal compilation.
add_custom_target(check-complexity
    COMMAND "${CMAKE_COMMAND}" -P "${PROJECT_SOURCE_DIR}/cmake/CheckComplexity.cmake"
    COMMENT "Checking C++ cyclomatic complexity with Lizard"
    VERBATIM
)
add_custom_target(check-clang-tidy
    COMMAND "${CMAKE_COMMAND}" "-DQUALITY_BUILD_DIR=${PROJECT_BINARY_DIR}"
        -P "${PROJECT_SOURCE_DIR}/cmake/CheckClangTidy.cmake"
    COMMENT "Checking C++ diagnostics and cognitive complexity with clang-tidy"
    VERBATIM
)
add_custom_target(check-quality)
if(ERLANG_AOT_BUILD_COMPILER AND ERLANG_AOT_BUILD_RUNTIME)
    add_dependencies(check-quality check-complexity check-clang-tidy)
else()
    add_custom_command(TARGET check-quality POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E echo "check-quality requires both compiler and runtime enabled."
        COMMAND "${CMAKE_COMMAND}" -E false
        VERBATIM
    )
endif()
add_subdirectory(abi)
if(ERLANG_AOT_BUILD_COMPILER)
    add_subdirectory(compiler)
endif()
if(ERLANG_AOT_BUILD_RUNTIME)
    add_subdirectory(runtime)
endif()
if(BUILD_TESTING AND ERLANG_AOT_BUILD_COMPILER AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(tests)
endif()
