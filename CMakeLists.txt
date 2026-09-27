cmake_minimum_required(VERSION 3.28)
include(cmake/WindowsToolchain.cmake)
project(ErlangAoT VERSION 0.1.0 LANGUAGES CXX)
# Require portable C++23 for every target created in this project and its subdirectories.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(cmake/WindowsHost.cmake)

option(ERLANG_AOT_BUILD_COMPILER "Build the erlangaot compiler tool" ON)
option(ERLANG_AOT_BUILD_RUNTIME "Build the runtime library" ON)

if(WIN32)
    message(STATUS "Windows development build; remaining platform work is tracked in abi/plan-windows.md.")
elseif(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(WARNING "Linux validation is pending. See README.md for platform status.")
endif()

include(cmake/ProjectOptions.cmake)
if(ERLANG_AOT_BUILD_COMPILER OR ERLANG_AOT_BUILD_RUNTIME)
    include(cmake/BoostDependencies.cmake)
    # Uncompiled headers may use any project target as their IDE analysis context.
    include_directories(SYSTEM ${ERLANG_AOT_BOOST_SYSTEM_INCLUDES})
endif()
if(ERLANG_AOT_BUILD_COMPILER)
    # Select the SDK ABI before creating compiler, runtime or test targets.
    include(cmake/LLVMDependencies.cmake)
endif()
include(CTest)
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(ERLANG_AOT_MSVC_INCLUDES "$ENV{INCLUDE}")
endif()
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
if(BUILD_TESTING AND ERLANG_AOT_BUILD_RUNTIME AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(tests/runtime)
endif()

if(BUILD_TESTING AND NOT CMAKE_CROSSCOMPILING)
    add_subdirectory(tests/abi)
endif()
# Compiler dependency discovery may enable C; capture the complete host toolchain for child builds.
configure_file(cmake/TestHost.cmake.in TestHost.cmake @ONLY)
